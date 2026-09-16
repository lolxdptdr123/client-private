#pragma once
#include "../Module.h"
#include <jni.h>

class PingFix : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Ping Fix"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override { return ""; }

    void Run(JNIEnv* env) override;
};
