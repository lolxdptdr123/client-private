#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace BacktrackSettings {
    inline int   mode = 0; // 0 Lag, 1 Smooth, 2 Advanced

    inline int   delayInTicks = 4;
    inline int   cooldown = 500;
    inline bool  distanceCheck = true;
    inline float distance = 1.0f;
    inline float distanceMax = 4.0f;

    inline int   smoothDelayMs = 200;
    inline int   forceFlushMs = 500;
    inline bool  onlySprinting = false;

    inline int   maxDelay = 200;
    inline int   minDelay = 0;
    inline int   delayBetweenLags = 0;
    inline int   stopAtHurt = 10;
    inline int   disableOn = 0; // 0 None, 1 OnAttack, 2 OnRange, 3 ClickCheck
    inline float stopOnAttackRange = 3.0f;
    inline bool  onlyWhenNeeded = true;
    inline bool  continueAtHurtTime = false;

    inline bool  drawBox = true;
    inline float boxColor[4] = { 0.14f, 0.12f, 0.58f, 0.34f };
    inline float outlineColor[4] = { 1.f, 1.f, 1.f, 1.f };
}

class Backtrack : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Backtrack"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[32];
        if (BacktrackSettings::mode == 0)
            snprintf(buf, sizeof(buf), "Lag %dt", BacktrackSettings::delayInTicks);
        else if (BacktrackSettings::mode == 1) {
            if (BacktrackSettings::forceFlushMs >= 1001)
                snprintf(buf, sizeof(buf), "Smooth Never");
            else
                snprintf(buf, sizeof(buf), "Smooth %dms", BacktrackSettings::forceFlushMs);
        } else
            snprintf(buf, sizeof(buf), "Advanced %dms", BacktrackSettings::maxDelay);
        return buf;
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
