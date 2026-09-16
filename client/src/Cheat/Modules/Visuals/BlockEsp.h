#pragma once
#include "../Module.h"
#include <cstdio>
#include <jni.h>

namespace BlockEspSettings {
    inline int   rangeChunks = 2;
    inline int   limitPerChunk = 64;
    inline float outlineWidth = 2.f;
    inline int   ids[32] = { 49, 56, 129, 14, 15 };
    inline float colors[32][4] = {
        { 0.45f, 0.18f, 0.72f, 1.f },
        { 0.25f, 0.85f, 0.95f, 1.f },
        { 0.20f, 0.90f, 0.35f, 1.f },
        { 1.00f, 0.84f, 0.00f, 1.f },
        { 0.75f, 0.75f, 0.78f, 1.f },
    };
}

bool BlockEsp_Has(int id);
bool BlockEsp_Add(int id);
void BlockEsp_Remove(int id);
const float* BlockEsp_Color(int id);

class BlockEsp : public Module {
public:
    bool enabled = false;

    const char* GetName()   override { return "Block ESP"; }
    bool        IsEnabled() override { return enabled; }
    const char* GetSuffix() override {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%d ch", BlockEspSettings::rangeChunks);
        return buf;
    }

    void Run(JNIEnv* env) override;
    void OnRender(JNIEnv* env) override;
};
