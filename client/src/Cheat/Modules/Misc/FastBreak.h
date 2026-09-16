#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace FastBreakSettings {
    inline int   mode = 0;       // 0 Normal, 1 Timer
    inline float power = 0.f;    // 0–100, Normal
    inline float multiplier = 1.f; // Timer
    inline int   liveDelay = 0;
    inline int   liveRemain = 0;
}

class FastBreak : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "FastBreak"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[40];
        snprintf(buf, sizeof(buf), "%d t", FastBreakSettings::liveRemain);
        return buf;
    }

    void Run(JNIEnv* env) override;
};
