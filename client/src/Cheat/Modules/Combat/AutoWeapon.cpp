#include "pch.h"
#include "AutoWeapon.h"

#include "AntiBot.h"
#include "SwordCheck.h"
#include "../Misc/Weapons.h"
#include "../Misc/Overlay.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Classes/MovingObjectPosition.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Cheat/Hack.h"

#include <algorithm>

static ULONGLONG s_aimSince = 0;
static int s_lastId = -1;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static int WeaponScore(JNIEnv* env, jobject stack, int slot) {
    if (!stack) return -1;
    auto* st = (ItemStack*)stack;
    if (st->IsEmpty(env)) return -1;
    JniOk(env);
    if (!Weapons_IsStack(env, st, slot)) {
        JniOk(env);
        return -1;
    }
    int id = st->GetItemId(env);
    JniOk(env);
    switch (id) {
    case 276: return 100;
    case 267: return 80;
    case 272: return 60;
    case 268: return 40;
    case 283: return 35;
    case 279: return 90;
    case 258: return 70;
    case 275: return 50;
    case 271: return 30;
    case 286: return 25;
    default:  return 20;
    }
}

static bool IsPlayerEntity(JNIEnv* env, jobject ent, jobject local) {
    if (!ent || !local) return false;
    if (env->IsSameObject(ent, local)) return false;
    std::string n = Mapper::Get("net/minecraft/entity/player/EntityPlayer");
    if (n.empty()) n = "net/minecraft/entity/player/EntityPlayer";
    Klass* k = g_Instance->FindClass(n.c_str());
    if (!k) return false;
    bool ok = env->IsInstanceOf(ent, (jclass)k) != JNI_FALSE;
    JniOk(env);
    return ok;
}

static jobject GetAimedEntity(JNIEnv* env) {
    jobject mop = Minecraft::GetObjectMouseOver(env);
    JniOk(env);
    if (mop) {
        if (((MovingObjectPosition*)mop)->IsAimingEntity(env)) {
            JniOk(env);
            jclass c = env->GetObjectClass(mop);
            std::string hit = Mapper::Get("entityHit");
            if (hit.empty()) hit = "entityHit";
            std::string entSig = Mapper::Get("net/minecraft/entity/Entity", 2);
            jobject found = nullptr;
            if (!entSig.empty()) {
                jfieldID f = env->GetFieldID(c, hit.c_str(), entSig.c_str());
                JniOk(env);
                if (f) found = env->GetObjectField(mop, f);
                JniOk(env);
            }
            env->DeleteLocalRef(c);
            env->DeleteLocalRef(mop);
            if (found) return found;
        } else {
            env->DeleteLocalRef(mop);
        }
    }
    return Minecraft::GetPointedEntity(env);
}

void AutoWeapon::Run(JNIEnv* env) {
    if (!enabled || !env || Overlay::isOpen) {
        s_aimSince = 0;
        s_lastId = -1;
        Sleep(20);
        return;
    }

    jobject screen = Minecraft::GetCurrentScreen(env);
    JniOk(env);
    if (screen) {
        env->DeleteLocalRef(screen);
        s_aimSince = 0;
        s_lastId = -1;
        Sleep(10);
        return;
    }

    jobject localObj = Minecraft::GetThePlayer(env);
    JniOk(env);
    jobject aimed = GetAimedEntity(env);
    JniOk(env);
    if (!localObj || !aimed || !IsPlayerEntity(env, aimed, localObj)) {
        if (aimed) env->DeleteLocalRef(aimed);
        if (localObj) env->DeleteLocalRef(localObj);
        s_aimSince = 0;
        s_lastId = -1;
        Sleep(4);
        return;
    }

    auto* target = (Player*)aimed;
    if (FriendsSettings::enabled && FriendsSettings::IsFriend(env, target)) {
        env->DeleteLocalRef(aimed);
        env->DeleteLocalRef(localObj);
        s_aimSince = 0;
        s_lastId = -1;
        Sleep(4);
        return;
    }
    if (EnemiesSettings::BlocksTarget(env, target)) {
        env->DeleteLocalRef(aimed);
        env->DeleteLocalRef(localObj);
        s_aimSince = 0;
        s_lastId = -1;
        Sleep(4);
        return;
    }
    if (AntiBot_IsBot(env, aimed)) {
        env->DeleteLocalRef(aimed);
        env->DeleteLocalRef(localObj);
        s_aimSince = 0;
        s_lastId = -1;
        Sleep(4);
        return;
    }

    int id = target->GetEntityId(env);
    JniOk(env);
    ULONGLONG now = GetTickCount64();
    if (id != s_lastId) {
        s_lastId = id;
        s_aimSince = now;
    }
    int wait = (std::max)(0, AutoWeaponSettings::activationMs);
    if (now - s_aimSince < (ULONGLONG)wait) {
        env->DeleteLocalRef(aimed);
        env->DeleteLocalRef(localObj);
        Sleep(1);
        return;
    }

    jobject invObj = ((Player*)localObj)->GetInventoryPlayer(env);
    JniOk(env);
    if (!invObj) {
        env->DeleteLocalRef(aimed);
        env->DeleteLocalRef(localObj);
        Sleep(4);
        return;
    }
    auto* inv = (InventoryPlayer*)invObj;
    int cur = inv->GetSlot(env);
    JniOk(env);
    int best = -1;
    int bestScore = -1;
    for (int i = 0; i < 9; i++) {
        jobject st = inv->GetStackInSlot(i, env);
        JniOk(env);
        int sc = WeaponScore(env, st, i);
        if (st) env->DeleteLocalRef(st);
        if (sc > bestScore) {
            bestScore = sc;
            best = i;
        }
    }
    if (best >= 0 && best != cur && bestScore > 0)
        inv->SetSlot(best, env);

    env->DeleteLocalRef(invObj);
    env->DeleteLocalRef(aimed);
    env->DeleteLocalRef(localObj);
    Sleep(1);
}
