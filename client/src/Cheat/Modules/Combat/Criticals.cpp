#include "pch.h"
#include "Criticals.h"

#include "../Misc/Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Timer.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Game/Method.h"

#include <chrono>
#include <cstdlib>

static constexpr int kBlindness = 15;

static jclass s_c04Cls = nullptr;
static jclass s_livingCls = nullptr;
static jmethodID s_c04Ctor = nullptr;
static jmethodID s_addQueue = nullptr;
static jmethodID s_getEye = nullptr;
static jfieldID s_sendQueue = nullptr;
static bool s_c04HasStance = true;
static bool s_tried = false;

static bool s_timerActive = false;
static bool s_wasHitAir = false;
static int s_lastHurt = 0;
static bool s_lagging = false;
static long long s_lagStart = 0;
static bool s_lmbPrev = false;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static long long NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static jclass GlobalClass(JNIEnv* env, const char* key) {
    std::string n = Mapper::Get(key);
    if (n.empty()) return nullptr;
    Klass* k = g_Instance->FindClass(n.c_str());
    if (!k) return nullptr;
    return (jclass)env->NewGlobalRef((jclass)k);
}

static void Ensure(JNIEnv* env) {
    if (s_tried || !env) return;
    s_tried = true;

    s_livingCls = GlobalClass(env, "net/minecraft/entity/EntityLivingBase");
    s_c04Cls = GlobalClass(env, "net/minecraft/network/play/client/C03PacketPlayer$C04PacketPlayerPosition");

    std::string playerN = Mapper::Get("net/minecraft/client/entity/EntityClientPlayerMP");
    Klass* pk = playerN.empty() ? nullptr : g_Instance->FindClass(playerN.c_str());
    if (pk) {
        std::string sq = Mapper::Get("net/minecraft/client/network/NetHandlerPlayClient", 2);
        s_sendQueue = env->GetFieldID((jclass)pk, Mapper::Get("sendQueue").c_str(), sq.c_str());
        JniOk(env);
        s_getEye = env->GetMethodID((jclass)pk, Mapper::Get("getEyeHeight").c_str(), "()F");
        JniOk(env);
    }

    std::string nhN = Mapper::Get("net/minecraft/client/network/NetHandlerPlayClient");
    Klass* nh = nhN.empty() ? nullptr : g_Instance->FindClass(nhN.c_str());
    if (nh) {
        std::string sig = "(" + Mapper::Get("net/minecraft/network/Packet", 2) + ")V";
        s_addQueue = env->GetMethodID((jclass)nh, Mapper::Get("addToSendQueue").c_str(), sig.c_str());
        JniOk(env);
    }

    if (s_c04Cls) {
        s_c04Ctor = env->GetMethodID(s_c04Cls, "<init>", "(DDDDZ)V");
        JniOk(env);
        if (!s_c04Ctor) {
            s_c04HasStance = false;
            s_c04Ctor = env->GetMethodID(s_c04Cls, "<init>", "(DDDZ)V");
            JniOk(env);
        }
    }
}

static void RestoreTimer(JNIEnv* env) {
    if (!s_timerActive) return;
    jobject t = Minecraft::GetTimer(env);
    if (t) {
        ((Timer*)t)->SetTicksPerSecond(20.f, env);
        env->DeleteLocalRef(t);
    }
    s_timerActive = false;
}

static bool CanCritEnv(JNIEnv* env, Player* p, bool needFall) {
    if (p->IsOnLadder(env)) return false;
    if (p->IsInWater(env)) return false;
    if (p->IsPotionActive(kBlindness, env)) return false;
    jobject ride = p->GetRidingEntity(env);
    if (ride) { env->DeleteLocalRef(ride); return false; }
    if (needFall) {
        if (p->IsOnGround(env)) return false;
        if (p->GetMotionY(env) >= 0.0) return false;
    }
    return true;
}

static bool AimingLiving(JNIEnv* env) {
    jobject ent = Minecraft::GetPointedEntity(env);
    if (!ent) return false;
    bool ok = !s_livingCls || env->IsInstanceOf(ent, s_livingCls);
    JniOk(env);
    env->DeleteLocalRef(ent);
    return ok;
}

static void SendPos(JNIEnv* env, jobject playerObj, double x, double y, double z, bool onGround) {
    if (!s_c04Ctor || !s_c04Cls || !s_sendQueue || !s_addQueue) return;
    jobject queue = env->GetObjectField(playerObj, s_sendQueue);
    JniOk(env);
    if (!queue) return;

    float eye = 1.62f;
    if (s_getEye) {
        eye = env->CallFloatMethod(playerObj, s_getEye);
        JniOk(env);
    }
    jobject pkt = nullptr;
    if (s_c04HasStance)
        pkt = env->NewObject(s_c04Cls, s_c04Ctor, x, y, y + (double)eye, z, (jboolean)onGround);
    else
        pkt = env->NewObject(s_c04Cls, s_c04Ctor, x, y, z, (jboolean)onGround);
    JniOk(env);
    if (pkt) {
        env->CallVoidMethod(queue, s_addQueue, pkt);
        JniOk(env);
        env->DeleteLocalRef(pkt);
    }
    env->DeleteLocalRef(queue);
}

static bool ChanceOk() {
    if (CriticalsSettings::chance >= 100) return true;
    return (rand() % 101) <= CriticalsSettings::chance;
}

static void TickPacket(JNIEnv* env, Player* player, jobject playerObj) {
    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const long long now = NowMs();
    const bool living = AimingLiving(env);
    const bool canFall = CanCritEnv(env, player, true);

    if (s_lagging) {
        bool timeout = s_lagStart > 0 && (now - s_lagStart) > CriticalsSettings::maxQueueTime;
        if (timeout || player->IsOnGround(env) || !lmb) {
            s_lagging = false;
            s_lagStart = 0;
        }
    }

    if (lmb && canFall && living) {
        if (!s_lagging) {
            if (!ChanceOk()) {
                s_lmbPrev = lmb;
                return;
            }
            s_lagging = true;
            s_lagStart = now;
        }
        player->SetOnGround(false, env);
        if (lmb && !s_lmbPrev) {
            Vec3D p = player->GetPos(env);
            SendPos(env, playerObj, p.x, p.y, p.z, false);
        }
    } else if (lmb && !s_lmbPrev && living && CanCritEnv(env, player, false) && player->IsOnGround(env)) {
        if (ChanceOk()) {
            Vec3D p = player->GetPos(env);
            SendPos(env, playerObj, p.x, p.y + 0.0625, p.z, false);
            SendPos(env, playerObj, p.x, p.y, p.z, false);
        }
    }

    s_lmbPrev = lmb;
}

static void TickTimer(JNIEnv* env, Player* player) {
    jobject tObj = Minecraft::GetTimer(env);
    if (!tObj) return;
    auto* timer = (Timer*)tObj;

    int ht = player->GetHurtTime(env);
    bool onGround = player->IsOnGround(env);
    double my = player->GetMotionY(env);

    if (ht > 0 && s_lastHurt == 0)
        s_wasHitAir = true;

    if (!s_timerActive) {
        if (s_wasHitAir && !onGround && my < 0.0 && CanCritEnv(env, player, false)) {
            if (ChanceOk()) {
                timer->SetTicksPerSecond(20.f * CriticalsSettings::timerSpeed, env);
                s_timerActive = true;
            }
        }
        if (s_wasHitAir && onGround && ht == 0)
            s_wasHitAir = false;
    } else if (onGround) {
        timer->SetTicksPerSecond(20.f, env);
        s_timerActive = false;
        s_wasHitAir = false;
    }

    s_lastHurt = ht;
    env->DeleteLocalRef(tObj);
}

void Criticals::Run(JNIEnv* env) {
    if (!enabled) {
        if (s_timerActive) RestoreTimer(env);
        s_lagging = false;
        s_wasHitAir = false;
        Sleep(20);
        return;
    }
    if (Overlay::isOpen) {
        if (s_timerActive) RestoreTimer(env);
        Sleep(20);
        return;
    }

    Ensure(env);
    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) { Sleep(5); return; }
    auto* player = (Player*)playerObj;

    if (CriticalsSettings::mode == 0)
        TickPacket(env, player, playerObj);
    else
        TickTimer(env, player);
}
