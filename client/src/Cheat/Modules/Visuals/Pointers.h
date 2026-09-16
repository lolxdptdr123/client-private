#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace PointersSettings {
    inline float range = 64.f;
    inline float ignoreFov = 0.f;
    inline bool  hideFriendlies = false;
    inline int   style = 0;      // 0 2D, 1 3D
    inline int   colorMode = 0;  // 0 Distance, 1 Name Tag, 2 Manual
    inline float nearColor[4] = { 1.f, 0.25f, 0.25f, 1.f };
    inline float farColor[4] = { 0.25f, 1.f, 0.35f, 1.f };
    inline float enemyColor[4] = { 1.f, 0.34f, 0.34f, 1.f };
    inline float friendColor[4] = { 0.3f, 1.f, 0.4f, 1.f };
    inline float nearDist = 10.f;
    inline float farDist = 50.f;
    inline float scale = 1.f;
    inline float radius = 80.f;
    inline bool  distanceRadius = false;
}

class Pointers : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Pointers"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", PointersSettings::range);
        return buf;
    }

    void OnImGuiRender(JNIEnv* env) override;
    void Run(JNIEnv* env) override { Sleep(50); }
};
