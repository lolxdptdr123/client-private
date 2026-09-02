#include "pch.h"
#include "AntiBot.h"

#include "../Misc/Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../../../Game/Klass.h"

#include <mutex>
#include <unordered_set>
#include <string>

static std::mutex s_mu;
static std::unordered_set<int> s_spawned;
static std::unordered_set<int> s_moved;
static std::unordered_set<int> s_bots;
static bool s_populated = false;
static bool s_active = false;

static jfieldID s_sendQueue = nullptr;
static jfieldID s_playerInfoMap = nullptr;
static jfieldID s_gameProfile = nullptr;
static jmethodID s_gpGetName = nullptr;
static jmethodID s_mapContains = nullptr;
static jclass s_playerCls = nullptr;
static bool s_jniTried = false;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static void EnsureJni(JNIEnv* env) {
    if (s_jniTried || !env) return;
    s_jniTried = true;

    std::string pN = Mapper::Get("net/minecraft/client/entity/EntityClientPlayerMP");
    Klass* pk = pN.empty() ? nullptr : g_Instance->FindClass(pN.c_str());
    if (pk) {
        std::string sq = Mapper::Get("net/minecraft/client/network/NetHandlerPlayClient", 2);
        s_sendQueue = env->GetFieldID((jclass)pk, Mapper::Get("sendQueue").c_str(), sq.c_str());
        JniOk(env);
    }

    std::string nhN = Mapper::Get("net/minecraft/client/network/NetHandlerPlayClient");
    Klass* nh = nhN.empty() ? nullptr : g_Instance->FindClass(nhN.c_str());
    if (nh) {
        s_playerInfoMap = env->GetFieldID((jclass)nh, Mapper::Get("playerInfoMap").c_str(), "Ljava/util/Map;");
        JniOk(env);
    }

    std::string epN = Mapper::Get("net/minecraft/entity/player/EntityPlayer");
    Klass* ep = epN.empty() ? nullptr : g_Instance->FindClass(epN.c_str());
    if (ep) {
        s_playerCls = (jclass)env->NewGlobalRef((jclass)ep);
        std::string gps = "L" + Mapper::Get("com/mojang/authlib/GameProfile") + ";";
        s_gameProfile = env->GetFieldID((jclass)ep, Mapper::Get("gameProfile").c_str(), gps.c_str());
        JniOk(env);
    }

    std::string gpN = Mapper::Get("com/mojang/authlib/GameProfile");
    Klass* gp = gpN.empty() ? nullptr : g_Instance->FindClass(gpN.c_str());
    if (gp) {
        s_gpGetName = env->GetMethodID((jclass)gp, "getName", "()Ljava/lang/String;");
        JniOk(env);
    }

    jclass mapCls = env->FindClass("java/util/Map");
    if (mapCls) {
        s_mapContains = env->GetMethodID(mapCls, "containsKey", "(Ljava/lang/Object;)Z");
        JniOk(env);
        env->DeleteLocalRef(mapCls);
    }
}

static bool InTabList(JNIEnv* env, jobject entity) {
    if (!s_gameProfile || !s_gpGetName || !s_sendQueue || !s_playerInfoMap || !s_mapContains)
        return true;

    jobject profile = env->GetObjectField(entity, s_gameProfile);
    JniOk(env);
    if (!profile) return false;

    jobject nameObj = env->CallObjectMethod(profile, s_gpGetName);
    env->DeleteLocalRef(profile);
    JniOk(env);
    if (!nameObj) return false;

    jobject local = Minecraft::GetThePlayer(env);
    if (!local) { env->DeleteLocalRef(nameObj); return true; }
    jobject queue = env->GetObjectField(local, s_sendQueue);
    JniOk(env);
    if (!queue) { env->DeleteLocalRef(nameObj); return true; }

    jobject mapObj = env->GetObjectField(queue, s_playerInfoMap);
    env->DeleteLocalRef(queue);
    JniOk(env);
    if (!mapObj) { env->DeleteLocalRef(nameObj); return true; }

    jboolean in = env->CallBooleanMethod(mapObj, s_mapContains, nameObj);
    JniOk(env);
    env->DeleteLocalRef(mapObj);
    env->DeleteLocalRef(nameObj);
    return in == JNI_TRUE;
}

void AntiBot_Reset() {
    std::lock_guard<std::mutex> lock(s_mu);
    s_spawned.clear();
    s_moved.clear();
    s_bots.clear();
    s_populated = false;
}

bool AntiBot_IsBot(JNIEnv* env, jobject entity) {
    if (!s_active || !entity || !env) return false;
    EnsureJni(env);
    if (s_playerCls && !env->IsInstanceOf(entity, s_playerCls)) return false;
    auto* p = (Player*)entity;
    int eid = p->GetEntityId(env);
    int ticks = p->GetTicksExisted(env);
    std::lock_guard<std::mutex> lock(s_mu);
    if (s_bots.count(eid)) return true;
    if (ticks < AntiBotSettings::minTicks && !s_spawned.count(eid)) return true;
    if (AntiBotSettings::checkPackets && ticks > AntiBotSettings::packetGrace && !s_moved.count(eid))
        return true;
    return false;
}

static void Tick(JNIEnv* env) {
    EnsureJni(env);
    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    if (!playerObj || !worldObj) return;
    auto* local = (Player*)playerObj;
    int myId = local->GetEntityId(env);

    auto players = ((World*)worldObj)->GetPlayerEntities(env);

    std::unordered_set<int> worldIds;
    std::unordered_set<int> botsTick;

    if (!s_populated) {
        std::lock_guard<std::mutex> lock(s_mu);
        for (auto* p : players) {
            if (!p) continue;
            int id = p->GetEntityId(env);
            s_spawned.insert(id);
            s_moved.insert(id);
        }
        s_populated = true;
    }

    for (auto* p : players) {
        if (!p) continue;
        int eid = p->GetEntityId(env);
        worldIds.insert(eid);
        if (eid == myId) { env->DeleteLocalRef((jobject)p); continue; }

        int ticks = p->GetTicksExisted(env);
        Vec3D pos = p->GetPos(env);
        Vec3D last = p->GetLastTickPos(env);
        bool moved = (pos.x != last.x || pos.y != last.y || pos.z != last.z);

        {
            std::lock_guard<std::mutex> lock(s_mu);
            if (moved) s_moved.insert(eid);
            if (ticks >= AntiBotSettings::minTicks)
                s_spawned.insert(eid);
        }

        bool flagged = false;
        if (AntiBotSettings::checkTab && ticks > 10 && !InTabList(env, (jobject)p))
            flagged = true;
        if (!flagged && p->GetHealth(env) <= 0.f)
            flagged = true;
        if (!flagged) {
            std::lock_guard<std::mutex> lock(s_mu);
            if (ticks > AntiBotSettings::minTicks + 10 && !s_spawned.count(eid))
                flagged = true;
            if (!flagged && ticks < AntiBotSettings::minTicks && !s_spawned.count(eid))
                flagged = true;
            if (!flagged && AntiBotSettings::checkPackets && ticks > AntiBotSettings::packetGrace && !s_moved.count(eid))
                flagged = true;
        }
        if (flagged) botsTick.insert(eid);
        env->DeleteLocalRef((jobject)p);
    }

    {
        std::lock_guard<std::mutex> lock(s_mu);
        for (int id : botsTick) s_bots.insert(id);
        for (auto it = s_bots.begin(); it != s_bots.end(); ) {
            if (!worldIds.count(*it)) it = s_bots.erase(it);
            else ++it;
        }
    }
}

void AntiBot::Run(JNIEnv* env) {
    s_active = enabled;
    if (!enabled) {
        AntiBot_Reset();
        Sleep(40);
        return;
    }
    if (Overlay::isOpen) { Sleep(20); return; }
    Tick(env);
}
