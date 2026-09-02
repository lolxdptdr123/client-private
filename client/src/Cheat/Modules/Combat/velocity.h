#pragma once
#include "../Module.h"
#include <cstdio>

namespace VelocitySettings {
    inline int   mode = 0;              // 0 Blatant, 1 Reverse, 2 Jump, 3 Reduce
    inline float horizontal = 50.f;
    inline float vertical = 100.f;
    inline float reverseStrength = 100.f;
    inline float reduceH = 60.f;
    inline bool  agcBypass = false;
    inline int   jumpDelayMs = 0;
    inline int   chance = 100;
    inline bool  weaponsOnly = true;
    inline bool  onlyWhenMovingForward = false;
    inline bool  onlyLookingAtPlayer = false;
    inline bool  onlyMousePressed = false;
}

class Velocity : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Velocity"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[40];
        if (VelocitySettings::mode == 0)
            snprintf(buf, sizeof(buf), "Blatant %.0f-%.0f",
                VelocitySettings::horizontal, VelocitySettings::vertical);
        else if (VelocitySettings::mode == 1)
            snprintf(buf, sizeof(buf), "Reverse %.0f", VelocitySettings::reverseStrength);
        else if (VelocitySettings::mode == 3)
            snprintf(buf, sizeof(buf), "Reduce %.0f%%", VelocitySettings::reduceH);
        else
            snprintf(buf, sizeof(buf), "Jump");
        return buf;
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
