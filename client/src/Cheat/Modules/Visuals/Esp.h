#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace EspSettings {
    inline int   renderMode = 2;   // 0 2D, 1 3D, 2 Both
    inline int   mode3d = 2;       // 0 Outline, 1 Fill, 2 Both
    inline int   mode2d = 2;

    inline bool  showHealthBar = true;
    inline bool  hideFriends = false;
    inline bool  enemiesOnly = false;
    inline float maxRenderDistance = 64.f;

    inline float outline3dColor[4] = { 0.f, 0.498f, 0.961f, 1.f };
    inline float fill3dColor[4]    = { 0.f, 0.498f, 0.961f, 1.f };
    inline float fill3dOpacity = 0.3f;
    inline float outline2dColor[4] = { 0.f, 0.498f, 0.961f, 1.f };
    inline float fill2dColor[4]    = { 0.f, 0.498f, 0.961f, 0.2f };
    inline float friendColor[4]    = { 0.f, 1.f, 0.f, 1.f };
    inline float enemyColor[4]     = { 1.f, 0.34f, 0.34f, 1.f };
    inline float neutralColor[4]   = { 1.f, 1.f, 1.f, 1.f };
    inline float healthBarBg[4]    = { 0.2f, 0.2f, 0.2f, 0.8f };
    inline float healthBarFull[4]  = { 0.f, 1.f, 0.f, 1.f };
    inline float healthBarLow[4]   = { 1.f, 0.f, 0.f, 1.f };

    inline float outline3dWidth = 2.f;
    inline float outline2dWidth = 1.5f;
    inline float healthBarWidth = 3.f;
    inline float healthBarOffset = 5.f;
}

class Esp : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "ESP"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        if (EspSettings::renderMode == 0) return "2D";
        if (EspSettings::renderMode == 2) return "Both";
        return "3D";
    }

    void OnRender(JNIEnv* env) override;
    void OnImGuiRender(JNIEnv* env) override;
    void Run(JNIEnv* env) override { Sleep(50); }
};
