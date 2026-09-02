#pragma once
#include "../Module.h"
#include <jni.h>

namespace SnapTapSettings {
    inline int  axis = 0;              // 0 Both, 1 Strafe, 2 Fwd/Back
    inline bool onlyOnGround = false;
    inline bool disableOnSneak = false;
}

class SnapTap : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "SnapTap"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        switch (SnapTapSettings::axis) {
        case 1:  return "Strafe";
        case 2:  return "Fwd/Back";
        default: return "Both";
        }
    }

    void Run(JNIEnv* env) override;

private:
    unsigned long lastTick = 0;
    bool prevLeft = false, prevRight = false, prevForward = false, prevBack = false;
    bool suppLeft = false, suppRight = false, suppForward = false, suppBack = false;
};
