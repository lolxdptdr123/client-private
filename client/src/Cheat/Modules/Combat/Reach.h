#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace ReachSettings {
    inline float distance = 3.2f;
    inline int   activateTicks = 3;
    inline bool  onlySprinting = false;
}

class Reach : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Reach"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", ReachSettings::distance);
        return buf;
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
