#pragma once
#include "../Module.h"
#include <jni.h>
#include <string>
#include <vector>
#include <cstdio>

class Player;

namespace EnemiesSettings {
    inline bool enabled = false;

    inline int addEnemyKey = VK_XBUTTON1;
    inline int addNearbyKey = 0;
    inline int clearEnemiesKey = 0;
    inline float nearbyRadius = 15.f;

    inline bool addEnemyListening = false;
    inline bool addNearbyListening = false;
    inline bool clearListening = false;

    inline char nameInput[64] = "";

    bool IsEnemy(JNIEnv* env, Player* p);
    bool IsEnemyByName(const std::string& name);
    void AddEnemy(JNIEnv* env, Player* p);
    void AddByName(JNIEnv* env, const std::string& name);
    void RemoveEnemy(JNIEnv* env, Player* p);
    void ClearAll();
    void RemoveLast();
    void RemoveAt(int idx);
    int Count();
    std::vector<std::string> DisplayCopy();
    void ReplaceFromNames(const std::vector<std::string>& names);

    inline bool RestrictsTargets() { return enabled && Count() > 0; }
    inline bool BlocksTarget(JNIEnv* env, Player* p) {
        return RestrictsTargets() && !IsEnemy(env, p);
    }
}

class EnemiesModule : public Module {
public:
    const char* GetName()   override { return "Enemies"; }
    bool        IsEnabled() override { return EnemiesSettings::enabled; }
    const char* GetSuffix() override {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%d", EnemiesSettings::Count());
        return buf;
    }
    void Run(JNIEnv* env) override;
};
