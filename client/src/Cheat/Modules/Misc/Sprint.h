#pragma once
#include "../Module.h"
#include <jni.h>

namespace SprintSettings {
    inline bool usingItem = false;
    inline bool backwards = false;
    inline bool sideways = false;
    inline bool inInventory = false;
}

class Sprint : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Sprint"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        return (SprintSettings::usingItem || SprintSettings::backwards
            || SprintSettings::sideways || SprintSettings::inInventory)
            ? "Blatant" : "Legit";
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
