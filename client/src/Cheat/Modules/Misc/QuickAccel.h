#pragma once
#include "../Module.h"
#include <jni.h>

namespace QuickAccelSettings {
    inline bool disableOnSneak = true;
}

class QuickAccel : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "QuickAccel"; }
    bool        IsEnabled() override { return enabled; }

    void Run(JNIEnv* env) override;
};
