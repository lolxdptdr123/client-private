#include "pch.h"
#include "SnapTap.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"

void SnapTap::Run(JNIEnv* env) {
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

    if (!enabled || Overlay::isOpen) {
        prevLeft = prevRight = prevForward = prevBack = false;
        suppLeft = suppRight = suppForward = suppBack = false;
        Sleep(20);
        return;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) {
        Sleep(5);
        return;
    }
    auto* player = (Player*)playerObj;

    if (SnapTapSettings::onlyOnGround && !player->IsOnGround(env)) {
        env->DeleteLocalRef(playerObj);
        Sleep(1);
        return;
    }
    if (SnapTapSettings::disableOnSneak && player->IsSneaking(env)) {
        env->DeleteLocalRef(playerObj);
        Sleep(1);
        return;
    }
    env->DeleteLocalRef(playerObj);

    jobject gsObj = Minecraft::GetGameSettings(env);
    if (!gsObj) {
        Sleep(5);
        return;
    }
    auto* gs = (GameSettings*)gsObj;

    jobject kL = gs->GetKeyBindLeft(env);
    jobject kR = gs->GetKeyBindRight(env);
    jobject kF = gs->GetKeyBindForward(env);
    jobject kB = gs->GetKeyBindBack(env);

    auto drop = [&]() {
        if (kL) env->DeleteLocalRef(kL);
        if (kR) env->DeleteLocalRef(kR);
        if (kF) env->DeleteLocalRef(kF);
        if (kB) env->DeleteLocalRef(kB);
        env->DeleteLocalRef(gsObj);
    };

    if (!kL || !kR || !kF || !kB) {
        drop();
        Sleep(5);
        return;
    }

    auto* left    = (KeyBinding*)kL;
    auto* right   = (KeyBinding*)kR;
    auto* forward = (KeyBinding*)kF;
    auto* back    = (KeyBinding*)kB;

    bool physLeft    = left->IsPhysDown(env);
    bool physRight   = right->IsPhysDown(env);
    bool physForward = forward->IsPhysDown(env);
    bool physBack    = back->IsPhysDown(env);

    if (SnapTapSettings::axis != 2 && physLeft && physRight) {
        bool newLeft  = physLeft  && !prevLeft;
        bool newRight = physRight && !prevRight;

        if (newRight && !newLeft) {
            left->SetPressed(false, env);  suppLeft = true;
            if (suppRight) { right->SetPressed(true, env); suppRight = false; }
        } else if (newLeft && !newRight) {
            right->SetPressed(false, env); suppRight = true;
            if (suppLeft)  { left->SetPressed(true, env);  suppLeft  = false; }
        }
    } else if (physLeft) {
        if (suppLeft) { left->SetPressed(true, env); suppLeft = false; }
        suppRight = false;
    } else if (physRight) {
        if (suppRight) { right->SetPressed(true, env); suppRight = false; }
        suppLeft = false;
    } else {
        suppLeft = suppRight = false;
    }

    if (SnapTapSettings::axis != 1 && physForward && physBack) {
        bool newForward = physForward && !prevForward;
        bool newBack    = physBack    && !prevBack;

        if (newBack && !newForward) {
            forward->SetPressed(false, env); suppForward = true;
            if (suppBack) { back->SetPressed(true, env); suppBack = false; }
        } else if (newForward && !newBack) {
            back->SetPressed(false, env); suppBack = true;
            if (suppForward) { forward->SetPressed(true, env); suppForward = false; }
        }
    } else if (physForward) {
        if (suppForward) { forward->SetPressed(true, env); suppForward = false; }
        suppBack = false;
    } else if (physBack) {
        if (suppBack) { back->SetPressed(true, env); suppBack = false; }
        suppForward = false;
    } else {
        suppForward = suppBack = false;
    }

    prevLeft    = physLeft;
    prevRight   = physRight;
    prevForward = physForward;
    prevBack    = physBack;

    drop();
    Sleep(1);
}
