#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace AntiBotSettings {
    inline int  minTicks = 20;
    inline bool checkTab = false;
    inline bool checkPackets = false;
    inline int  packetGrace = 40;
}

bool AntiBot_IsBot(JNIEnv* env, jobject entity);
void AntiBot_Reset();

class AntiBot : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "AntiBot"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override { return AntiBotSettings::checkTab ? "Tab" : ""; }

    void Run(JNIEnv* env) override;
};
