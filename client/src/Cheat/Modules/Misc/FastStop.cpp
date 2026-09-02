#include "pch.h"
#include "FastStop.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"

void FastStop::ResetHold() {
    prevForward = prevBack = prevLeft = prevRight = false;
    forwardHeld = backHeld = leftHeld = rightHeld = 0;
}

void FastStop::Run(JNIEnv* env) {
    if (!env) {
        Sleep(5);
        return;
    }

    DWORD now = GetTickCount();
    if (now - lastTick < 50) {
        Sleep(1);
        return;
    }
    lastTick = now;

    if (Overlay::isOpen) {
        ResetHold();
        Sleep(20);
        return;
    }

    if (!enabled && !csF && !csB && !csL && !csR) {
        ResetHold();
        Sleep(20);
        return;
    }

    jobject gsObj = Minecraft::GetGameSettings(env);
    if (!gsObj) {
        Sleep(5);
        return;
    }
    auto* gs = (GameSettings*)gsObj;

    jobject kF = gs->GetKeyBindForward(env);
    jobject kB = gs->GetKeyBindBack(env);
    jobject kL = gs->GetKeyBindLeft(env);
    jobject kR = gs->GetKeyBindRight(env);

    auto dropBinds = [&]() {
        if (kF) env->DeleteLocalRef(kF);
        if (kB) env->DeleteLocalRef(kB);
        if (kL) env->DeleteLocalRef(kL);
        if (kR) env->DeleteLocalRef(kR);
        env->DeleteLocalRef(gsObj);
    };

    if (!kF || !kB || !kL || !kR) {
        dropBinds();
        Sleep(5);
        return;
    }

    auto* f = (KeyBinding*)kF;
    auto* b = (KeyBinding*)kB;
    auto* l = (KeyBinding*)kL;
    auto* r = (KeyBinding*)kR;

    bool physF = f->IsPhysDown(env);
    bool physB = b->IsPhysDown(env);
    bool physL = l->IsPhysDown(env);
    bool physR = r->IsPhysDown(env);

    if (csF) { b->SetPressed(physB, env); csF = false; }
    if (csB) { f->SetPressed(physF, env); csB = false; }
    if (csL) { r->SetPressed(physR, env); csL = false; }
    if (csR) { l->SetPressed(physL, env); csR = false; }

    jobject playerObj = Minecraft::GetThePlayer(env);
    auto* player = (Player*)playerObj;
    bool canApply = enabled && player;
    if (canApply && FastStopSettings::disableOnSneak && player->IsSneaking(env))
        canApply = false;
    if (canApply && player->IsInWater(env))
        canApply = false;
    if (playerObj) env->DeleteLocalRef(playerObj);

    if (canApply) {
        constexpr int MIN_HOLD_TICKS = 2;

        if (FastStopSettings::axis != 1) {
            if (prevForward && !physF && !physB && forwardHeld >= MIN_HOLD_TICKS) {
                csF = true;
                b->SetPressed(true, env);
                f->SetPressed(false, env);
            } else if (prevBack && !physB && !physF && backHeld >= MIN_HOLD_TICKS) {
                csB = true;
                f->SetPressed(true, env);
                b->SetPressed(false, env);
            }
        }

        if (FastStopSettings::axis != 2) {
            if (prevLeft && !physL && !physR && leftHeld >= MIN_HOLD_TICKS) {
                csL = true;
                r->SetPressed(true, env);
                l->SetPressed(false, env);
            } else if (prevRight && !physR && !physL && rightHeld >= MIN_HOLD_TICKS) {
                csR = true;
                l->SetPressed(true, env);
                r->SetPressed(false, env);
            }
        }

        forwardHeld = physF ? forwardHeld + 1 : 0;
        backHeld    = physB ? backHeld + 1 : 0;
        leftHeld    = physL ? leftHeld + 1 : 0;
        rightHeld   = physR ? rightHeld + 1 : 0;

        prevForward = physF;
        prevBack    = physB;
        prevLeft    = physL;
        prevRight   = physR;
    } else {
        ResetHold();
    }

    dropBinds();
    Sleep(1);
}
