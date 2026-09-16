#include "pch.h"
#include "NoSlow.h"

#include "Overlay.h"
#include "../Combat/SwordCheck.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"

#include <algorithm>
#include <cmath>

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

enum class NsKind { None, Sword, Bow, Consume };

static NsKind HeldKind(JNIEnv* env, Player* local) {
    jobject st = local->GetHeldItem(env);
    JniOk(env);
    if (!st) return NsKind::None;
    auto* stack = (ItemStack*)st;

    int id = stack->GetItemId(env);
    JniOk(env);

    NsKind k = NsKind::Consume;
    if (stack->IsBlock(env)) {
        JniOk(env);
        k = NsKind::None;
    } else if (SC_IsHoldingSword(env) || id == 267 || id == 268 || id == 272 || id == 276 || id == 283
        || id == 258 || id == 271 || id == 275 || id == 279 || id == 286) {
        k = NsKind::Sword;
    } else if (id == 261) {
        k = NsKind::Bow;
    } else {
        std::string bow = Mapper::Get("net/minecraft/item/ItemBow");
        if (!bow.empty() && stack->Is(bow.c_str(), env))
            k = NsKind::Bow;
        JniOk(env);
    }

    env->DeleteLocalRef(st);
    return k;
}

static int PctFor(NsKind k) {
    if (k == NsKind::Sword) return NoSlowSettings::swords;
    if (k == NsKind::Bow) return NoSlowSettings::bows;
    if (k == NsKind::Consume) return NoSlowSettings::consumables;
    return 20;
}

static int ItemInUseCount(JNIEnv* env, Player* local) {
    jclass c = env->GetObjectClass((jobject)local);
    JniOk(env);
    if (!c) return 0;
    std::string n = Mapper::Get("itemInUseCount");
    if (n.empty()) n = "itemInUseCount";
    jfieldID f = env->GetFieldID(c, n.c_str(), "I");
    JniOk(env);
    int v = 0;
    if (f) v = env->GetIntField((jobject)local, f);
    JniOk(env);
    env->DeleteLocalRef(c);
    return v;
}

static bool KeyDown(JNIEnv* env, jobject kb) {
    if (!kb) return false;
    bool d = ((KeyBinding*)kb)->IsPhysDown(env);
    JniOk(env);
    env->DeleteLocalRef(kb);
    return d;
}

static void Axis(JNIEnv* env, float& fwd, float& str) {
    fwd = 0.f;
    str = 0.f;
    jobject gs = Minecraft::GetGameSettings(env);
    JniOk(env);
    if (gs) {
        auto* g = (GameSettings*)gs;
        if (KeyDown(env, g->GetKeyBindForward(env))) fwd += 1.f;
        if (KeyDown(env, g->GetKeyBindBack(env)))    fwd -= 1.f;
        if (KeyDown(env, g->GetKeyBindLeft(env)))    str += 1.f;
        if (KeyDown(env, g->GetKeyBindRight(env)))   str -= 1.f;
        env->DeleteLocalRef(gs);
    }
    if (fabsf(fwd) < 1e-3f && fabsf(str) < 1e-3f) {
        if ((GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState('Z') & 0x8000)) fwd += 1.f;
        if (GetAsyncKeyState('S') & 0x8000) fwd -= 1.f;
        if ((GetAsyncKeyState('A') & 0x8000) || (GetAsyncKeyState('Q') & 0x8000)) str += 1.f;
        if (GetAsyncKeyState('D') & 0x8000) str -= 1.f;
    }
}

static void SetInputF(JNIEnv* env, jobject obj, const char* key, float v) {
    if (!obj) return;
    const auto cls = (Klass*)env->GetObjectClass(obj);
    if (!cls) return;
    if (Field* f = cls->GetField(env, Mapper::Get(key).data(), "F"))
        f->SetFloatField(env, obj, v);
    JniOk(env);
    env->DeleteLocalRef((jclass)cls);
}

static void MoveFlying(JNIEnv* env, Player* local, float strafe, float forward, float friction) {
    jclass c = env->GetObjectClass((jobject)local);
    JniOk(env);
    if (!c) return;
    jmethodID m = env->GetMethodID(c, "moveFlying", "(FFF)V");
    JniOk(env);
    if (m)
        env->CallVoidMethod((jobject)local, m, strafe, forward, friction);
    JniOk(env);
    env->DeleteLocalRef(c);
}

static int s_lastTick = -1;

static void Tick(JNIEnv* env) {
    if (!env || Overlay::isOpen) return;

    jobject localObj = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!localObj) return;
    auto* local = (Player*)localObj;

    NsKind kind = HeldKind(env, local);
    bool inUse = local->IsUsingItem(env);
    JniOk(env);
    if (!inUse)
        inUse = ItemInUseCount(env, local) > 0;
    if (!inUse && kind != NsKind::None && (GetAsyncKeyState(VK_RBUTTON) & 0x8000))
        inUse = true;
    if (!inUse || kind == NsKind::None) {
        env->DeleteLocalRef(localObj);
        return;
    }

    int pct = (std::max)(20, (std::min)(100, PctFor(kind)));
    float k = (float)pct / 100.f;

    float fwd = 0.f, str = 0.f;
    Axis(env, fwd, str);
    float mag = sqrtf(fwd * fwd + str * str);
    if (mag > 1.f) {
        fwd /= mag;
        str /= mag;
    }

    const float wantF = fwd * k;
    const float wantS = str * k;
    const float preF = fwd * (k / 0.2f);
    const float preS = str * (k / 0.2f);

    local->SetMoveForward(wantF, env);
    local->SetMoveStrafing(wantS, env);
    JniOk(env);

    jobject input = local->GetMovementInput(env);
    JniOk(env);
    if (input) {
        SetInputF(env, input, "moveForward", preF);
        SetInputF(env, input, "moveStrafe", preS);
        SetInputF(env, input, "moveStrafing", preS);
        env->DeleteLocalRef(input);
    }

    if (pct <= 20) {
        env->DeleteLocalRef(localObj);
        return;
    }

    bool sprint = local->IsSprinting(env);
    JniOk(env);
    bool ground = local->IsOnGround(env);
    JniOk(env);
    float cap = ((sprint && fwd > 0.f) ? 0.2806f : 0.221f) * k;
    if (!ground)
        cap = ((std::min)(cap, 0.26f * k));

    double mx = local->GetMotionX(env);
    double mz = local->GetMotionZ(env);
    JniOk(env);
    double spd = sqrt(mx * mx + mz * mz);

    if (spd > 0.008) {
        double scale = (double)cap / spd;
        if (scale > 1.02 && scale < 7.0) {
            local->SetMotionX(mx * scale, env);
            local->SetMotionZ(mz * scale, env);
            JniOk(env);
        }
    } else if (mag > 0.1f) {
        float yaw = local->GetRotationYaw(env) * 0.017453292f;
        JniOk(env);
        float s = sinf(yaw);
        float c = cosf(yaw);
        local->SetMotionX((double)((str * c - fwd * s) * cap), env);
        local->SetMotionZ((double)((fwd * c + str * s) * cap), env);
        JniOk(env);
    }

    int tick = local->GetTicksExisted(env);
    JniOk(env);
    if (tick != s_lastTick && mag > 0.1f) {
        s_lastTick = tick;
        float extra = 0.1f * ((k / 0.2f) - 1.f);
        if (extra > 0.f)
            MoveFlying(env, local, str, fwd, extra);
    }

    env->DeleteLocalRef(localObj);
}

void NoSlow::Run(JNIEnv* env) {
    if (!enabled) return;
    Tick(env);
}

void NoSlow::OnRender(JNIEnv* env) {
    if (!enabled || !env) return;
    Tick(env);
}
