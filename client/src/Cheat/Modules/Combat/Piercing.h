#pragma once
#include "../Module.h"
#include <jni.h>

namespace PiercingSettings {
    inline bool weaponsOnly = true;
    inline bool throughBlock = false;
    inline bool targetEnemiesOnly = false;
}

class Piercing : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Piercing"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        return PiercingSettings::throughBlock ? "Blocks" : "";
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
