#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace BowBoostSettings {
    inline int   chargeTicks = 3;
    inline int   delayMs = 50;
    inline bool  switchItem = true;
    inline bool  lookUp = false;
    inline float pitch = 78.f;
}

void BowBoost_Trigger();

class BowBoost : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "BowBoost"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%d", BowBoostSettings::chargeTicks);
        return buf;
    }

    void Run(JNIEnv* env) override;
};
