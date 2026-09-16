#include "pch.h"
#include "Sprint.h"

#include "Overlay.h"
#include "../Combat/SprintReset.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/GuiScreen.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Field.h"
#include "../../../Game/Klass.h"
#include "../../../Cheat/Hack.h"

#include <cmath>

static bool s_forced = false;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static void RestoreSprintKey(JNIEnv* env) {
    if (!s_forced || !env) return;
    jobject gs = Minecraft::GetGameSettings(env);
    JniOk(env);
    if (!gs) { s_forced = false; return; }
    jobject kb = ((GameSettings*)gs)->GetKeyBindSprint(env);
    env->DeleteLocalRef(gs);
    JniOk(env);
    if (kb) {
        bool phys = ((KeyBinding*)kb)->IsPhysDown(env);
        JniOk(env);
        ((KeyBinding*)kb)->SetPressed(phys, env);
        JniOk(env);
        env->DeleteLocalRef(kb);
    }
    s_forced = false;
}

static void ForceSprint(JNIEnv* env, Player* local) {
    if (!local) return;
    local->SetSprinting(true, env);
    JniOk(env);

    const auto clazz = (Klass*)env->GetObjectClass((jobject)local);
    if (clazz) {
        Field* f = clazz->GetField(env, Mapper::Get("sprintingTicksLeft").data(), "I");
        env->DeleteLocalRef((jclass)clazz);
        if (f) f->SetIntField(env, local, 600);
        JniOk(env);
    }

    jobject gs = Minecraft::GetGameSettings(env);
    JniOk(env);
    if (!gs) return;
    jobject kb = ((GameSettings*)gs)->GetKeyBindSprint(env);
    env->DeleteLocalRef(gs);
    JniOk(env);
    if (!kb) return;
    ((KeyBinding*)kb)->SetPressed(true, env);
    JniOk(env);
    env->DeleteLocalRef(kb);
    s_forced = true;
}

static void Tick(JNIEnv* env) {
    if (!env || Overlay::isOpen) {
        RestoreSprintKey(env);
        return;
    }
    if (SprintReset_IsStopping()) {
        RestoreSprintKey(env);
        return;
    }

    jobject localObj = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!localObj) {
        RestoreSprintKey(env);
        return;
    }
    auto* local = (Player*)localObj;

    jobject screen = Minecraft::GetCurrentScreen(env);
    JniOk(env);
    if (screen) {
        bool inv = ((GuiScreen*)screen)->IsInventory(env);
        JniOk(env);
        env->DeleteLocalRef(screen);
        if (!SprintSettings::inInventory || !inv) {
            RestoreSprintKey(env);
            env->DeleteLocalRef(localObj);
            return;
        }
    }

    if (local->IsSneaking(env)) {
        JniOk(env);
        RestoreSprintKey(env);
        env->DeleteLocalRef(localObj);
        return;
    }
    JniOk(env);

    if (local->IsUsingItem(env) && !SprintSettings::usingItem) {
        JniOk(env);
        RestoreSprintKey(env);
        env->DeleteLocalRef(localObj);
        return;
    }
    JniOk(env);

    float fwd = local->GetMoveForward(env);
    JniOk(env);
    float str = local->GetMoveStrafing(env);
    JniOk(env);
    bool w = (GetAsyncKeyState('W') & 0x8000) != 0;
    bool s = (GetAsyncKeyState('S') & 0x8000) != 0;
    bool a = (GetAsyncKeyState('A') & 0x8000) != 0;
    bool d = (GetAsyncKeyState('D') & 0x8000) != 0;

    bool forward = fwd > 0.01f || w;
    bool back = fwd < -0.01f || s;
    bool side = fabsf(str) > 0.01f || a || d;

    if (!forward && !back && !side) {
        RestoreSprintKey(env);
        env->DeleteLocalRef(localObj);
        return;
    }
    if (back && !forward && !SprintSettings::backwards) {
        RestoreSprintKey(env);
        env->DeleteLocalRef(localObj);
        return;
    }
    if (side && !forward && !back && !SprintSettings::sideways) {
        RestoreSprintKey(env);
        env->DeleteLocalRef(localObj);
        return;
    }
    if (!forward && !SprintSettings::backwards && !SprintSettings::sideways) {
        RestoreSprintKey(env);
        env->DeleteLocalRef(localObj);
        return;
    }

    ForceSprint(env, local);
    env->DeleteLocalRef(localObj);
}

void Sprint::Run(JNIEnv* env) {
    if (!enabled) {
        RestoreSprintKey(env);
        return;
    }
    Tick(env);
}

void Sprint::OnRender(JNIEnv* env) {
    if (!enabled || !env) {
        RestoreSprintKey(env);
        return;
    }
    Tick(env);
}
