#pragma once
#include "../Module.h"
#include <jni.h>

class NoHurtCam : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "No Hurt Cam"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override { return ""; }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
