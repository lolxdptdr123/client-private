#include "pch.h"
#include "InvWalk.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Classes/GuiScreen.h"

static void ApplyBind(JNIEnv* env, jobject bind, bool down) {
    if (!bind) return;
    ((KeyBinding*)bind)->SetPressed(down, env);
    env->DeleteLocalRef(bind);
}

void InvWalk::Run(JNIEnv* env) {
    if (!enabled || !env || Overlay::isOpen) {
        Sleep(20);
        return;
    }

    jobject screenObj = Minecraft::GetCurrentScreen(env);
    if (!screenObj) {
        Sleep(5);
        return;
    }
    auto* screen = (GuiScreen*)screenObj;
    if (screen->IsChat(env)) {
        env->DeleteLocalRef(screenObj);
        Sleep(5);
        return;
    }
    env->DeleteLocalRef(screenObj);

    jobject gsObj = Minecraft::GetGameSettings(env);
    if (!gsObj) {
        Sleep(2);
        return;
    }
    auto* gs = (GameSettings*)gsObj;

    jobject forward = gs->GetKeyBindForward(env);
    jobject back    = gs->GetKeyBindBack(env);
    jobject left    = gs->GetKeyBindLeft(env);
    jobject right   = gs->GetKeyBindRight(env);
    jobject jump    = gs->GetKeyBindJump(env);
    jobject sneak   = gs->GetKeyBindSneak(env);
    jobject sprint  = gs->GetKeyBindSprint(env);

    if (forward) ApplyBind(env, forward, ((KeyBinding*)forward)->IsPhysDown(env));
    if (back)    ApplyBind(env, back,    ((KeyBinding*)back)->IsPhysDown(env));
    if (left)    ApplyBind(env, left,    ((KeyBinding*)left)->IsPhysDown(env));
    if (right)   ApplyBind(env, right,   ((KeyBinding*)right)->IsPhysDown(env));
    if (jump)    ApplyBind(env, jump,    ((KeyBinding*)jump)->IsPhysDown(env));
    ApplyBind(env, sneak, false);

    const bool blatant = (InvWalkSettings::mode == 1);
    if (sprint) ApplyBind(env, sprint, blatant);

    env->DeleteLocalRef(gsObj);

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (playerObj) {
        ((Player*)playerObj)->SetSprinting(blatant, env);
        env->DeleteLocalRef(playerObj);
    }

    Sleep(1);
}
