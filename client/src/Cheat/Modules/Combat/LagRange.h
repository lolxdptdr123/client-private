#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace LagRangeSettings {
    inline int   mode = 0; // 0 Static, 1 Dynamic
    inline float activationDistance = 6.0f;
    inline float flushDistance = 4.0f;
    inline int   delay = 300;
    inline bool  onlyWeapon = true;
    inline bool  onlySprinting = false;
    inline bool  drawBox = true;
    inline float boxColor[4] = { 0.58f, 0.12f, 0.14f, 0.34f };
    inline float outlineColor[4] = { 1.0f, 0.3f, 0.3f, 1.0f };
}

class LagRange : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "LagRange"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        return LagRangeSettings::mode == 1 ? "Dynamic" : "Static";
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
