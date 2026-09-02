#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace ItemEspSettings {
    inline float color[4] = { 1.f, 0.84f, 0.f, 1.f };
    inline float maxDistance = 32.f;
}

class ItemEsp : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Item ESP"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", ItemEspSettings::maxDistance);
        return buf;
    }

    void OnRender(JNIEnv* env) override;
    void OnImGuiRender(JNIEnv* env) override;
    void Run(JNIEnv* env) override { Sleep(50); }
};
