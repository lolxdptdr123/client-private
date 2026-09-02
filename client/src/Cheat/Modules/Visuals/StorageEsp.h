#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace StorageEspSettings {
    inline int   renderMode = 2; // 0 2D, 1 3D, 2 Both
    inline int   mode3d = 0;     // 0 Outline, 1 Fill, 2 Both
    inline int   mode2d = 2;
    inline bool  showLabels = false;
    inline float maxDistance = 64.f;
    inline float outline3dWidth = 2.f;
    inline float outline2dWidth = 1.5f;
    inline float fillAlpha3d = 0.15f;
    inline float fillAlpha2d = 0.25f;
    inline float labelScale = 1.f;

    inline bool chest = true;
    inline bool enderChest = true;
    inline bool furnace = true;
    inline bool dispenser = true;
    inline bool dropper = true;
    inline bool hopper = true;

    inline float chestColor[4] = { 1.f, 0.84f, 0.f, 1.f };
    inline float enderChestColor[4] = { 0.5f, 0.f, 0.5f, 1.f };
    inline float furnaceColor[4] = { 0.5f, 0.5f, 0.5f, 1.f };
    inline float dispenserColor[4] = { 0.3f, 0.3f, 0.3f, 1.f };
    inline float dropperColor[4] = { 0.4f, 0.4f, 0.4f, 1.f };
    inline float hopperColor[4] = { 0.2f, 0.2f, 0.2f, 1.f };
    inline float labelColor[4] = { 1.f, 1.f, 1.f, 1.f };
}

class StorageEsp : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Storage ESP"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        if (StorageEspSettings::renderMode == 0) return "2D";
        if (StorageEspSettings::renderMode == 2) return "Both";
        return "3D";
    }

    void OnRender(JNIEnv* env) override;
    void OnImGuiRender(JNIEnv* env) override;
    void Run(JNIEnv* env) override { Sleep(50); }
};
