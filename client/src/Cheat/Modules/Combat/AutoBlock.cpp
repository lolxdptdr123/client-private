#include "pch.h"
#include "AutoBlock.h"

#include "AntiBot.h"
#include "../Misc/Overlay.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"
#include "../../Hooks/WSA.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Helper/Vec3.h"
#include "../../../Cheat/Hack.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

enum class AbState { Idle, Holding, Lagging };

static AbState   s_state = AbState::Idle;
static ULONGLONG s_holdUntil = 0;
static bool      s_weHoldUse = false;
static bool      s_weForceAnim = false;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static ULONGLONG Now() {
    return GetTickCount64();
}

static bool HoldingSword(JNIEnv* env, Player* lp) {
    jobject st = lp->GetHeldItem(env);
    JniOk(env);
    if (!st) return false;
    auto* stack = (ItemStack*)st;
    std::string sword = Mapper::Get("net/minecraft/item/ItemSword");
    bool ok = false;
    if (!sword.empty())
        ok = stack->Is(sword.c_str(), env);
    JniOk(env);
    if (!ok) {
        int id = stack->GetItemId(env);
        JniOk(env);
        ok = (id == 267 || id == 268 || id == 272 || id == 276 || id == 283);
    }
    env->DeleteLocalRef(st);
    return ok;
}

static bool PhysUseDown(JNIEnv* env) {
    jobject gs = Minecraft::GetGameSettings(env);
    JniOk(env);
    if (!gs) return false;
    jobject kb = ((GameSettings*)gs)->GetKeyBindUseItem(env);
    env->DeleteLocalRef(gs);
    JniOk(env);
    if (!kb) return false;
    bool down = ((KeyBinding*)kb)->IsPhysDown(env);
    JniOk(env);
    env->DeleteLocalRef(kb);
    return down;
}

static void SetUsePressed(JNIEnv* env, bool down) {
    jobject gs = Minecraft::GetGameSettings(env);
    JniOk(env);
    if (!gs) return;
    jobject kb = ((GameSettings*)gs)->GetKeyBindUseItem(env);
    env->DeleteLocalRef(gs);
    JniOk(env);
    if (!kb) return;
    bool phys = ((KeyBinding*)kb)->IsPhysDown(env);
    JniOk(env);
    if (down) {
        ((KeyBinding*)kb)->SetPressed(true, env);
        s_weHoldUse = true;
    } else if (!phys) {
        ((KeyBinding*)kb)->SetPressed(false, env);
        s_weHoldUse = false;
    } else {
        s_weHoldUse = false;
    }
    JniOk(env);
    env->DeleteLocalRef(kb);
}

static void SetItemInUseCount(JNIEnv* env, Player* lp, int count) {
    if (!lp || !env) return;
    jclass cls = env->GetObjectClass((jobject)lp);
    JniOk(env);
    if (!cls) return;
    std::string n = Mapper::Get("itemInUseCount");
    if (n.empty()) n = "itemInUseCount";
    jfieldID f = env->GetFieldID(cls, n.c_str(), "I");
    JniOk(env);
    if (f) env->SetIntField((jobject)lp, f, count);
    JniOk(env);
    env->DeleteLocalRef(cls);
}

static void ReleaseAll(JNIEnv* env, Player* lp) {
    if (s_weHoldUse)
        SetUsePressed(env, false);
    if (s_weForceAnim && lp) {
        SetItemInUseCount(env, lp, 0);
        s_weForceAnim = false;
    }
    AutoBlock_LagFlush();
    s_state = AbState::Idle;
    s_holdUntil = 0;
}

static bool ConditionsOk() {
    if (AutoBlockSettings::condLmb && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
        return false;
    if (AutoBlockSettings::condRmb && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000))
        return false;
    return true;
}

static bool InHurtWindow(Player* lp, JNIEnv* env) {
    int hrt = lp->GetHurtResistantTime(env);
    JniOk(env);
    int remainingMs = hrt * 50;
    return remainingMs <= AutoBlockSettings::maxHurtTimeMs;
}

static bool RecentlyDamaged(Player* lp, JNIEnv* env) {
    int ht = lp->GetHurtTime(env);
    JniOk(env);
    int hrt = lp->GetHurtResistantTime(env);
    JniOk(env);
    return ht > 0 || hrt > 0;
}

static double NearestEnemyDist(JNIEnv* env, Player* local, bool* anyInRange) {
    *anyInRange = false;
    jobject worldObj = Minecraft::GetTheWorld(env);
    JniOk(env);
    if (!worldObj) return 1e9;
    auto players = ((World*)worldObj)->GetPlayerEntities(env);
    JniOk(env);
    env->DeleteLocalRef(worldObj);

    int localId = local->GetEntityId(env);
    JniOk(env);
    Vec3D me = local->GetPos(env);
    JniOk(env);
    double best = 1e9;
    float range = AutoBlockSettings::range;

    for (Player* p : players) {
        if (!p) continue;
        if (p->GetEntityId(env) == localId) continue;
        JniOk(env);
        if (p->IsDead(env)) continue;
        JniOk(env);
        if (FriendsSettings::enabled && FriendsSettings::IsFriend(env, p)) continue;
        if (EnemiesSettings::BlocksTarget(env, p)) continue;
        if (AntiBot_IsBot(env, (jobject)p)) continue;
        Vec3D pos = p->GetPos(env);
        JniOk(env);
        double d = me.distance(pos);
        if (d < best) best = d;
        if (d <= (double)range) *anyInRange = true;
    }
    return best;
}

static bool ChanceLag() {
    int c = AutoBlockSettings::lagChance;
    if (c <= 0) return false;
    if (c >= 100) return true;
    return (rand() % 100) < c;
}

void AutoBlock::Run(JNIEnv* env) {
    if (!enabled || !env || Overlay::isOpen) {
        jobject lp = env ? Minecraft::GetThePlayer(env) : nullptr;
        ReleaseAll(env, lp ? (Player*)lp : nullptr);
        if (lp) env->DeleteLocalRef(lp);
        Sleep(15);
        return;
    }

    jobject screen = Minecraft::GetCurrentScreen(env);
    JniOk(env);
    if (screen) {
        env->DeleteLocalRef(screen);
        jobject lp = Minecraft::GetThePlayer(env);
        ReleaseAll(env, lp ? (Player*)lp : nullptr);
        if (lp) env->DeleteLocalRef(lp);
        Sleep(10);
        return;
    }

    jobject localObj = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!localObj) {
        ReleaseAll(env, nullptr);
        Sleep(10);
        return;
    }
    auto* local = (Player*)localObj;

    if (s_state == AbState::Lagging) {
        AutoBlock_LagTick();
        if (AutoBlockSettings::preventDelayAttacks && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
            AutoBlock_LagFlush();
        if (!AutoBlock_LagActive()) {
            s_state = AbState::Idle;
            if (AutoBlockSettings::blockAgainImmediately)
                s_holdUntil = 0;
        }
    }

    if (!HoldingSword(env, local)) {
        ReleaseAll(env, local);
        env->DeleteLocalRef(localObj);
        Sleep(4);
        return;
    }

    bool inRange = false;
    NearestEnemyDist(env, local, &inRange);

    if (AutoBlockSettings::forceAnim) {
        bool show = !AutoBlockSettings::forceAnimInRange || inRange;
        if (show) {
            SetItemInUseCount(env, local, 72000);
            s_weForceAnim = true;
        } else if (s_weForceAnim && s_state != AbState::Holding) {
            SetItemInUseCount(env, local, 0);
            s_weForceAnim = false;
        }
    } else if (s_weForceAnim && s_state != AbState::Holding) {
        SetItemInUseCount(env, local, 0);
        s_weForceAnim = false;
    }

    bool want = inRange && ConditionsOk() && InHurtWindow(local, env);
    if (AutoBlockSettings::condDamaged && !RecentlyDamaged(local, env))
        want = false;

    ULONGLONG now = Now();

    if (s_state == AbState::Holding) {
        if (!want || now >= s_holdUntil) {
            SetUsePressed(env, false);
            if (want && ChanceLag() && AutoBlockSettings::lagMaxMs > 0) {
                AutoBlock_LagStart(AutoBlockSettings::lagMaxMs);
                s_state = AbState::Lagging;
            } else if (want && AutoBlockSettings::blockAgainImmediately) {
                SetUsePressed(env, true);
                s_holdUntil = now + (ULONGLONG)(std::max)(1, AutoBlockSettings::maxHoldMs);
                s_state = AbState::Holding;
            } else {
                s_state = AbState::Idle;
            }
        }
        env->DeleteLocalRef(localObj);
        Sleep(1);
        return;
    }

    if (s_state == AbState::Lagging) {
        env->DeleteLocalRef(localObj);
        Sleep(1);
        return;
    }

    if (want && !PhysUseDown(env)) {
        SetUsePressed(env, true);
        s_holdUntil = now + (ULONGLONG)(std::max)(1, AutoBlockSettings::maxHoldMs);
        s_state = AbState::Holding;
    }

    env->DeleteLocalRef(localObj);
    Sleep(1);
}
