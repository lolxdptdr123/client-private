#include "pch.h"
#include "Strafe.h"

#include "Overlay.h"
#include "../Combat/SwordCheck.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Cheat/Hack.h"

#include <algorithm>
#include <cmath>

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static bool KeyDown(JNIEnv* env, jobject kb) {
    if (!kb) return false;
    bool d = ((KeyBinding*)kb)->IsPhysDown(env);
    JniOk(env);
    env->DeleteLocalRef(kb);
    return d;
}

static void Axis(JNIEnv* env, float& fwd, float& str, bool& jump) {
    fwd = 0.f;
    str = 0.f;
    jump = false;
    jobject gs = Minecraft::GetGameSettings(env);
    JniOk(env);
    if (gs) {
        auto* g = (GameSettings*)gs;
        if (KeyDown(env, g->GetKeyBindForward(env))) fwd += 1.f;
        if (KeyDown(env, g->GetKeyBindBack(env)))    fwd -= 1.f;
        if (KeyDown(env, g->GetKeyBindLeft(env)))    str += 1.f;
        if (KeyDown(env, g->GetKeyBindRight(env)))   str -= 1.f;
        jump = KeyDown(env, g->GetKeyBindJump(env));
        env->DeleteLocalRef(gs);
    }
    if (fabsf(fwd) < 1e-3f && fabsf(str) < 1e-3f) {
        if ((GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState('Z') & 0x8000)) fwd += 1.f;
        if (GetAsyncKeyState('S') & 0x8000) fwd -= 1.f;
        if ((GetAsyncKeyState('A') & 0x8000) || (GetAsyncKeyState('Q') & 0x8000)) str += 1.f;
        if (GetAsyncKeyState('D') & 0x8000) str -= 1.f;
    }
    if (!jump)
        jump = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
}

static bool s_wasGround = true;
static int  s_jumpLeft = 0;
static int  s_lastTick = -1;

static void Tick(JNIEnv* env) {
    if (!env || Overlay::isOpen) return;

    jobject localObj = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!localObj) return;
    auto* local = (Player*)localObj;

    int tick = local->GetTicksExisted(env);
    JniOk(env);
    if (tick == s_lastTick) {
        env->DeleteLocalRef(localObj);
        return;
    }
    s_lastTick = tick;

    if (StrafeSettings::holdingWeapon && !SC_IsHoldingSword(env)) {
        env->DeleteLocalRef(localObj);
        return;
    }

    int ht = local->GetHurtTime(env);
    JniOk(env);
    if (ht > StrafeSettings::maxHurtTime) {
        env->DeleteLocalRef(localObj);
        return;
    }

    bool ground = local->IsOnGround(env);
    JniOk(env);
    float fwd = 0.f, str = 0.f;
    bool jump = false;
    Axis(env, fwd, str, jump);

    double my = local->GetMotionY(env);
    JniOk(env);
    if (s_wasGround && !ground && (jump || my > 0.08))
        s_jumpLeft = 2;
    else if (s_jumpLeft > 0)
        s_jumpLeft--;
    s_wasGround = ground;

    int pct = ground ? StrafeSettings::onGround : StrafeSettings::inAir;
    if (s_jumpLeft > 0)
        pct = StrafeSettings::onJump;
    pct = (std::max)(0, (std::min)(100, pct));
    if (pct <= 0) {
        env->DeleteLocalRef(localObj);
        return;
    }

    float mag = sqrtf(fwd * fwd + str * str);
    if (mag < 0.01f) {
        env->DeleteLocalRef(localObj);
        return;
    }
    fwd /= mag;
    str /= mag;

    double mx = local->GetMotionX(env);
    double mz = local->GetMotionZ(env);
    JniOk(env);
    double spd = sqrt(mx * mx + mz * mz);
    if (spd < 0.02) {
        env->DeleteLocalRef(localObj);
        return;
    }

    float yaw = local->GetRotationYaw(env) * 0.017453292f;
    JniOk(env);
    float s = sinf(yaw);
    float c = cosf(yaw);
    double tx = (double)((str * c - fwd * s) * (float)spd);
    double tz = (double)((fwd * c + str * s) * (float)spd);

    float k = (float)pct / 100.f;
    local->SetMotionX(mx + (tx - mx) * (double)k, env);
    local->SetMotionZ(mz + (tz - mz) * (double)k, env);
    JniOk(env);
    env->DeleteLocalRef(localObj);
}

void Strafe::Run(JNIEnv* env) {
    if (!enabled) {
        s_wasGround = true;
        s_jumpLeft = 0;
        s_lastTick = -1;
        return;
    }
    Tick(env);
}

void Strafe::OnRender(JNIEnv* env) {
    if (!enabled || !env) return;
    Tick(env);
}
