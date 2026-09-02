#pragma once
#include "../Module.h"
#include <jni.h>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace ThrowSettings {
    inline bool enabled = false;

    inline bool  potEnabled = false;
    inline float potSpeed = 10.f;
    inline bool  potSmart = false;
    inline bool  potDouble = false;
    inline int   potBind = 0;
    inline bool  potListening = false;

    inline bool  soupEnabled = false;
    inline float soupSpeed = 10.f;
    inline bool  soupSmart = false;
    inline bool  soupDouble = false;
    inline bool  soupAutoDrop = false;
    inline int   soupBind = 0;
    inline bool  soupListening = false;

    inline bool  debuffEnabled = false;
    inline float debuffSpeed = 10.f;
    inline bool  debuffDouble = false;
    inline int   debuffBind = 0;
    inline bool  debuffListening = false;

    inline bool  pearlEnabled = false;
    inline float pearlSpeed = 10.f;
    inline int   pearlBind = 0;
    inline bool  pearlListening = false;
}

void Throw_Start();
void Throw_Stop();
bool Throw_IsBusy();

class ThrowModule : public Module {
public:
    const char* GetName() override { return "Throw"; }
    bool IsEnabled() override {
        return ThrowSettings::enabled && (
            ThrowSettings::potEnabled || ThrowSettings::soupEnabled
            || ThrowSettings::debuffEnabled || ThrowSettings::pearlEnabled);
    }
    const char* GetSuffix() override {
        static char buf[48];
        buf[0] = 0;
        if (ThrowSettings::potEnabled) strcat_s(buf, sizeof(buf), "Health ");
        if (ThrowSettings::soupEnabled) strcat_s(buf, sizeof(buf), "Soup ");
        if (ThrowSettings::debuffEnabled) strcat_s(buf, sizeof(buf), "Debuff ");
        if (ThrowSettings::pearlEnabled) strcat_s(buf, sizeof(buf), "Pearl ");
        size_t n = strlen(buf);
        if (n > 0) buf[n - 1] = 0;
        return buf;
    }
    void Run(JNIEnv* env) override;
};
