#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace PlayerEspSettings {
    inline bool armor = true;
    inline bool potions = true;
    inline bool heldItem = true;
    inline bool skeleton = true;
    inline bool outline = true;
    inline bool gapple = true;
    inline bool hideFriends = false;
    inline bool enemiesOnly = false;
    inline int   outlineMode = 0; // 0 3D parts, 1 2D box
    inline bool  outlineGlow = false;
    inline float displayScale = 1.5f;
    inline float skeletonThickness = 1.5f;
    inline float outlineThickness = 1.5f;
    inline float outlineGlowRadius = 6.f;
    inline float skeletonColor[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float outlineColor[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float maxRenderDistance = 64.f;
}

class PlayerEsp : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Player ESP"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        if (PlayerEspSettings::outline && PlayerEspSettings::outlineMode == 1) return "2D";
        if (PlayerEspSettings::outline) return "3D";
        return "";
    }

    void OnRender(JNIEnv* env) override;
    void OnImGuiRender(JNIEnv* env) override;
    void Run(JNIEnv* env) override { Sleep(50); }
};
