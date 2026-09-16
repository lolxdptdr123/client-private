#pragma once
#include "pch.h"
#include "../Module.h"

namespace Clicker {
    inline bool enabled = false;

    inline int  cps = 12;      // CPS (5-25)
    inline int  mode = 0;      // 0 = Blatant, 1 = Butterfly, 2 = Jitter
    inline bool exhaust = true; // Whip : jitter + butterfly seulement

    inline bool requireClick = true;
    inline bool weaponsOnly = false;

    void Start();
    void Stop();
}

extern std::atomic<bool> g_physicalDown;
extern std::atomic<bool> g_physicalRightDown;

class LeftClicker : public Module
{
public:
    const char* GetName()   override { return "Clicker"; }
    bool        IsEnabled() override { return Clicker::enabled; }
    const char* GetSuffix() override {
        static char buf[32];
        snprintf(buf, sizeof(buf), "%s %d",
            Clicker::mode == 2 ? "Jitter" : Clicker::mode == 1 ? "Butterfly" : "Blatant",
            Clicker::cps);
        return buf;
    }
    virtual void OnImGuiRender(JNIEnv* env) override;
};