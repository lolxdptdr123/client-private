#include "pch.h"
#include "SprintReset.h"

#include "AntiBot.h"
#include "SwordCheck.h"
#include "../Misc/Overlay.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Classes/MovingObjectPosition.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Cheat/Hack.h"

#include <algorithm>
#include <cstdlib>

enum class SrState { Idle, Wait, Stop };

static SrState   s_state = SrState::Idle;
static ULONGLONG s_waitUntil = 0;
static ULONGLONG s_stopUntil = 0;
static bool      s_swingPrev = false;
static bool      s_lmbPrev = false;
static int       s_lastTargetId = -1;
static int       s_lastTargetHt = 0;
static ULONGLONG s_lastCycle = 0;
static bool      s_weHoldFwd = false;
static bool      s_weHoldSneak = false;
static bool      s_weClearedSprint = false;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static int Jitter(int v) {
    if (!SprintResetSettings::randomize || v <= 0) return v;
    int d = (std::max)(4, v / 5);
    return v - d + (rand() % (2 * d + 1));
}

static jobject GetHitEntity(JNIEnv* env) {
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

static jobject GetFwd(JNIEnv* env) {
    jobject gs = Minecraft::GetGameSettings(env);
    JniOk(env);
    if (!gs) return nullptr;
    jobject kb = ((GameSettings*)gs)->GetKeyBindForward(env);
    env->DeleteLocalRef(gs);
    JniOk(env);
    return kb;
}

static jobject GetSneak(JNIEnv* env) {
    jobject gs = Minecraft::GetGameSettings(env);
    JniOk(env);
    if (!gs) return nullptr;
    jobject kb = ((GameSettings*)gs)->GetKeyBindSneak(env);
    env->DeleteLocalRef(gs);
    JniOk(env);
    return kb;
}

static void SetSneaking(JNIEnv* env, Player* local, bool on) {
    if (!env || !local) return;
    jclass c = env->GetObjectClass((jobject)local);
    JniOk(env);
    if (!c) return;
    std::string n = Mapper::Get("setSneaking");
    if (n.empty()) n = "setSneaking";
    jmethodID m = env->GetMethodID(c, n.c_str(), "(Z)V");
    JniOk(env);
    if (m) {
        env->CallVoidMethod((jobject)local, m, on ? JNI_TRUE : JNI_FALSE);
        JniOk(env);
    }
    env->DeleteLocalRef(c);

    jobject mi = local->GetMovementInput(env);
    JniOk(env);
    if (!mi) return;
    jclass mic = env->GetObjectClass(mi);
    JniOk(env);
    if (mic) {
        std::string sn = Mapper::Get("sneak");
        if (sn.empty()) sn = "sneak";
        jfieldID f = env->GetFieldID(mic, sn.c_str(), "Z");
        JniOk(env);
        if (f) env->SetBooleanField(mi, f, on ? JNI_TRUE : JNI_FALSE);
        JniOk(env);
        env->DeleteLocalRef(mic);
    }
    env->DeleteLocalRef(mi);
}

static void HoldStop(JNIEnv* env, Player* local) {
    if (s_weHoldFwd) {
        jobject kb = GetFwd(env);
        if (kb) {
            ((KeyBinding*)kb)->SetPressed(false, env);
            JniOk(env);
            env->DeleteLocalRef(kb);
        }
        if (local) {
            local->SetSprinting(false, env);
            JniOk(env);
        }
    }
    if (s_weHoldSneak) {
        jobject kb = GetSneak(env);
        if (kb) {
            ((KeyBinding*)kb)->SetPressed(true, env);
            JniOk(env);
            env->DeleteLocalRef(kb);
        }
        if (local) {
            SetSneaking(env, local, true);
            local->SetSprinting(false, env);
            JniOk(env);
        }
    }
    if (s_weClearedSprint && local) {
        local->SetSprinting(false, env);
        JniOk(env);
    }
}

static void Restore(JNIEnv* env, Player* local) {
    if (s_weHoldFwd) {
        jobject kb = GetFwd(env);
        if (kb) {
            bool phys = ((KeyBinding*)kb)->IsPhysDown(env);
            JniOk(env);
            ((KeyBinding*)kb)->SetPressed(phys, env);
            JniOk(env);
            env->DeleteLocalRef(kb);
        }
        s_weHoldFwd = false;
    }
    if (s_weHoldSneak) {
        bool phys = false;
        jobject kb = GetSneak(env);
        if (kb) {
            phys = ((KeyBinding*)kb)->IsPhysDown(env);
            JniOk(env);
            ((KeyBinding*)kb)->SetPressed(phys, env);
            JniOk(env);
            env->DeleteLocalRef(kb);
        }
        if (local)
            SetSneaking(env, local, phys);
        s_weHoldSneak = false;
    }
    if (s_weClearedSprint && local) {
        float fwd = local->GetMoveForward(env);
        JniOk(env);
        bool w = (GetAsyncKeyState('W') & 0x8000) != 0;
        if (fwd > 0.01f || w)
            local->SetSprinting(true, env);
        JniOk(env);
        s_weClearedSprint = false;
    }
    s_state = SrState::Idle;
}

static bool ConditionsOk(JNIEnv* env, Player* local) {
    if (SprintResetSettings::holdingWeapon && !SC_IsHoldingSword(env))
        return false;
    if (SprintResetSettings::waitForDamage) {
        int ht = local->GetHurtTime(env);
        JniOk(env);
        int hrt = local->GetHurtResistantTime(env);
        JniOk(env);
        if (ht <= 0 && hrt <= 0)
            return false;
    }
    return true;
}

static bool ValidTarget(JNIEnv* env, jobject aimed, jobject localObj) {
    if (!aimed || !IsPlayerEntity(env, aimed, localObj))
        return false;
    if (FriendsSettings::enabled && FriendsSettings::IsFriend(env, (Player*)aimed))
        return false;
    if (EnemiesSettings::BlocksTarget(env, (Player*)aimed))
        return false;
    if (AntiBot_IsBot(env, aimed))
        return false;
    return true;
}

static void BeginStop(JNIEnv* env, Player* local);

static void ArmReset(ULONGLONG now, JNIEnv* env, Player* local) {
    s_lastCycle = now;
    s_state = SrState::Wait;
    s_waitUntil = now + (ULONGLONG)(std::max)(0, Jitter(SprintResetSettings::delayMs));
    if (s_waitUntil <= now)
        BeginStop(env, local);
}

static void BeginStop(JNIEnv* env, Player* local) {
    int mode = SprintResetSettings::mode;
    if (mode == 0) {
        jobject kb = GetFwd(env);
        if (kb) {
            ((KeyBinding*)kb)->SetPressed(false, env);
            JniOk(env);
            env->DeleteLocalRef(kb);
            s_weHoldFwd = true;
        }
    } else if (mode == 1) {
        jobject kb = GetSneak(env);
        if (kb) {
            ((KeyBinding*)kb)->SetPressed(true, env);
            JniOk(env);
            env->DeleteLocalRef(kb);
            s_weHoldSneak = true;
        }
        if (local) {
            SetSneaking(env, local, true);
            local->SetSprinting(false, env);
            JniOk(env);
        }
    } else {
        local->SetSprinting(false, env);
        JniOk(env);
        s_weClearedSprint = true;
    }
    s_state = SrState::Stop;
    s_stopUntil = GetTickCount64() + (ULONGLONG)(std::max)(1, Jitter(SprintResetSettings::stopMs));
}

static void Tick(JNIEnv* env) {
    if (!env || Overlay::isOpen) {
        jobject lp = env ? Minecraft::GetThePlayer(env) : nullptr;
        Restore(env, lp ? (Player*)lp : nullptr);
        if (lp) env->DeleteLocalRef(lp);
        s_swingPrev = false;
        s_lmbPrev = false;
        return;
    }

    jobject screen = Minecraft::GetCurrentScreen(env);
    JniOk(env);
    if (screen) {
        env->DeleteLocalRef(screen);
        jobject lp = Minecraft::GetThePlayer(env);
        Restore(env, lp ? (Player*)lp : nullptr);
        if (lp) env->DeleteLocalRef(lp);
        return;
    }

    jobject localObj = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!localObj) {
        Restore(env, nullptr);
        return;
    }
    auto* local = (Player*)localObj;
    ULONGLONG now = GetTickCount64();

    if (s_state == SrState::Wait && now >= s_waitUntil)
        BeginStop(env, local);
    if (s_state == SrState::Stop && now >= s_stopUntil)
        Restore(env, local);
    else if (s_state == SrState::Stop)
        HoldStop(env, local);

    bool swinging = local->IsSwingInProgress(env);
    JniOk(env);
    bool swingStart = swinging && !s_swingPrev;
    s_swingPrev = swinging;

    bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool lmbClick = lmb && !s_lmbPrev;
    s_lmbPrev = lmb;

    if (s_state == SrState::Idle && ConditionsOk(env, local)) {
        jobject aimed = GetHitEntity(env);
        JniOk(env);
        if (ValidTarget(env, aimed, localObj)) {
            auto* t = (Player*)aimed;
            int id = t->GetEntityId(env);
            JniOk(env);
            int ht = t->GetHurtTime(env);
            JniOk(env);
            bool freshHit = (id == s_lastTargetId && ht > s_lastTargetHt && ht >= 8);
            s_lastTargetId = id;
            s_lastTargetHt = ht;

            bool due = (s_lastCycle == 0) || (now - s_lastCycle >= 400);
            if (freshHit || ((swingStart || lmbClick) && due))
                ArmReset(now, env, local);
        } else {
            s_lastTargetId = -1;
            s_lastTargetHt = 0;
        }
        if (aimed) env->DeleteLocalRef(aimed);
    }

    env->DeleteLocalRef(localObj);
}

void SprintReset::Run(JNIEnv* env) {
    if (!enabled) {
        jobject lp = env ? Minecraft::GetThePlayer(env) : nullptr;
        Restore(env, lp ? (Player*)lp : nullptr);
        if (lp) env->DeleteLocalRef(lp);
        s_swingPrev = false;
        s_lmbPrev = false;
        s_lastCycle = 0;
        return;
    }
    Tick(env);
}

void SprintReset::OnRender(JNIEnv* env) {
    if (!enabled || !env) return;
    Tick(env);
}

bool SprintReset_IsStopping() {
    return s_state == SrState::Stop;
}
