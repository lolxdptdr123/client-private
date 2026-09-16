#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace ClutchSettings {
    inline int   range = 3;
    inline float fov = 90.f;
    inline int   minHeight = 2;
    inline float clickSpeed = 14.f;
    inline float randomization = 8.f;
    inline int   selectBlocks = 2; // 0 No, 1 On depletion, 2 Always
    inline bool  onlySideways = false;

    inline float baseSpeed = 18.f;
    inline float acceleration = 14.f;
    inline float accelStrength = 55.f;
    inline bool  multipoint = false;

    inline int   snapDelay = 50;
    inline int   snapDuration = 120;
    inline bool  keepJumpDir = false;
    inline bool  disableAfter = false;

    inline bool  midAir = true;
    inline bool  onHurt = false;
    inline bool  backwards = false;
}

class Clutch : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Clutch"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[24];
        snprintf(buf, sizeof(buf), "R%d", ClutchSettings::range);
        return buf;
    }

    void Run(JNIEnv* env) override;
};
