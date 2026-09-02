#pragma once
#include "../Module.h"
#include <jni.h>

class NoJumpDelay : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "NoJumpDelay"; }
    bool        IsEnabled() override { return enabled; }

    void Run(JNIEnv* env) override;
};
