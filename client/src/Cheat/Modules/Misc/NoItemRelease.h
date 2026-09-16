#pragma once
#include "../Module.h"
#include <jni.h>

namespace NoItemReleaseSettings {
    inline int mode = 2; // 0 Consumable, 1 Sword, 2 All
}

class NoItemRelease : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "No Item Release"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        return NoItemReleaseSettings::mode == 0 ? "Consumable"
            : NoItemReleaseSettings::mode == 1 ? "Sword" : "All";
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
