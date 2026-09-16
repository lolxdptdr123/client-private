#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace RightClicker {
    inline bool enabled = false;
    inline int  cps = 12;
    inline bool blatant = true;
    inline bool exhaust = false;

    void Start();
    void Stop();
}

class RightClickerModule : public Module {
public:
    const char* GetName()   override { return "Right Clicker"; }
    bool        IsEnabled() override { return RightClicker::enabled; }
    const char* GetSuffix() override {
        static char buf[24];
        snprintf(buf, sizeof(buf), "%s %d", RightClicker::blatant ? "Blatant" : "Normal", RightClicker::cps);
        return buf;
    }
};
