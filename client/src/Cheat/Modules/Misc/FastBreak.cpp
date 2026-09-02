#include "pch.h"
#include "FastBreak.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Cheat/Hack.h"

static Field* s_dmg = nullptr;
static Field* s_delay = nullptr;
static bool s_tried = false;
static float s_lastDamage = 0.f;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static void Ensure(JNIEnv* env) {
    if (s_tried || !env) return;
    s_tried = true;
    std::string n = Mapper::Get("net/minecraft/client/multiplayer/PlayerControllerMP");
    if (n.empty()) return;
    Klass* k = g_Instance->FindClass(n.c_str());
    if (!k) return;
    s_dmg = k->GetField(env, Mapper::Get("curBlockDamageMP").c_str(), "F");
    JniOk(env);
    s_delay = k->GetField(env, Mapper::Get("blockHitDelay").c_str(), "I");
    JniOk(env);
}

void FastBreak::Run(JNIEnv* env) {
    if (!enabled || !env || Overlay::isOpen) {
        s_lastDamage = 0.f;
        Sleep(20);
        return;
    }

    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) {
        s_lastDamage = 0.f;
        Sleep(2);
        return;
    }

    Ensure(env);
    jobject pc = Minecraft::GetPlayerController(env);
    JniOk(env);
    if (!pc || !s_dmg) {
        Sleep(2);
        return;
    }

    if (s_delay) {
        int delay = s_delay->GetIntField(env, pc);
        JniOk(env);
        if (delay > 0)
            s_delay->SetIntField(env, pc, 0);
    }

    float cur = s_dmg->GetFloatField(env, pc);
    JniOk(env);

    if (FastBreakSettings::mode == 0) {
        const float thresh = 1.f - (FastBreakSettings::power / 100.f);
        if (cur >= thresh)
            s_dmg->SetFloatField(env, pc, 1.f);
    } else {
        if (cur > 0.f) {
            float diff = cur - s_lastDamage;
            cur += diff * (FastBreakSettings::multiplier - 1.f);
            if (cur > 1.f) cur = 1.f;
            if (cur < 0.f) cur = 0.f;
            s_dmg->SetFloatField(env, pc, cur);
            s_lastDamage = cur;
        } else if (s_lastDamage > 0.f) {
            s_lastDamage = 0.f;
        }
    }
    JniOk(env);
    env->DeleteLocalRef(pc);
    Sleep(1);
}
