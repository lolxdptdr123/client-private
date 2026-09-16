#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace BlinkSettings {
    inline int   direction = 0; // 0 OutBound, 1 InBound, 2 Both
    inline int   autoSendDelay = 5000;
    inline bool  disableOnLocalDamage = false;
    inline bool  disableOnTargetDamage = false;
    inline bool  drawEsp = true;
    inline float espColor[4] = { 0.08f, 0.47f, 0.90f, 0.55f };
}

class Blink : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Blink"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        if (BlinkSettings::direction == 1) return "InBound";
        if (BlinkSettings::direction == 2) return "Both";
        return "OutBound";
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
    void OnImGuiRender(JNIEnv* env) override;
};
