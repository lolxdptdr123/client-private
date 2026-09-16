#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace BridgeAssistSettings {
    inline float edgeOffset = 0.15f;
    inline int   unsneakDelay = 40;
    inline float pitch = 55.f;
    inline bool  onlyBlocks = true;
    inline bool  lookingDown = true;
    inline bool  sneakOnJump = false;
}

class BridgeAssist : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "BridgeAssist"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[32];
        snprintf(buf, sizeof(buf), "Edge %.2f", BridgeAssistSettings::edgeOffset);
        return buf;
    }

    void Run(JNIEnv* env) override;
};
