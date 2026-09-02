#include "pch.h"
#include "Friends.h"

#include "../Visuals/Notifications.h"
#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Cheat/Hack.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <mutex>

struct FriendEntry {
    jlong uuidHi = 0;
    jlong uuidLo = 0;
    int entityId = -1;
    std::string name;
};

static std::mutex s_mu;
static std::vector<FriendEntry> s_friends;

static std::string Lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static FriendEntry MakeEntry(JNIEnv* env, Player* p) {
    FriendEntry e;
    if (!p || !env) return e;
    p->GetUuidBits(env, e.uuidHi, e.uuidLo);
    JniOk(env);
    e.entityId = p->GetEntityId(env);
    JniOk(env);
    e.name = p->GetName(env, true);
    JniOk(env);
    return e;
}

static bool SameUuid(const FriendEntry& a, jlong hi, jlong lo) {
    if (hi == 0 && lo == 0) return false;
    return a.uuidHi == hi && a.uuidLo == lo;
}

static bool MatchesLocked(const FriendEntry& needle) {
    for (const auto& f : s_friends) {
        if (SameUuid(f, needle.uuidHi, needle.uuidLo)) return true;
        if (needle.entityId > 0 && f.entityId == needle.entityId) return true;
        if (!needle.name.empty() && Lower(f.name) == Lower(needle.name)) return true;
    }
    return false;
}

bool FriendsSettings::IsFriend(JNIEnv* env, Player* p) {
    if (!enabled || !p || !env) return false;
    FriendEntry needle = MakeEntry(env, p);
    std::lock_guard<std::mutex> lock(s_mu);
    return MatchesLocked(needle);
}

void FriendsSettings::AddFriend(JNIEnv* env, Player* p) {
    if (!p || !env) return;
    FriendEntry e = MakeEntry(env, p);
    std::lock_guard<std::mutex> lock(s_mu);
    if (MatchesLocked(e)) return;
    s_friends.push_back(std::move(e));
}

void FriendsSettings::RemoveFriend(JNIEnv* env, Player* p) {
    if (!p || !env) return;
    FriendEntry needle = MakeEntry(env, p);
    std::lock_guard<std::mutex> lock(s_mu);
    s_friends.erase(std::remove_if(s_friends.begin(), s_friends.end(),
        [&](const FriendEntry& f) {
            if (SameUuid(f, needle.uuidHi, needle.uuidLo)) return true;
            if (needle.entityId > 0 && f.entityId == needle.entityId) return true;
            if (!needle.name.empty() && Lower(f.name) == Lower(needle.name)) return true;
            return false;
        }), s_friends.end());
}

void FriendsSettings::ClearAll() {
    std::lock_guard<std::mutex> lock(s_mu);
    s_friends.clear();
}

void FriendsSettings::RemoveLast() {
    std::lock_guard<std::mutex> lock(s_mu);
    if (!s_friends.empty()) s_friends.pop_back();
}

void FriendsSettings::RemoveAt(int idx) {
    std::lock_guard<std::mutex> lock(s_mu);
    if (idx < 0 || idx >= (int)s_friends.size()) return;
    s_friends.erase(s_friends.begin() + idx);
}

int FriendsSettings::Count() {
    std::lock_guard<std::mutex> lock(s_mu);
    return (int)s_friends.size();
}

std::vector<std::string> FriendsSettings::DisplayCopy() {
    std::lock_guard<std::mutex> lock(s_mu);
    std::vector<std::string> out;
    out.reserve(s_friends.size());
    for (const auto& f : s_friends)
        out.push_back(f.name.empty() ? ("#" + std::to_string(f.entityId)) : f.name);
    return out;
}

void FriendsSettings::ReplaceFromNames(const std::vector<std::string>& names) {
    std::lock_guard<std::mutex> lock(s_mu);
    s_friends.clear();
    for (const auto& n : names) {
        if (n.empty()) continue;
        FriendEntry e;
        e.name = n;
        s_friends.push_back(std::move(e));
    }
}

static jclass s_playerCls = nullptr;

static void EnsurePlayerClass(JNIEnv* env) {
    if (s_playerCls || !env) return;
    std::string n = Mapper::Get("net/minecraft/entity/player/EntityPlayer");
    if (n.empty()) return;
    Klass* k = g_Instance->FindClass(n.c_str());
    if (!k) return;
    s_playerCls = (jclass)env->NewGlobalRef((jclass)k);
}

void FriendsModule::Run(JNIEnv* env) {
    if (!FriendsSettings::enabled || !env) {
        Sleep(40);
        return;
    }
    if (Overlay::isOpen) {
        Sleep(40);
        return;
    }

    jobject screen = Minecraft::GetCurrentScreen(env);
    if (screen) {
        env->DeleteLocalRef(screen);
        Sleep(40);
        return;
    }

    EnsurePlayerClass(env);

    static bool clearWas = false;
    static bool nearbyWas = false;
    static bool addWas = false;

    if (FriendsSettings::clearFriendsKey != 0) {
        bool down = (GetAsyncKeyState(FriendsSettings::clearFriendsKey) & 0x8000) != 0;
        if (down && !clearWas) {
            int count = FriendsSettings::Count();
            FriendsSettings::ClearAll();
            if (count > 0) {
                char msg[48];
                snprintf(msg, sizeof(msg), "%d friends removed", count);
                NotificationSettings::PushInfo("Friends", msg, "Utility");
            } else {
                NotificationSettings::PushInfo("Friends", "No friends to remove", "Utility");
            }
        }
        clearWas = down;
    } else {
        clearWas = false;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    if (!playerObj || !worldObj) {
        Sleep(40);
        return;
    }
    auto* local = (Player*)playerObj;

    if (FriendsSettings::addNearbyKey != 0) {
        bool down = (GetAsyncKeyState(FriendsSettings::addNearbyKey) & 0x8000) != 0;
        if (down && !nearbyWas) {
            int old = FriendsSettings::Count();
            Vec3D me = local->GetPos(env);
            auto players = ((World*)worldObj)->GetPlayerEntities(env);
            const int myId = local->GetEntityId(env);
            const float r = FriendsSettings::nearbyRadius;
            for (Player* p : players) {
                if (!p) continue;
                if (p->GetEntityId(env) == myId) { env->DeleteLocalRef((jobject)p); continue; }
                Vec3D pos = p->GetPos(env);
                double dx = me.x - pos.x, dy = me.y - pos.y, dz = me.z - pos.z;
                double dist = sqrt(dx * dx + dy * dy + dz * dz);
                if (dist <= (double)r)
                    FriendsSettings::AddFriend(env, p);
                env->DeleteLocalRef((jobject)p);
            }
            int added = FriendsSettings::Count() - old;
            char msg[48];
            if (added == 1) snprintf(msg, sizeof(msg), "1 friend added");
            else snprintf(msg, sizeof(msg), "%d friends added", added);
            NotificationSettings::PushInfo("Friends", msg, "Utility");
        }
        nearbyWas = down;
    } else {
        nearbyWas = false;
    }

    if (FriendsSettings::addFriendKey != 0 && s_playerCls) {
        bool down = (GetAsyncKeyState(FriendsSettings::addFriendKey) & 0x8000) != 0;
        if (down && !addWas) {
            jobject pointed = Minecraft::GetPointedEntity(env);
            JniOk(env);
            if (pointed && env->IsInstanceOf(pointed, s_playerCls)) {
                auto* ent = (Player*)pointed;
                if (ent->GetEntityId(env) != local->GetEntityId(env)) {
                    if (FriendsSettings::IsFriend(env, ent)) {
                        FriendsSettings::RemoveFriend(env, ent);
                        NotificationSettings::PushInfo("Friends", "Friend removed", "Utility");
                    } else {
                        FriendsSettings::AddFriend(env, ent);
                        NotificationSettings::PushInfo("Friends", "Friend added", "Utility");
                    }
                }
            }
            if (pointed) env->DeleteLocalRef(pointed);
        }
        addWas = down;
    } else {
        addWas = false;
    }

    Sleep(20);
}
