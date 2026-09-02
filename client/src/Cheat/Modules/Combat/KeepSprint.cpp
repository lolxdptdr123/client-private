#include "pch.h"
#include "KeepSprint.h"

#include "SwordCheck.h"
#include "../Misc/Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/MovingObjectPosition.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Field.h"
#include "../../../Cheat/Hack.h"
#include "../../../Game/Klass.h"

#include <cmath>
#include <cstdlib>

static constexpr int kHurtCap = 10;
static constexpr ULONGLONG kHitMs = 450;

static jclass s_livingCls = nullptr;
static jclass s_mopCls = nullptr;
static jfieldID s_entityHit = nullptr;
static bool s_tried = false;

static bool s_keepHit = false;
static bool s_scaled = false;
static bool s_swingPrev = false;
static bool s_sprintPrev = false;
static ULONGLONG s_keepUntil = 0;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static void Ensure(JNIEnv* env) {
    if (s_tried || !env) return;
    s_tried = true;

    std::string living = Mapper::Get("net/minecraft/entity/EntityLivingBase");
    if (!living.empty()) {
        Klass* k = g_Instance->FindClass(living.c_str());
        if (k) s_livingCls = (jclass)env->NewGlobalRef((jclass)k);
    }

    std::string mopN = Mapper::Get("net/minecraft/util/MovingObjectPosition");
    if (!mopN.empty()) {
        Klass* k = g_Instance->FindClass(mopN.c_str());
        if (k) {
            s_mopCls = (jclass)env->NewGlobalRef((jclass)k);
            std::string entSig = Mapper::Get("net/minecraft/entity/Entity", 2);
            if (s_mopCls && !entSig.empty()) {
                s_entityHit = env->GetFieldID(s_mopCls, Mapper::Get("entityHit").c_str(), entSig.c_str());
                JniOk(env);
            }
        }
    }
}

static float Wrap360(float y) {
    while (y < 0.f) y += 360.f;
    while (y > 360.f) y -= 360.f;
    return y;
}

static bool TargetBehind(JNIEnv* env, Player* local, Player* target) {
    float a = Wrap360(local->GetRotationYaw(env));
    float b = Wrap360(target->GetRotationYaw(env));
    float diff = fabsf(a - b);
    if (diff > 180.f) diff = 360.f - diff;
    return diff < 90.f;
}

static float RetainedSpeed(JNIEnv* env, Player* local) {
    if (KeepSprintSettings::mode != 0)
        return KeepSprintSettings::speed;
    int ht = local->GetHurtTime(env);
    if (ht > 0 && ht <= kHurtCap)
        return 0.6f;
    return KeepSprintSettings::speed;
}

static jobject GetHitEntity(JNIEnv* env) {
    jobject mop = Minecraft::GetObjectMouseOver(env);
    JniOk(env);
    if (mop) {
        bool ent = ((MovingObjectPosition*)mop)->IsAimingEntity(env);
        JniOk(env);
        if (ent && s_entityHit) {
            jobject hit = env->GetObjectField(mop, s_entityHit);
            JniOk(env);
            env->DeleteLocalRef(mop);
            if (hit) return hit;
        } else {
            env->DeleteLocalRef(mop);
        }
    }

    jobject pointed = Minecraft::GetPointedEntity(env);
    JniOk(env);
    return pointed;
}

static bool IsLiving(JNIEnv* env, jobject e) {
    if (!e) return false;
    if (!s_livingCls) return true;
    bool ok = env->IsInstanceOf(e, s_livingCls) == JNI_TRUE;
    JniOk(env);
    return ok;
}

static void ForceSprint(JNIEnv* env, Player* local) {
    local->SetSprinting(true, env);
    JniOk(env);

    const auto clazz = (Klass*)env->GetObjectClass((jobject)local);
    if (!clazz) return;
    Field* f = clazz->GetField(env, Mapper::Get("sprintingTicksLeft").data(), "I");
    env->DeleteLocalRef((jclass)clazz);
    if (f) f->SetIntField(env, local, 600);
    JniOk(env);
}

static bool AllowHit(JNIEnv* env, Player* local, jobject target) {
    if (KeepSprintSettings::weaponsOnly && !SC_IsHoldingSword(env))
        return false;
    if (KeepSprintSettings::onlyOnBehind) {
        if (!target) return false;
        if (!TargetBehind(env, local, (Player*)target)) return false;
    }
    if (KeepSprintSettings::chance < 100 && (rand() % 101) > KeepSprintSettings::chance)
        return false;
    return true;
}

static void ApplyMotionKeep(JNIEnv* env, Player* local, float retained) {
    if (retained == 0.6f) return;
    double mx = local->GetMotionX(env);
    double mz = local->GetMotionZ(env);
    float k = retained / 0.6f;
    local->SetMotionX(mx * (double)k, env);
    local->SetMotionZ(mz * (double)k, env);
    JniOk(env);
}

static void ApplyKeepSprint(JNIEnv* env) {
    if (!env) return;
    if (Overlay::isOpen) return;
    JniOk(env);
    Ensure(env);

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) return;
    auto* local = (Player*)playerObj;

    const ULONGLONG now = GetTickCount64();
    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool sprint = local->IsSprinting(env);
    JniOk(env);
    const bool swinging = local->IsSwingInProgress(env);
    JniOk(env);
    const float fwd = local->GetMoveForward(env);
    JniOk(env);
    const bool wantMove = fwd > 0.01f || (GetAsyncKeyState('W') & 0x8000) != 0;

    jobject target = GetHitEntity(env);
    const bool living = IsLiving(env, target);

    const bool swingStart = swinging && !s_swingPrev;
    const bool sprintDrop = s_sprintPrev && !sprint && (swinging || lmb);

    if ((swingStart && living) || sprintDrop) {
        if (AllowHit(env, local, living ? target : nullptr)) {
            s_keepHit = true;
            s_scaled = false;
            s_keepUntil = now + kHitMs;
        }
    }

    if (s_keepHit && now > s_keepUntil)
        s_keepHit = false;

    if (s_keepHit && wantMove) {
        float retained = RetainedSpeed(env, local);
        if (retained != 0.6f) {
            if (!s_scaled && (!sprint || sprintDrop)) {
                ApplyMotionKeep(env, local, retained);
                s_scaled = true;
            }
            ForceSprint(env, local);
        }
    }

    s_swingPrev = swinging;
    s_sprintPrev = local->IsSprinting(env);
    JniOk(env);
    if (target) env->DeleteLocalRef(target);
}

void KeepSprint::Run(JNIEnv* env) {
    (void)env;
    if (!enabled) {
        s_keepHit = false;
        s_scaled = false;
        s_swingPrev = false;
        s_sprintPrev = false;
        Sleep(20);
        return;
    }
    Sleep(5);
}

void KeepSprint::OnRender(JNIEnv* env) {
    if (!enabled || !env) return;
    ApplyKeepSprint(env);
}
