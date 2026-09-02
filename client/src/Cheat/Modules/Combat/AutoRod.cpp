#include "pch.h"
#include "AutoRod.h"
#include "Throw.h"

#include "../Misc/Overlay.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"
#include "AntiBot.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Classes/Timer.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../../../Game/Klass.h"
#include "../../../Helper/Utils.h"

#include "../../../../vendors/imgui/imgui.h"
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <algorithm>

enum class RodState { Idle, Aim, Throw, SwapBack };

static jclass s_playerCls = nullptr;
static jclass s_livingCls = nullptr;
static jclass s_c05Cls = nullptr;
static jmethodID s_c05Ctor = nullptr;
static jmethodID s_addQueue = nullptr;
static jfieldID s_sendQueue = nullptr;
static bool s_tried = false;

static RodState s_state = RodState::Idle;
static long long s_lastThrow = 0;
static long long s_stateAt = 0;
static int s_targetId = -1;
static float s_dYaw = 0.f, s_dPitch = 0.f;
static float s_savedYaw = 0.f, s_savedPitch = 0.f;
static int s_rodSlot = -1, s_origSlot = -1;
static Vec3 s_espFeet{}, s_espHead{};
static bool s_espOk = false;

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
    s_playerCls = GlobalClass(env, "net/minecraft/entity/player/EntityPlayer");
    s_livingCls = GlobalClass(env, "net/minecraft/entity/EntityLivingBase");
    s_c05Cls = GlobalClass(env, "net/minecraft/network/play/client/C03PacketPlayer$C05PacketPlayerLook");

    std::string playerN = Mapper::Get("net/minecraft/client/entity/EntityClientPlayerMP");
    Klass* pk = playerN.empty() ? nullptr : g_Instance->FindClass(playerN.c_str());
    if (pk) {
        std::string sq = Mapper::Get("net/minecraft/client/network/NetHandlerPlayClient", 2);
        s_sendQueue = env->GetFieldID((jclass)pk, Mapper::Get("sendQueue").c_str(), sq.c_str());
        JniOk(env);
    }
    std::string nhN = Mapper::Get("net/minecraft/client/network/NetHandlerPlayClient");
    Klass* nh = nhN.empty() ? nullptr : g_Instance->FindClass(nhN.c_str());
    if (nh) {
        std::string sig = "(" + Mapper::Get("net/minecraft/network/Packet", 2) + ")V";
        s_addQueue = env->GetMethodID((jclass)nh, Mapper::Get("addToSendQueue").c_str(), sig.c_str());
        JniOk(env);
    }
    if (s_c05Cls) {
        s_c05Ctor = env->GetMethodID(s_c05Cls, "<init>", "(FFZ)V");
        JniOk(env);
    }
}

static float AngleTo180(float f) {
    f = fmodf(f, 360.f);
    if (f >= 180.f) f -= 360.f;
    if (f < -180.f) f += 360.f;
    return f;
}

static void Direction3D(double* to, double* from, double* out) {
    double dx = to[0] - from[0];
    double dy = to[1] - from[1];
    double dz = to[2] - from[2];
    out[0] = atan2(dz, dx) * 180.0 / 3.14159265359 - 90.0;
    double horiz = sqrt(dx * dx + dz * dz);
    out[1] = horiz > 1e-6 ? -atan(dy / horiz) * 180.0 / 3.14159265359 : 0.0;
}

static float RandF(float a, float b) {
    return a + (b - a) * ((float)rand() / (float)RAND_MAX);
}

static void SendLook(JNIEnv* env, jobject playerObj, float yaw, float pitch, bool onGround) {
    if (!AutoRodSettings::rotations) return;
    if (!s_c05Ctor || !s_c05Cls || !s_sendQueue || !s_addQueue) return;
    jobject queue = env->GetObjectField(playerObj, s_sendQueue);
    JniOk(env);
    if (!queue) return;
    jobject pkt = env->NewObject(s_c05Cls, s_c05Ctor, yaw, pitch, (jboolean)onGround);
    JniOk(env);
    if (pkt) {
        env->CallVoidMethod(queue, s_addQueue, pkt);
        JniOk(env);
        env->DeleteLocalRef(pkt);
    }
    env->DeleteLocalRef(queue);
}

static int FindRod(JNIEnv* env, InventoryPlayer* inv) {
    for (int i = 0; i < 9; i++) {
        jobject st = inv->GetStackInSlot(i, env);
        if (!st) continue;
        bool rod = ((ItemStack*)st)->IsRod(env);
        env->DeleteLocalRef(st);
        if (rod) return i;
    }
    return -1;
}

static bool ConsiderTarget(JNIEnv* env, Player* local, jobject e, bool isPlayer,
    int myId, double me[3], int& bestId, double& bestDist, float& bestYaw, float& bestPitch)
{
    auto* ent = (Player*)e;
    int id = ent->GetEntityId(env);
    if (id == myId) return false;
    if (ent->GetHealth(env) <= 0.f) return false;
    if (ent->IsInvisible(env)) return false;
    if (isPlayer) {
        if (FriendsSettings::IsFriend(env, ent)) return false;
        if (AutoRodSettings::targetEnemiesOnly && !EnemiesSettings::IsEnemy(env, ent)) return false;
        if (AntiBot_IsBot(env, e)) return false;
        if (AutoRodSettings::ignoreEating && ent->IsUsingItem(env)) return false;
    }

    Vec3D pos = ent->GetPos(env);
    float eye = ent->GetEyeHeight(env);
    JniOk(env);
    double tgt[3]{ pos.x, pos.y + eye * 0.5, pos.z };
    double dx = tgt[0] - me[0], dy = tgt[1] - me[1], dz = tgt[2] - me[2];
    double dist = sqrt(dx * dx + dy * dy + dz * dz);
    if (dist > AutoRodSettings::maxRange) return false;

    if (AutoRodSettings::moveFix) {
        Vec3D last = ent->GetLastTickPos(env);
        double vx = pos.x - last.x, vy = pos.y - last.y, vz = pos.z - last.z;
        double ticks = dist / 1.0;
        if (ticks > 40.0) ticks = 40.0;
        tgt[0] = pos.x + vx * ticks;
        tgt[1] = pos.y + eye * 0.5 + vy * ticks + 0.5 * 0.03 * ticks * ticks;
        tgt[2] = pos.z + vz * ticks;
        dx = tgt[0] - me[0]; dy = tgt[1] - me[1]; dz = tgt[2] - me[2];
        dist = sqrt(dx * dx + dy * dy + dz * dz);
    }

    double dir[2];
    Direction3D(tgt, me, dir);
    float yaw = AngleTo180((float)fmod(dir[0], 360.0) - local->GetRotationYaw(env));
    float pitch = AngleTo180((float)dir[1] - local->GetRotationPitch(env));
    float ang = sqrtf(yaw * yaw + pitch * pitch);
    if (fabsf(yaw) > AutoRodSettings::fov) return false;
    if (ang > AutoRodSettings::maxLookFov) return false;
    if (AutoRodSettings::onlyIfNotInReach && dist <= AutoRodSettings::meleeReach) return false;

    if (dist < bestDist) {
        bestDist = dist;
        bestId = id;
        bestYaw = yaw;
        bestPitch = pitch;
        return true;
    }
    return false;
}

static bool FindTarget(JNIEnv* env, Player* local, jobject worldObj, int& outId, float& outYaw, float& outPitch) {
    Vec3D lp = local->GetPos(env);
    double me[3]{ lp.x, lp.y + local->GetEyeHeight(env), lp.z };
    int myId = local->GetEntityId(env);
    int bestId = -1;
    double bestDist = 1e18;
    float bestYaw = 0.f, bestPitch = 0.f;
    auto* world = (World*)worldObj;

    if (AutoRodSettings::targetPlayers) {
        auto players = world->GetPlayerEntities(env);
        for (auto* p : players) {
            if (!p) continue;
            ConsiderTarget(env, local, (jobject)p, true, myId, me, bestId, bestDist, bestYaw, bestPitch);
            env->DeleteLocalRef((jobject)p);
        }
    }
    if (AutoRodSettings::targetMobs && s_livingCls) {
        auto ents = world->GetLoadedEntities(env);
        for (jobject e : ents) {
            if (!e) continue;
            if (s_playerCls && env->IsInstanceOf(e, s_playerCls)) { env->DeleteLocalRef(e); continue; }
            if (!env->IsInstanceOf(e, s_livingCls)) { env->DeleteLocalRef(e); continue; }
            ConsiderTarget(env, local, e, false, myId, me, bestId, bestDist, bestYaw, bestPitch);
            env->DeleteLocalRef(e);
        }
    }

    if (bestId == -1) return false;
    outId = bestId; outYaw = bestYaw; outPitch = bestPitch;
    return true;
}

static void SnapshotEsp(JNIEnv* env, jobject worldObj, int id) {
    s_espOk = false;
    if (id < 0 || !worldObj) return;
    auto ents = ((World*)worldObj)->GetLoadedEntities(env);
    for (jobject e : ents) {
        if (!e) continue;
        auto* p = (Player*)e;
        if (p->GetEntityId(env) == id) {
            Vec3D pos = p->GetPos(env);
            Vec3D last = p->GetLastTickPos(env);
            jobject t = Minecraft::GetTimer(env);
            float pt = t ? ((Timer*)t)->GetRenderPartialTicks(env) : 1.f;
            if (t) env->DeleteLocalRef(t);
            double x = last.x + (pos.x - last.x) * pt;
            double y = last.y + (pos.y - last.y) * pt;
            double z = last.z + (pos.z - last.z) * pt;
            s_espFeet = { (float)x, (float)y, (float)z };
            s_espHead = { (float)x, (float)y + 1.8f, (float)z };
            s_espOk = true;
        }
        env->DeleteLocalRef(e);
    }
}

void AutoRod::Run(JNIEnv* env) {
    if (!enabled) {
        s_state = RodState::Idle;
        s_targetId = -1;
        s_espOk = false;
        return;
    }
    if (Overlay::isOpen || Throw_IsBusy()) return;
    Ensure(env);

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    if (!playerObj || !worldObj) return;
    auto* local = (Player*)playerObj;
    long long now = NowMs();

    if (s_state != RodState::Idle && now - s_stateAt < 50) {
        SnapshotEsp(env, worldObj, s_targetId);
        return;
    }

    jobject invObj = local->GetInventoryPlayer(env);
    auto* inv = (InventoryPlayer*)invObj;

    switch (s_state) {
    case RodState::SwapBack:
        if (inv && s_origSlot >= 0) inv->SetSlot(s_origSlot, env);
        SendLook(env, playerObj, s_savedYaw, s_savedPitch, local->IsOnGround(env));
        s_lastThrow = now;
        s_state = RodState::Idle;
        s_targetId = -1;
        if (invObj) env->DeleteLocalRef(invObj);
        return;

    case RodState::Throw: {
        float cy = local->GetRotationYaw(env), cp = local->GetRotationPitch(env);
        float py = local->GetPrevRotationYaw(env), pp = local->GetPrevRotationPitch(env);
        float ay = s_savedYaw + s_dYaw;
        float ap = s_savedPitch + s_dPitch;
        if (AutoRodSettings::rotations) {
            local->SetRotationYaw(ay, env);
            local->SetRotationPitch(ap, env);
            local->SetPrevRotationYaw(ay, env);
            local->SetPrevRotationPitch(ap, env);
        }
        Minecraft::RightClickMouse(env);
        if (AutoRodSettings::rotations) {
            local->SetRotationYaw(cy, env);
            local->SetRotationPitch(cp, env);
            local->SetPrevRotationYaw(py, env);
            local->SetPrevRotationPitch(pp, env);
        }
        s_state = RodState::SwapBack;
        s_stateAt = now;
        if (invObj) env->DeleteLocalRef(invObj);
        return;
    }

    case RodState::Aim:
        if (inv) {
            s_origSlot = inv->GetSlot(env);
            if (AutoRodSettings::click && s_rodSlot != s_origSlot)
                inv->SetSlot(s_rodSlot, env);
        }
        s_state = RodState::Throw;
        s_stateAt = now;
        if (invObj) env->DeleteLocalRef(invObj);
        return;

    default:
        break;
    }

    if (now - s_lastThrow < AutoRodSettings::cooldownMs) {
        s_targetId = -1;
        s_espOk = false;
        if (invObj) env->DeleteLocalRef(invObj);
        return;
    }
    if (local->IsSwingInProgress(env)) {
        if (invObj) env->DeleteLocalRef(invObj);
        return;
    }

    int tid = -1; float ty = 0.f, tp = 0.f;
    if (!FindTarget(env, local, worldObj, tid, ty, tp)) {
        s_targetId = -1;
        s_espOk = false;
        if (invObj) env->DeleteLocalRef(invObj);
        return;
    }
    if (!inv) { if (invObj) env->DeleteLocalRef(invObj); return; }
    int rod = FindRod(env, inv);
    if (rod < 0) { env->DeleteLocalRef(invObj); return; }

    s_targetId = tid;
    s_dYaw = ty;
    s_dPitch = tp;
    s_savedYaw = local->GetRotationYaw(env);
    s_savedPitch = local->GetRotationPitch(env);
    if (AutoRodSettings::luckyThrow) {
        s_dYaw += (RandF(0.f, 1.f) - 0.5f) * 4.f;
        s_dPitch += (RandF(0.f, 1.f) - 0.5f) * 4.f;
    }
    SendLook(env, playerObj, s_savedYaw + s_dYaw, s_savedPitch + s_dPitch, local->IsOnGround(env));
    s_rodSlot = rod;
    s_state = RodState::Aim;
    s_stateAt = now;
    SnapshotEsp(env, worldObj, s_targetId);
    env->DeleteLocalRef(invObj);
}

void AutoRod::OnRender(JNIEnv* env) {
    Run(env);
}

void AutoRod::OnImGuiRender(JNIEnv* env) {
    if (!env || !enabled || !AutoRodSettings::esp || !s_espOk || s_targetId < 0) return;
    auto mv = ActiveRenderInfo::GetModelView(env);
    auto proj = ActiveRenderInfo::GetProjection(env);
    if (mv.empty() || proj.empty()) return;
    ImGuiIO& io = ImGui::GetIO();
    int w = (int)io.DisplaySize.x, h = (int)io.DisplaySize.y;
    Vec2 f{}, hd{};
    if (!WorldToScreen(s_espFeet, f, mv, proj, w, h)) return;
    if (!WorldToScreen(s_espHead, hd, mv, proj, w, h)) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    float x = (std::min)(f.x, hd.x) - 12.f;
    float y = (std::min)(f.y, hd.y);
    float x2 = (std::max)(f.x, hd.x) + 12.f;
    float y2 = (std::max)(f.y, hd.y);
    ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
        AutoRodSettings::espColor[0], AutoRodSettings::espColor[1],
        AutoRodSettings::espColor[2], AutoRodSettings::espColor[3]));
    dl->AddRect(ImVec2(x, y), ImVec2(x2, y2), col, 0.f, 0, 2.f);
}
