#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace AutoWeaponSettings {
    inline int  activationMs = 50;
}

class AutoWeapon : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Auto Weapon"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[24];
        snprintf(buf, sizeof(buf), "%dms", AutoWeaponSettings::activationMs);
        return buf;
    }

    void Run(JNIEnv* env) override;
};
