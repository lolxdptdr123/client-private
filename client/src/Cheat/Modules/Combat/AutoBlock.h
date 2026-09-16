#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace AutoBlockSettings {
    inline float range = 3.0f;
    inline int   maxHurtTimeMs = 200;
    inline int   maxHoldMs = 150;
    inline bool  forceAnim = false;
    inline bool  forceAnimInRange = true;
    inline int   lagChance = 0;
    inline int   lagMaxMs = 150;
    inline bool  preventDelayAttacks = true;
    inline bool  blockAgainImmediately = false;
    inline bool  condLmb = false;
    inline bool  condRmb = false;
    inline bool  condDamaged = false;
}

class AutoBlock : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Auto Block"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[24];
        snprintf(buf, sizeof(buf), "%.1f", AutoBlockSettings::range);
        return buf;
    }

    void Run(JNIEnv* env) override;
};
