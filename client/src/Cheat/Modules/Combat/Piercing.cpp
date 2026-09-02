#include "pch.h"
#include "Piercing.h"

#include "SwordCheck.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"
#include "AntiBot.h"
#include "../Misc/Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/AxisAlignedBB.h"
#include "../../../Game/Classes/MovingObjectPosition.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../../../Game/Klass.h"

#include <cmath>
#include <algorithm>

static constexpr float kBorder = 0.1f;

static jclass s_playerCls = nullptr;
static jclass s_vecCls = nullptr;
static jclass s_mopCls = nullptr;
static jmethodID s_getEyeHeight = nullptr;
static jmethodID s_vecHelper = nullptr;
static jmethodID s_mopCtor = nullptr;
static jmethodID s_rayTrace = nullptr;
static jfieldID s_hitVec = nullptr;
static jfieldID s_entityHit = nullptr;
static bool s_tried = false;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static jclass GlobalClass(JNIEnv* env, const char* mapKey) {
    std::string n = Mapper::Get(mapKey);
    if (n.empty()) return nullptr;
    Klass* k = g_Instance->FindClass(n.c_str());
    if (!k) return nullptr;
    return (jclass)env->NewGlobalRef((jclass)k);
}

static void Ensure(JNIEnv* env) {
    if (s_tried || !env) return;
    s_tried = true;

    s_playerCls = GlobalClass(env, "net/minecraft/entity/player/EntityPlayer");
    s_vecCls = GlobalClass(env, "net/minecraft/util/Vec3");
    s_mopCls = GlobalClass(env, "net/minecraft/util/MovingObjectPosition");

    if (s_playerCls) {
        s_getEyeHeight = env->GetMethodID(s_playerCls, Mapper::Get("getEyeHeight").c_str(), "()F");
        JniOk(env);
    }

    std::string vecN = Mapper::Get("net/minecraft/util/Vec3");
    if (s_vecCls && !vecN.empty()) {
        std::string helper = Mapper::Get("createVectorHelper");
        if (!helper.empty()) {
            std::string sig = "(DDD)L" + vecN + ";";
            s_vecHelper = env->GetStaticMethodID(s_vecCls, helper.c_str(), sig.c_str());
            JniOk(env);
        }
    }

    if (s_mopCls) {
        std::string sig = "(" + Mapper::Get("net/minecraft/entity/Entity", 2)
            + Mapper::Get("net/minecraft/util/Vec3", 2) + ")V";
        s_mopCtor = env->GetMethodID(s_mopCls, "<init>", sig.c_str());
        JniOk(env);
        if (!vecN.empty()) {
            s_hitVec = env->GetFieldID(s_mopCls, Mapper::Get("hitVec").c_str(), ("L" + vecN + ";").c_str());
            JniOk(env);
        }
        std::string entSig = Mapper::Get("net/minecraft/entity/Entity", 2);
        if (!entSig.empty()) {
            s_entityHit = env->GetFieldID(s_mopCls, Mapper::Get("entityHit").c_str(), entSig.c_str());
            JniOk(env);
        }
    }

    std::string mopN = Mapper::Get("net/minecraft/util/MovingObjectPosition");
    if (!vecN.empty() && !mopN.empty()) {
        std::string rsig = "(L" + vecN + ";L" + vecN + ";)L" + mopN + ";";
        std::string rt = Mapper::Get("rayTraceBlocks");
        Klass* wk = g_Instance->FindClass(Mapper::Get("net/minecraft/client/multiplayer/WorldClient").c_str());
        if (wk) {
            s_rayTrace = env->GetMethodID((jclass)wk, rt.c_str(), rsig.c_str());
            JniOk(env);
        }
        if (!s_rayTrace) {
            Klass* w = g_Instance->FindClass("net/minecraft/world/World");
            if (w) {
                s_rayTrace = env->GetMethodID((jclass)w, rt.c_str(), rsig.c_str());
                JniOk(env);
            }
        }
    }
}

static jobject MakeVec(JNIEnv* env, double x, double y, double z) {
    if (!s_vecHelper || !s_vecCls) return nullptr;
    jobject v = env->CallStaticObjectMethod(s_vecCls, s_vecHelper, x, y, z);
    JniOk(env);
    return v;
}

static double ReadD(JNIEnv* env, jobject obj, const char* key) {
    if (!obj) return 0.0;
    jclass cls = env->GetObjectClass(obj);
    if (!cls) return 0.0;
    jfieldID f = env->GetFieldID(cls, Mapper::Get(key).c_str(), "D");
    JniOk(env);
    env->DeleteLocalRef(cls);
    return f ? env->GetDoubleField(obj, f) : 0.0;
}

static bool RayAabb(double ox, double oy, double oz,
    double dx, double dy, double dz,
    double minX, double minY, double minZ,
    double maxX, double maxY, double maxZ,
    double reach, double& hitDist)
{
    if (ox >= minX && ox <= maxX && oy >= minY && oy <= maxY && oz >= minZ && oz <= maxZ) {
        hitDist = 0.0;
        return true;
    }

    double tmin = -1e30, tmax = 1e30;
    if (std::abs(dx) > 1e-10) {
        double t1 = (minX - ox) / dx, t2 = (maxX - ox) / dx;
        if (t1 > t2) std::swap(t1, t2);
        tmin = (std::max)(tmin, t1);
        tmax = (std::min)(tmax, t2);
    } else if (ox < minX || ox > maxX) return false;

    if (std::abs(dy) > 1e-10) {
        double t1 = (minY - oy) / dy, t2 = (maxY - oy) / dy;
        if (t1 > t2) std::swap(t1, t2);
        tmin = (std::max)(tmin, t1);
        tmax = (std::min)(tmax, t2);
    } else if (oy < minY || oy > maxY) return false;

    if (std::abs(dz) > 1e-10) {
        double t1 = (minZ - oz) / dz, t2 = (maxZ - oz) / dz;
        if (t1 > t2) std::swap(t1, t2);
        tmin = (std::max)(tmin, t1);
        tmax = (std::min)(tmax, t2);
    } else if (oz < minZ || oz > maxZ) return false;

    if (tmax < 0.0 || tmin > tmax) return false;
    hitDist = tmin >= 0.0 ? tmin : tmax;
    return hitDist >= 0.0 && hitDist <= reach;
}

static void ApplyPiercing(JNIEnv* env) {
    if (!env) return;
    if (Overlay::isOpen) return;
    JniOk(env);
    Ensure(env);
    if (!s_playerCls || !s_mopCtor || !s_vecHelper) return;

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    if (!playerObj || !worldObj) return;
    auto* local = (Player*)playerObj;

    if (PiercingSettings::weaponsOnly && !SC_IsHoldingSword(env)) return;

    jobject curMop = Minecraft::GetObjectMouseOver(env);
    if (curMop) {
        if (((MovingObjectPosition*)curMop)->IsAimingEntity(env)) {
            jobject hit = nullptr;
            if (s_entityHit) hit = env->GetObjectField(curMop, s_entityHit);
            JniOk(env);
            env->DeleteLocalRef(curMop);
            bool keep = false;
            if (hit && s_playerCls && env->IsInstanceOf(hit, s_playerCls)) {
                auto* hp = (Player*)hit;
                int hid = hp->GetEntityId(env);
                keep = hid != local->GetEntityId(env) && !hp->IsDead(env)
                    && !FriendsSettings::IsFriend(env, hp)
                    && (!PiercingSettings::targetEnemiesOnly || EnemiesSettings::IsEnemy(env, hp));
            }
            if (hit) env->DeleteLocalRef(hit);
            if (keep) return;
        } else {
            bool block = ((MovingObjectPosition*)curMop)->IsAimingBlock(env);
            env->DeleteLocalRef(curMop);
            if (block && !PiercingSettings::throughBlock) return;
        }
    }

    Vec3D lp = local->GetPos(env);
    float eye = 1.62f;
    if (s_getEyeHeight) {
        eye = env->CallFloatMethod(playerObj, s_getEyeHeight);
        JniOk(env);
    }
    const double ox = lp.x, oy = lp.y + (double)eye, oz = lp.z;

    float yaw = local->GetRotationYaw(env);
    float pitch = local->GetRotationPitch(env);
    float f = std::cos(-yaw * 0.017453292f - 3.14159265f);
    float f1 = std::sin(-yaw * 0.017453292f - 3.14159265f);
    float f2 = -std::cos(-pitch * 0.017453292f);
    float f3 = std::sin(-pitch * 0.017453292f);
    const double dx = (double)(f1 * f2);
    const double dy = (double)f3;
    const double dz = (double)(f * f2);

    const double reach = 3.0;
    const int myId = local->GetEntityId(env);

    auto players = ((World*)worldObj)->GetPlayerEntities(env);
    double closest = reach;
    jobject best = nullptr;

    for (auto* p : players) {
        if (!p) continue;
        if (p->GetEntityId(env) == myId) { env->DeleteLocalRef((jobject)p); continue; }
        if (p->IsDead(env)) { env->DeleteLocalRef((jobject)p); continue; }
        if (FriendsSettings::IsFriend(env, p)) { env->DeleteLocalRef((jobject)p); continue; }
        if (PiercingSettings::targetEnemiesOnly && !EnemiesSettings::IsEnemy(env, p)) { env->DeleteLocalRef((jobject)p); continue; }
        if (AntiBot_IsBot(env, (jobject)p)) { env->DeleteLocalRef((jobject)p); continue; }

        jobject bbObj = p->GetBoundingBox(env);
        if (!bbObj) { env->DeleteLocalRef((jobject)p); continue; }
        AxisAlignedBB_t bb = ((AxisAlignedBB*)bbObj)->GetNativeBoundingBox(env);
        env->DeleteLocalRef(bbObj);

        double hitDist = 0.0;
        if (!RayAabb(ox, oy, oz, dx, dy, dz,
            (double)bb.minX - kBorder, (double)bb.minY - kBorder, (double)bb.minZ - kBorder,
            (double)bb.maxX + kBorder, (double)bb.maxY + kBorder, (double)bb.maxZ + kBorder,
            reach, hitDist)) {
            env->DeleteLocalRef((jobject)p);
            continue;
        }
        if (hitDist < closest) {
            closest = hitDist;
            if (best) env->DeleteLocalRef(best);
            best = (jobject)p;
        } else {
            env->DeleteLocalRef((jobject)p);
        }
    }

    if (!best) return;

    if (!PiercingSettings::throughBlock && s_rayTrace) {
        jobject from = MakeVec(env, ox, oy, oz);
        jobject to = MakeVec(env, ox + dx * closest, oy + dy * closest, oz + dz * closest);
        if (from && to) {
            jobject blockHit = env->CallObjectMethod(worldObj, s_rayTrace, from, to);
            JniOk(env);
            if (blockHit) {
                bool blocked = ((MovingObjectPosition*)blockHit)->IsAimingBlock(env);
                if (blocked && s_hitVec) {
                    jobject hv = env->GetObjectField(blockHit, s_hitVec);
                    if (hv) {
                        double hx = ReadD(env, hv, "xCoord") - ox;
                        double hy = ReadD(env, hv, "yCoord") - oy;
                        double hz = ReadD(env, hv, "zCoord") - oz;
                        env->DeleteLocalRef(hv);
                        if (hx * hx + hy * hy + hz * hz + 1e-6 < closest * closest)
                            blocked = true;
                        else
                            blocked = false;
                    }
                }
                env->DeleteLocalRef(blockHit);
                if (blocked) {
                    env->DeleteLocalRef(from);
                    env->DeleteLocalRef(to);
                    env->DeleteLocalRef(best);
                    return;
                }
            }
        }
        if (from) env->DeleteLocalRef(from);
        if (to) env->DeleteLocalRef(to);
    }

    jobject hitVec = MakeVec(env, ox + dx * closest, oy + dy * closest, oz + dz * closest);
    if (!hitVec) { env->DeleteLocalRef(best); return; }
    jobject mop = env->NewObject(s_mopCls, s_mopCtor, best, hitVec);
    JniOk(env);
    env->DeleteLocalRef(hitVec);
    if (!mop) { env->DeleteLocalRef(best); return; }

    Minecraft::SetObjectMouseOver(env, mop);
    Minecraft::SetPointedEntity(env, best);
    env->DeleteLocalRef(mop);
    env->DeleteLocalRef(best);
}

void Piercing::Run(JNIEnv* env) {
    if (!enabled) { Sleep(20); return; }
    ApplyPiercing(env);
}

void Piercing::OnRender(JNIEnv* env) {
    if (!enabled) return;
    ApplyPiercing(env);
}
