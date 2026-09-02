#pragma once
#include "../Module.h"
#include <jni.h>

namespace InvWalkSettings {
    inline int mode = 0; // 0 Legit, 1 Blatant
}

class InvWalk : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "InvWalk"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        return InvWalkSettings::mode == 1 ? "Blatant" : "Legit";
    }

    void Run(JNIEnv* env) override;
};
