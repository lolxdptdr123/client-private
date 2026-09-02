#pragma once
#include "../Module.h"
#include <jni.h>

namespace NametagSettings {
    inline bool  showNames = true;
    inline bool  showHealth = true;
    inline bool  showDistance = true;
    inline bool  showBackground = true;
    inline bool  showHealthBar = false;
    inline bool  showOutline = true;
    inline bool  hideFriends = false;
    inline bool  enemiesOnly = false;
    inline float nametagScale = 1.f;
    inline float textSize = 20.f;
    inline float outlineThickness = 1.f;
    inline float maxRenderDistance = 64.f;
    inline float healthBarWidth = 50.f;
    inline float healthBarHeight = 4.f;

    inline float friendColor[4] = { 0.f, 1.f, 0.f, 1.f };
    inline float enemyColor[4] = { 1.f, 0.34f, 0.34f, 1.f };
    inline float neutralColor[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float nameColor[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float distanceColor[4] = { 0.8f, 0.8f, 0.8f, 1.f };
    inline float backgroundColor[4] = { 0.f, 0.f, 0.f, 0.5f };
    inline float healthBarBg[4] = { 0.2f, 0.2f, 0.2f, 0.8f };
    inline float healthBarFull[4] = { 0.f, 1.f, 0.f, 1.f };
    inline float healthBarLow[4] = { 1.f, 0.33f, 0.33f, 1.f };
    inline float outlineColor[4] = { 0.f, 0.f, 0.f, 1.f };
}

class Nametag : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Nametags"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override { return ""; }

    void OnRender(JNIEnv* env) override;
    void OnImGuiRender(JNIEnv* env) override;
    void Run(JNIEnv* env) override { Sleep(50); }
};
