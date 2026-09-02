#include "pch.h"
#include "Enemies.h"

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

struct EnemyEntry {
    jlong uuidHi = 0;
    jlong uuidLo = 0;
    int entityId = -1;
    std::string name;
};

static std::mutex s_mu;
static std::vector<EnemyEntry> s_enemies;

static std::string Lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static EnemyEntry MakeEntry(JNIEnv* env, Player* p) {
    EnemyEntry e;
    if (!p || !env) return e;
    p->GetUuidBits(env, e.uuidHi, e.uuidLo);
    JniOk(env);
    e.entityId = p->GetEntityId(env);
    JniOk(env);
    e.name = p->GetName(env, true);
    JniOk(env);
    return e;
}

static bool SameUuid(const EnemyEntry& a, jlong hi, jlong lo) {
    if (hi == 0 && lo == 0) return false;
    return a.uuidHi == hi && a.uuidLo == lo;
}

static bool MatchesLocked(const EnemyEntry& needle) {
    for (const auto& f : s_enemies) {
        if (SameUuid(f, needle.uuidHi, needle.uuidLo)) return true;
        if (needle.entityId > 0 && f.entityId == needle.entityId) return true;
        if (!needle.name.empty() && Lower(f.name) == Lower(needle.name)) return true;
    }
    return false;
}

bool EnemiesSettings::IsEnemy(JNIEnv* env, Player* p) {
    if (!enabled || !p || !env) return false;
    EnemyEntry needle = MakeEntry(env, p);
    std::lock_guard<std::mutex> lock(s_mu);
    return MatchesLocked(needle);
}

bool EnemiesSettings::IsEnemyByName(const std::string& name) {
    if (name.empty()) return false;
    std::string n = Lower(name);
    std::lock_guard<std::mutex> lock(s_mu);
    for (const auto& e : s_enemies) {
        if (!e.name.empty() && Lower(e.name) == n) return true;
    }
    return false;
}

void EnemiesSettings::AddEnemy(JNIEnv* env, Player* p) {
    if (!p || !env) return;
    EnemyEntry e = MakeEntry(env, p);
    std::lock_guard<std::mutex> lock(s_mu);
    if (MatchesLocked(e)) return;
    s_enemies.push_back(std::move(e));
}

void EnemiesSettings::AddByName(JNIEnv* env, const std::string& name) {
    if (name.empty() || IsEnemyByName(name)) return;
    if (env) {
        jobject worldObj = Minecraft::GetTheWorld(env);
        if (worldObj) {
            auto players = ((World*)worldObj)->GetPlayerEntities(env);
            std::string want = Lower(name);
            for (Player* p : players) {
                if (!p) continue;
                std::string n = p->GetName(env, true);
                bool hit = !n.empty() && Lower(n) == want;
                if (hit) AddEnemy(env, p);
                env->DeleteLocalRef((jobject)p);
                if (hit) {
                    env->DeleteLocalRef(worldObj);
                    return;
                }
            }
            env->DeleteLocalRef(worldObj);
        }
    }
    EnemyEntry e;
    e.name = name;
    std::lock_guard<std::mutex> lock(s_mu);
    s_enemies.push_back(std::move(e));
}

void EnemiesSettings::RemoveEnemy(JNIEnv* env, Player* p) {
    if (!p || !env) return;
    EnemyEntry needle = MakeEntry(env, p);
    std::lock_guard<std::mutex> lock(s_mu);
    s_enemies.erase(std::remove_if(s_enemies.begin(), s_enemies.end(),
        [&](const EnemyEntry& f) {
            if (SameUuid(f, needle.uuidHi, needle.uuidLo)) return true;
            if (needle.entityId > 0 && f.entityId == needle.entityId) return true;
            if (!needle.name.empty() && Lower(f.name) == Lower(needle.name)) return true;
            return false;
        }), s_enemies.end());
}

void EnemiesSettings::ClearAll() {
    std::lock_guard<std::mutex> lock(s_mu);
    s_enemies.clear();
}

void EnemiesSettings::RemoveLast() {
    std::lock_guard<std::mutex> lock(s_mu);
    if (!s_enemies.empty()) s_enemies.pop_back();
}

void EnemiesSettings::RemoveAt(int idx) {
    std::lock_guard<std::mutex> lock(s_mu);
    if (idx < 0 || idx >= (int)s_enemies.size()) return;
    s_enemies.erase(s_enemies.begin() + idx);
}

int EnemiesSettings::Count() {
    std::lock_guard<std::mutex> lock(s_mu);
    return (int)s_enemies.size();
}

std::vector<std::string> EnemiesSettings::DisplayCopy() {
    std::lock_guard<std::mutex> lock(s_mu);
    std::vector<std::string> out;
    out.reserve(s_enemies.size());
    for (const auto& f : s_enemies)
        out.push_back(f.name.empty() ? ("#" + std::to_string(f.entityId)) : f.name);
    return out;
}

void EnemiesSettings::ReplaceFromNames(const std::vector<std::string>& names) {
    std::lock_guard<std::mutex> lock(s_mu);
    s_enemies.clear();
    for (const auto& n : names) {
        if (n.empty()) continue;
        EnemyEntry e;
        e.name = n;
        s_enemies.push_back(std::move(e));
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

void EnemiesModule::Run(JNIEnv* env) {
    if (!EnemiesSettings::enabled || !env) {
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

    if (EnemiesSettings::clearEnemiesKey != 0) {
        bool down = (GetAsyncKeyState(EnemiesSettings::clearEnemiesKey) & 0x8000) != 0;
        if (down && !clearWas) {
            int count = EnemiesSettings::Count();
            EnemiesSettings::ClearAll();
            if (count > 0) {
                char msg[48];
                snprintf(msg, sizeof(msg), "%d enemies removed", count);
                NotificationSettings::PushInfo("Enemies", msg, "Utility");
            } else {
                NotificationSettings::PushInfo("Enemies", "No enemies to remove", "Utility");
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

    if (EnemiesSettings::addNearbyKey != 0) {
        bool down = (GetAsyncKeyState(EnemiesSettings::addNearbyKey) & 0x8000) != 0;
        if (down && !nearbyWas) {
            int old = EnemiesSettings::Count();
            Vec3D me = local->GetPos(env);
            auto players = ((World*)worldObj)->GetPlayerEntities(env);
            const int myId = local->GetEntityId(env);
            const float r = EnemiesSettings::nearbyRadius;
            for (Player* p : players) {
                if (!p) continue;
                if (p->GetEntityId(env) == myId) { env->DeleteLocalRef((jobject)p); continue; }
                Vec3D pos = p->GetPos(env);
                double dx = me.x - pos.x, dy = me.y - pos.y, dz = me.z - pos.z;
                double dist = sqrt(dx * dx + dy * dy + dz * dz);
                if (dist <= (double)r)
                    EnemiesSettings::AddEnemy(env, p);
                env->DeleteLocalRef((jobject)p);
            }
            int added = EnemiesSettings::Count() - old;
            char msg[48];
            if (added == 1) snprintf(msg, sizeof(msg), "1 enemy added");
            else snprintf(msg, sizeof(msg), "%d enemies added", added);
            NotificationSettings::PushInfo("Enemies", msg, "Utility");
        }
        nearbyWas = down;
    } else {
        nearbyWas = false;
    }

    if (EnemiesSettings::addEnemyKey != 0 && s_playerCls) {
        bool down = (GetAsyncKeyState(EnemiesSettings::addEnemyKey) & 0x8000) != 0;
        if (down && !addWas) {
            jobject pointed = Minecraft::GetPointedEntity(env);
            JniOk(env);
            if (pointed && env->IsInstanceOf(pointed, s_playerCls)) {
                auto* ent = (Player*)pointed;
                if (ent->GetEntityId(env) != local->GetEntityId(env)) {
                    if (EnemiesSettings::IsEnemy(env, ent)) {
                        EnemiesSettings::RemoveEnemy(env, ent);
                        NotificationSettings::PushInfo("Enemies", "Enemy removed", "Utility");
                    } else {
                        EnemiesSettings::AddEnemy(env, ent);
                        NotificationSettings::PushInfo("Enemies", "Enemy added", "Utility");
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
