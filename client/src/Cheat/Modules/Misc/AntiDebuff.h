#pragma once
#include "../Module.h"
#include <jni.h>

namespace AntiDebuffSettings {
    inline bool blindness = true;
    inline bool nausea = true;
}

class AntiDebuff : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Anti Debuff"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        const bool b = AntiDebuffSettings::blindness;
        const bool n = AntiDebuffSettings::nausea;
        if (b && n) return nullptr;
        if (b) return "Blindness";
        if (n) return "Nausea";
        return nullptr;
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
