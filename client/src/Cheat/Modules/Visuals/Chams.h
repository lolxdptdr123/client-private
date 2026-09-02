#pragma once
#include "../Module.h"
#include <jni.h>

namespace ChamsSettings {
    inline bool players = true;
    inline bool mobs = true;
    inline bool animals = true;
    inline bool villagers = true;
    inline bool armorStands = true;
    inline bool invisibles = false;
    inline bool renderTexture = false;
    inline bool glowMode = false;
    inline bool hideFriends = false;
    inline bool enemiesOnly = false;
    inline float colorNeutral[4] = { 1.f, 1.f, 1.f, 0.39f };
    inline float colorFriend[4] = { 0.f, 1.f, 0.f, 0.39f };
    inline float colorEnemy[4] = { 1.f, 0.34f, 0.34f, 0.39f };
}

class Chams : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Chams"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        if (ChamsSettings::glowMode) return "Glow";
        if (ChamsSettings::renderTexture) return "Texture";
        return "";
    }

    void OnRender(JNIEnv* env) override;
    void Run(JNIEnv* env) override { Sleep(50); }
};
