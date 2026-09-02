#pragma once
#include "../Module.h"
#include <jni.h>

namespace TrajectoriesSettings {
    inline bool  bow = true;
    inline bool  potion = true;
    inline bool  pearl = true;
    inline bool  snowball = true;
    inline bool  egg = true;
    inline bool  rod = true;
    inline float lineWidth = 2.f;
    inline float arcColor[4] = { 0.08f, 0.47f, 0.90f, 0.85f };
}

class Trajectories : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Trajectories"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override { return ""; }

    void OnRender(JNIEnv* env) override;
    void Run(JNIEnv* env) override { Sleep(50); }
};
