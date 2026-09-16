#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace NoSlowSettings {
    inline int swords = 100;
    inline int bows = 20;
    inline int consumables = 20;
}

class NoSlow : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "NoSlow"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[24];
        snprintf(buf, sizeof(buf), "%d%%", NoSlowSettings::swords);
        return buf;
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
