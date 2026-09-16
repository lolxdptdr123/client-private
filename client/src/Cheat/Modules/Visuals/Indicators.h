#pragma once
#include "../Module.h"
#include <jni.h>

namespace IndicatorsSettings {
    inline bool fireballs = true;
    inline bool pearls = true;
    inline bool arrows = true;
    inline bool comingCloser = false;
}

class Indicators : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Indicators"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override { return ""; }

    void OnImGuiRender(JNIEnv* env) override;
    void Run(JNIEnv* env) override { Sleep(50); }
};
