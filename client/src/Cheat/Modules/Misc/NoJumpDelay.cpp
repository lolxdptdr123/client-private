#include "pch.h"
#include "NoJumpDelay.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"

void NoJumpDelay::Run(JNIEnv* env) {
    if (!enabled || !env || Overlay::isOpen) {
        Sleep(20);
        return;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) {
        Sleep(5);
        return;
    }
    ((Player*)playerObj)->SetJumpTicks(0, env);
    env->DeleteLocalRef(playerObj);
    Sleep(1);
}
