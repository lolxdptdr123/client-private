#pragma once
#include "../Module.h"
#include <jni.h>

namespace AimAssistSettings {
    inline int   currentMode = 0; // 0 = Blatant, 1 = Legit
    inline float speed = 5.f;
    inline float fovMin = 0.f;
    inline float fovMax = 120.f;
    inline float distanceMin = 0.f;
    inline float distanceMax = 6.f;
    inline int   priority = 0; // 0 Distance, 1 Fov, 2 HurtTime
    inline bool  targetPlayers = true;
    inline bool  allowInvisible = false;
    inline bool  allowNaked = true;
    inline bool  requireClick = true;
    inline bool  weaponsOnly = true;
    inline bool  breakBlock = false;
    inline bool  keepOnTarget = false;
    inline int   keepOnTargetKeybind = 0;
    inline bool  keepBindListening = false;
    inline int   multipoint = 50;
}

class AimAssist : public Module
{
public:
    bool enabled = false;

    const char* GetName()   override { return "Aim Assist"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[32];
        snprintf(buf, sizeof(buf), "%s %.1f",
            AimAssistSettings::currentMode == 0 ? "Blatant" : "Legit",
            AimAssistSettings::speed);
        return buf;
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
