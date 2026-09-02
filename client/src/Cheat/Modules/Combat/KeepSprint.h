#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace KeepSprintSettings {
    inline int   mode = 0; // 0 Dynamic, 1 Static
    inline float speed = 0.8f;
    inline int   chance = 100;
    inline bool  weaponsOnly = true;
    inline bool  onlyOnBehind = false;
}

class KeepSprint : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "KeepSprint"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[32];
        snprintf(buf, sizeof(buf), "%s %.1f",
            KeepSprintSettings::mode == 0 ? "Dynamic" : "Static",
            KeepSprintSettings::speed);
        return buf;
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
