#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace CriticalsSettings {
    inline int   mode = 0; // 0 Packet, 1 Timer
    inline int   chance = 100;
    inline float timerSpeed = 0.5f;
    inline int   maxQueueTime = 500;
}

class Criticals : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Criticals"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        return CriticalsSettings::mode == 0 ? "Packet" : "Timer";
    }

    void Run(JNIEnv* env) override;
};
