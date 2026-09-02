#pragma once
#include "../Module.h"
#include <jni.h>

namespace TracerSettings {
    inline bool  hideFriends = false;
    inline bool  enemiesOnly = false;
    inline float maxRenderDistance = 64.f;
    inline float tracerWidth = 2.5f;
    inline float tracerColor[4] = { 1.f, 0.f, 0.f, 1.f };
    inline float friendColor[4] = { 0.f, 1.f, 0.f, 1.f };
    inline float enemyColor[4] = { 1.f, 0.34f, 0.34f, 1.f };
}

class Tracer : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Tracer"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override { return ""; }

    void OnRender(JNIEnv* env) override;
    void OnImGuiRender(JNIEnv* env) override;
    void Run(JNIEnv* env) override { Sleep(50); }
};
