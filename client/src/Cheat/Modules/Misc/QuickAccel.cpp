#include "pch.h"
#include "QuickAccel.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Game/Mapper.h"
#include <cmath>

static void ScaleUnitFloat(JNIEnv* env, jobject obj, const char* mapKey) {
    if (!obj || !env) return;
    const auto cls = (Klass*)env->GetObjectClass(obj);
    if (!cls) return;
    const auto field = cls->GetField(env, Mapper::Get(mapKey).data(), "F");
    env->DeleteLocalRef((jclass)cls);
    if (!field) return;
    float v = field->GetFloatField(env, obj);
    if (fabsf(v) > 1e-4f && fabsf(v) <= 1.05f)
        field->SetFloatField(env, obj, v * 5.f);
}

void QuickAccel::Run(JNIEnv* env) {
    if (!enabled || !env || Overlay::isOpen) {
        Sleep(20);
        return;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) {
        Sleep(5);
        return;
    }
    auto* player = (Player*)playerObj;

    if (QuickAccelSettings::disableOnSneak && player->IsSneaking(env)) {
        env->DeleteLocalRef(playerObj);
        Sleep(2);
        return;
    }

    jobject gsObj = Minecraft::GetGameSettings(env);
    if (gsObj) {
        jobject use = ((GameSettings*)gsObj)->GetKeyBindUseItem(env);
        if (use) {
            bool usingItem = ((KeyBinding*)use)->IsPressed(env);
            env->DeleteLocalRef(use);
            env->DeleteLocalRef(gsObj);
            if (usingItem) {
                env->DeleteLocalRef(playerObj);
                Sleep(2);
                return;
            }
        } else {
            env->DeleteLocalRef(gsObj);
        }
    }

    jobject input = player->GetMovementInput(env);
    if (input) {
        ScaleUnitFloat(env, input, "moveForward");
        ScaleUnitFloat(env, input, "moveStrafe");
        env->DeleteLocalRef(input);
    }

    ScaleUnitFloat(env, playerObj, "moveForward");
    ScaleUnitFloat(env, playerObj, "moveStrafing");

    env->DeleteLocalRef(playerObj);
    Sleep(1);
}
