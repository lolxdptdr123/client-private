#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace ChestStealerSettings {
    inline int  delayMin = 40;
    inline int  delayMax = 80;
    inline int  firstDelay = 80;
    inline int  closeDelay = 50;
    inline bool autoClose = true;
    inline bool nameCheck = true;
    inline bool randomize = false;
    inline bool intelligent = false;
}

class ChestStealer : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "ChestStealer"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[40];
        snprintf(buf, sizeof(buf), "%d-%dms", ChestStealerSettings::delayMin, ChestStealerSettings::delayMax);
        return buf;
    }

    void Run(JNIEnv* env) override;
};
