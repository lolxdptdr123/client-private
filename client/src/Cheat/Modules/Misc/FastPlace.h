#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace FastPlaceSettings {
    inline int   mode = 0;          // 0 Delay, 1 Click
    inline int   tickDelay = 0;     // 0–3, Delay mode
    inline bool  onlyBlock = true;
    inline float average = 15.f;    // CPS, Click mode
    inline bool  holdToClick = true;
    inline bool  exhaust = true;
}

class FastPlace : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "FastPlace"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        return FastPlaceSettings::mode == 0 ? "Delay" : "Click";
    }

    void Run(JNIEnv* env) override;
};
