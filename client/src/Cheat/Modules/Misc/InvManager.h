#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace InvManagerSettings {
    inline int  delayAfterOpen = 120;
    inline int  speed = 7;
    inline bool smartSpeed = true;
    inline bool randomize = false;
    inline bool equipArmor = true;
    inline bool sortHotbar = true;
    inline bool smartFallbacks = true;
    // 0 None, 1 Sword, 2 Axe, 3 Bow, 4 Blocks, 5 Gapple, 6 Pearl, 7 Rod,
    // 8 Projectiles, 9 Water, 10 Lava, 11 Soup, 12 Potion, 13 Pickaxe, 14 Food, 15 Flint, 16 Web
    inline int  hotbar[9] = { 1, 4, 5, 6, 7, 0, 0, 0, 0 };
}

class InvManager : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Inv Manager"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[24];
        snprintf(buf, sizeof(buf), "Spd %d", InvManagerSettings::speed);
        return buf;
    }

    void Run(JNIEnv* env) override;
};
