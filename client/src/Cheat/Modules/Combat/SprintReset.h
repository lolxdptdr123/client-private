#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace SprintResetSettings {
    inline int  mode = 0; // 0 WTap, 1 Sneak, 2 NoStop
    inline int  delayMs = 250;
    inline int  stopMs = 50;
    inline bool randomize = true;
    inline bool waitForDamage = false;
    inline bool holdingWeapon = true;
}

class SprintReset : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Sprint Reset"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        if (SprintResetSettings::mode == 1) return "Sneak";
        if (SprintResetSettings::mode == 2) return "NoStop";
        return "WTap";
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};

bool SprintReset_IsStopping();
