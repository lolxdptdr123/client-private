#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace AutoToolSettings {
    inline bool switchBack = true;
    inline bool onlyMining = true;
    inline bool preferSilk = false;
    inline int  delayMs = 0;
}

class AutoTool : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "AutoTool"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[32];
        if (AutoToolSettings::onlyMining)
            snprintf(buf, sizeof(buf), "Mine");
        else
            snprintf(buf, sizeof(buf), "Look");
        return buf;
    }

    void Run(JNIEnv* env) override;
};
