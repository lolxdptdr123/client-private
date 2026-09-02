#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace AutoRodSettings {
    inline float fov = 180.f;
    inline float maxLookFov = 90.f;
    inline float maxRange = 10.f;
    inline int   cooldownMs = 250;
    inline bool  ignoreEating = true;
    inline bool  moveFix = true;
    inline bool  onlyIfNotInReach = true;
    inline float meleeReach = 3.f;
    inline bool  click = true;
    inline bool  rotations = true;
    inline bool  luckyThrow = false;
    inline bool  targetPlayers = true;
    inline bool  targetEnemiesOnly = false;
    inline bool  targetMobs = false;
    inline bool  esp = true;
    inline float espColor[4] = { 0.f, 1.f, 0.f, 1.f };
}

class AutoRod : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "AutoRod"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%.1fm", AutoRodSettings::maxRange);
        return buf;
    }

    void Run(JNIEnv* env) override;
    void OnImGuiRender(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
