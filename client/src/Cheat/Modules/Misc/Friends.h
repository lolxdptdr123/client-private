#pragma once
#include "../Module.h"
#include <jni.h>
#include <string>
#include <vector>
#include <cstdio>

class Player;

namespace FriendsSettings {
    inline bool enabled = false;

    inline int addFriendKey = VK_MBUTTON;
    inline int addNearbyKey = 0;
    inline int clearFriendsKey = 0;
    inline float nearbyRadius = 15.f;

    inline bool addFriendListening = false;
    inline bool addNearbyListening = false;
    inline bool clearListening = false;

    bool IsFriend(JNIEnv* env, Player* p);
    void AddFriend(JNIEnv* env, Player* p);
    void RemoveFriend(JNIEnv* env, Player* p);
    void ClearAll();
    void RemoveLast();
    void RemoveAt(int idx);
    int Count();
    std::vector<std::string> DisplayCopy();
    void ReplaceFromNames(const std::vector<std::string>& names);
}

class FriendsModule : public Module {
public:
    const char* GetName()   override { return "Friends"; }
    bool        IsEnabled() override { return FriendsSettings::enabled; }
    const char* GetSuffix() override {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%d", FriendsSettings::Count());
        return buf;
    }
    void Run(JNIEnv* env) override;
};
