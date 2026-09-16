#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace BlockInSettings {
    inline float speed = 5.f;
    inline bool  onlyOnGround = true;
}

class BlockIn : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Block In"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[24];
        snprintf(buf, sizeof(buf), "%.0f", BlockInSettings::speed);
        return buf;
    }

    void Run(JNIEnv* env) override;
};
