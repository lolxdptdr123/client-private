#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace StrafeSettings {
    inline int  onGround = 0;
    inline int  inAir = 100;
    inline int  onJump = 100;
    inline int  maxHurtTime = 10;
    inline bool holdingWeapon = false;
}

class Strafe : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Strafe"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[24];
        snprintf(buf, sizeof(buf), "A %d%%", StrafeSettings::inAir);
        return buf;
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
