#pragma once
#include "../Module.h"
#include <jni.h>

namespace FastStopSettings {
    inline int  axis = 0;              // 0 Both, 1 Strafe, 2 Fwd/Back
    inline bool disableOnSneak = false;
}

class FastStop : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "FastStop"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        switch (FastStopSettings::axis) {
        case 1:  return "Strafe";
        case 2:  return "Fwd/Back";
        default: return "Both";
        }
    }

    void Run(JNIEnv* env) override;

private:
    unsigned long lastTick = 0;
    bool prevForward = false, prevBack = false, prevLeft = false, prevRight = false;
    int  forwardHeld = 0, backHeld = 0, leftHeld = 0, rightHeld = 0;
    bool csF = false, csB = false, csL = false, csR = false;

    void ResetHold();
};
