#pragma once
#include "../Module.h"
#include <jni.h>
#include <atomic>

namespace AutoRefillSettings {
    inline int  mode = 0;       // 0 Blatant, 1 Legit, 2 Semi Blatant
    inline int  itemMode = 0;   // 0 Potion, 1 Soup, 2 Both
    inline int  speed = 7;      // 0-10 (Whip: reelSpeed = 10 - speed)
    inline bool randomMode = false;
    inline bool dynamicSpeed = false;
    inline bool transition = true;
}

void AutoRefill_Trigger();
void AutoRefill_Start();
void AutoRefill_Stop();
bool AutoRefill_IsBusy();

class AutoRefill : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "AutoRefill"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[40];
        const char* m = AutoRefillSettings::mode == 2 ? "Semi"
            : AutoRefillSettings::mode == 1 ? "Legit" : "Blatant";
        snprintf(buf, sizeof(buf), "%s %d", m, AutoRefillSettings::speed);
        return buf;
    }

    void Run(JNIEnv* env) override;
};
