#include "pch.h"
#include "velocity.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../Misc/Overlay.h"
#include <chrono>

static int  g_lastHrt = 0;
static bool g_pendingJumpDelay = false;
static bool g_pendingJumpReset = false;
static long long g_jumpDelayStart = 0;
static long long g_jumpPressedAt = 0;

static long long NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static bool ChanceOk() {
    if (VelocitySettings::chance >= 100) return true;
    return (rand() % 101) <= VelocitySettings::chance;
}

static bool MovingForward(JNIEnv* env) {
    jobject gs = Minecraft::GetGameSettings(env);
    if (!gs) return false;
    jobject fwd = ((GameSettings*)gs)->GetKeyBindForward(env);
    env->DeleteLocalRef(gs);
    if (!fwd) return false;
    bool p = ((KeyBinding*)fwd)->IsPressed(env);
    env->DeleteLocalRef(fwd);
    return p;
}

static void SetJump(JNIEnv* env, bool pressed) {
    jobject gs = Minecraft::GetGameSettings(env);
    if (!gs) return;
    jobject jmp = ((GameSettings*)gs)->GetKeyBindJump(env);
    env->DeleteLocalRef(gs);
    if (!jmp) return;
    ((KeyBinding*)jmp)->SetPressed(pressed, env);
    env->DeleteLocalRef(jmp);
}

void Velocity::Run(JNIEnv* env) {
    if (!enabled) {
        g_pendingJumpDelay = false;
        g_pendingJumpReset = false;
        Sleep(20);
        return;
    }
    if (Overlay::isOpen) { Sleep(20); return; }

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) { Sleep(5); return; }
    auto* player = (Player*)playerObj;

    const long long now = NowMs();

    if (VelocitySettings::mode == 2) {
        if (g_pendingJumpDelay && !g_pendingJumpReset) {
            if (now - g_jumpDelayStart >= VelocitySettings::jumpDelayMs) {
                SetJump(env, true);
                g_jumpPressedAt = now;
                g_pendingJumpReset = true;
                g_pendingJumpDelay = false;
            }
        }
        if (g_pendingJumpReset && now - g_jumpPressedAt >= 50) {
            SetJump(env, false);
            g_pendingJumpReset = false;
        }
    }

    int hrt = player->GetHurtResistantTime(env);
    int maxHrt = player->GetMaxHurtResistantTime(env);
    bool newHit = (maxHrt > 0 && hrt >= maxHrt - 1 && g_lastHrt < maxHrt - 1);
    g_lastHrt = hrt;

    if (!newHit) {
        Sleep(5);
        return;
    }

    if (VelocitySettings::weaponsOnly) {
        jobject held = player->GetHeldItem(env);
        bool ok = false;
        if (held) {
            ok = ((ItemStack*)held)->IsWeapon(env);
            env->DeleteLocalRef(held);
        }
        if (!ok) { Sleep(5); return; }
    }

    if (VelocitySettings::onlyLookingAtPlayer) {
        jobject pointed = Minecraft::GetPointedEntity(env);
        if (!pointed) { Sleep(5); return; }
        env->DeleteLocalRef(pointed);
    }

    if (VelocitySettings::onlyMousePressed && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        Sleep(5);
        return;
    }

    bool forward = MovingForward(env);
    bool sprinting = player->IsSprinting(env);
    bool onGround = player->IsOnGround(env);

    if (VelocitySettings::mode == 2) {
        if (!onGround) { Sleep(5); return; }
        if (!ChanceOk()) { Sleep(5); return; }
        if (VelocitySettings::onlyWhenMovingForward && !forward) { Sleep(5); return; }
        g_pendingJumpDelay = true;
        g_jumpDelayStart = now;
        Sleep(5);
        return;
    }

    if (VelocitySettings::onlyWhenMovingForward && !forward) { Sleep(5); return; }
    if (!ChanceOk()) { Sleep(5); return; }

    double mx = player->GetMotionX(env);
    double my = player->GetMotionY(env);
    double mz = player->GetMotionZ(env);

    if (VelocitySettings::mode == 3) {
        bool shouldReduce = forward && sprinting;
        if (VelocitySettings::agcBypass && onGround) {
            int ht = player->GetHurtTime(env);
            shouldReduce = shouldReduce && (ht >= 1 && ht <= 4);
        }
        if (shouldReduce) {
            const float mult = 1.f - (VelocitySettings::reduceH / 100.f);
            player->SetMotionX(mx * (double)mult, env);
            player->SetMotionZ(mz * (double)mult, env);
        }
        Sleep(5);
        return;
    }

    if (VelocitySettings::mode == 1) {
        const float rev = VelocitySettings::reverseStrength / 100.f;
        player->SetMotionX(-(mx * (double)rev), env);
        player->SetMotionZ(-(mz * (double)rev), env);
    } else {
        const float hx = VelocitySettings::horizontal / 100.f;
        const float vy = VelocitySettings::vertical / 100.f;
        player->SetMotionX(mx * (double)hx, env);
        player->SetMotionY(my * (double)vy, env);
        player->SetMotionZ(mz * (double)hx, env);
    }
    Sleep(5);
}
