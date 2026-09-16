#include "pch.h"
#include "Reach.h"

#include "AntiBot.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"
#include "../Misc/Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/MovingObjectPosition.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../../../Game/Klass.h"

#include <jvmti.h>
#include <algorithm>
#include <cmath>

static constexpr float kBorder = 0.1f;
static constexpr double kVanillaEntityReach = 3.0;

static jclass s_playerCls = nullptr;
static jclass s_vecCls = nullptr;
static jclass s_mopCls = nullptr;
static jmethodID s_vecHelper = nullptr;
static jmethodID s_vecCtor = nullptr;
static jmethodID s_mopCtor2 = nullptr;
static jmethodID s_mopCtor1 = nullptr;
static jfieldID s_entityHit = nullptr;
static jfieldID s_typeEnum = nullptr;
static jfieldID s_typeInt = nullptr;
static jobject s_entityType = nullptr;
static jmethodID s_attackEntity = nullptr;
static bool s_tried = false;
static bool s_attackScan = false;

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

static jmethodID ScanAttackEntity(JNIEnv* env, jobject controller) {
    if (!controller) return nullptr;
    JavaVM* vm = nullptr;
    jvmtiEnv* jvmti = nullptr;
    if (env->GetJavaVM(&vm) != 0 || !vm) return nullptr;
    if (vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2) != JNI_OK || !jvmti) return nullptr;

    std::string player = Mapper::Get("net/minecraft/entity/player/EntityPlayer");
    std::string ent = Mapper::Get("net/minecraft/entity/Entity");
    jclass cls = env->GetObjectClass(controller);
    if (!cls) return nullptr;

    jmethodID best = nullptr;
    jclass walk = cls;
    for (int d = 0; walk && d < 6; d++) {
        jint nm = 0;
        jmethodID* mids = nullptr;
        if (jvmti->GetClassMethods(walk, &nm, &mids) == JVMTI_ERROR_NONE && mids) {
            for (jint m = 0; m < nm; m++) {
                char* mn = nullptr;
                char* ms = nullptr;
                if (jvmti->GetMethodName(mids[m], &mn, &ms, nullptr) != JVMTI_ERROR_NONE) continue;
                bool nameOk = mn && (strcmp(mn, "attackEntity") == 0 || strcmp(mn, "func_78764_a") == 0);
                bool sigOk = false;
                if (ms) {
                    std::string s = ms;
                    sigOk = s.size() > 6 && s.front() == '(' && s.find(")V") != std::string::npos
                        && s.find('I') == std::string::npos && s.find('Z') == std::string::npos;
                    int objs = 0;
                    for (size_t i = 1; i < s.size() && s[i] != ')'; i++) {
                        if (s[i] == 'L') {
                            objs++;
                            while (i < s.size() && s[i] != ';') i++;
                        }
                    }
                    sigOk = sigOk && objs == 2;
                    if (sigOk && !player.empty() && s.find(player) == std::string::npos)
                        sigOk = nameOk;
                    if (sigOk && !ent.empty() && s.find(ent) == std::string::npos)
                        sigOk = nameOk;
                }
                if (nameOk && ms && ms[0] == '(') {
                    best = mids[m];
                    if (mn) jvmti->Deallocate((unsigned char*)mn);
                    if (ms) jvmti->Deallocate((unsigned char*)ms);
                    jvmti->Deallocate((unsigned char*)mids);
                    if (walk != cls) env->DeleteLocalRef(walk);
                    env->DeleteLocalRef(cls);
                    return best;
                }
                if (!best && sigOk) best = mids[m];
                if (mn) jvmti->Deallocate((unsigned char*)mn);
                if (ms) jvmti->Deallocate((unsigned char*)ms);
            }
            jvmti->Deallocate((unsigned char*)mids);
        }
        jclass sup = env->GetSuperclass(walk);
        if (walk != cls) env->DeleteLocalRef(walk);
        walk = sup;
    }
    env->DeleteLocalRef(cls);
    return best;
}

static void Ensure(JNIEnv* env) {
    if (s_tried || !env) return;
    s_tried = true;

    s_playerCls = GlobalClass(env, "net/minecraft/entity/player/EntityPlayer");
    s_vecCls = GlobalClass(env, "net/minecraft/util/Vec3");
    s_mopCls = GlobalClass(env, "net/minecraft/util/MovingObjectPosition");

    std::string vecN = Mapper::Get("net/minecraft/util/Vec3");
    if (s_vecCls && !vecN.empty()) {
        s_vecCtor = env->GetMethodID(s_vecCls, "<init>", "(DDD)V");
        JniOk(env);
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
        s_mopCtor2 = env->GetMethodID(s_mopCls, "<init>", sig.c_str());
        JniOk(env);
        sig = "(" + Mapper::Get("net/minecraft/entity/Entity", 2) + ")V";
        s_mopCtor1 = env->GetMethodID(s_mopCls, "<init>", sig.c_str());
        JniOk(env);
        std::string entSig = Mapper::Get("net/minecraft/entity/Entity", 2);
        if (!entSig.empty()) {
            s_entityHit = env->GetFieldID(s_mopCls, Mapper::Get("entityHit").c_str(), entSig.c_str());
            JniOk(env);
        }
        std::string typeSig = Mapper::Get("net/minecraft/util/MovingObjectPosition$MovingObjectType", 2);
        std::string typeName = Mapper::Get("typeOfHit");
        if (!typeSig.empty()) {
            s_typeEnum = env->GetFieldID(s_mopCls, typeName.c_str(), typeSig.c_str());
            JniOk(env);
        }
        s_typeInt = env->GetFieldID(s_mopCls, typeName.c_str(), "I");
        JniOk(env);

        std::string enumN = Mapper::Get("net/minecraft/util/MovingObjectPosition$MovingObjectType");
        if (!enumN.empty()) {
            Klass* ek = g_Instance->FindClass(enumN.c_str());
            if (ek) {
                std::string esig = "L" + enumN + ";";
                const char* names[] = { "ENTITY", Mapper::Get("ENTITY").c_str() };
                for (const char* n : names) {
                    if (!n || !n[0]) continue;
                    jfieldID f = env->GetStaticFieldID((jclass)ek, n, esig.c_str());
                    JniOk(env);
                    if (!f) continue;
                    jobject o = env->GetStaticObjectField((jclass)ek, f);
                    JniOk(env);
                    if (o) {
                        if (s_entityType) env->DeleteGlobalRef(s_entityType);
                        s_entityType = env->NewGlobalRef(o);
                        env->DeleteLocalRef(o);
                        break;
                    }
                }
                if (!s_entityType) {
                    std::string vsig = "()[L" + enumN + ";";
                    jmethodID values = env->GetStaticMethodID((jclass)ek, "values", vsig.c_str());
                    JniOk(env);
                    if (values) {
                        jobjectArray arr = (jobjectArray)env->CallStaticObjectMethod((jclass)ek, values);
                        JniOk(env);
                        if (arr && env->GetArrayLength(arr) >= 3) {
                            jobject o = env->GetObjectArrayElement(arr, 2);
                            if (o) {
                                s_entityType = env->NewGlobalRef(o);
                                env->DeleteLocalRef(o);
                            }
                        }
                        if (arr) env->DeleteLocalRef(arr);
                    }
                }
            }
        }
    }
}

static jobject MakeVec(JNIEnv* env, double x, double y, double z) {
    if (!s_vecCls) return nullptr;
    if (s_vecHelper) {
        jobject v = env->CallStaticObjectMethod(s_vecCls, s_vecHelper, x, y, z);
        JniOk(env);
        if (v) return v;
    }
    if (s_vecCtor) {
        jobject v = env->NewObject(s_vecCls, s_vecCtor, x, y, z);
        JniOk(env);
        return v;
    }
    return nullptr;
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

static int  s_hitSlot = 0;
static bool s_swingPrev = false;
static bool s_lmbPrev = false;

static bool ExtraReachThisHit(JNIEnv* env, Player* local) {
    const int allow = (std::max)(1, (std::min)(10, ReachSettings::activateTicks));
    if (allow >= 10) return true;
    const bool swing = local->IsSwingInProgress(env);
    JniOk(env);
    if (swing && !s_swingPrev)
        s_hitSlot = (s_hitSlot + 1) % 10;
    s_swingPrev = swing;
    return s_hitSlot < allow;
}

static void MarkEntityMop(JNIEnv* env, jobject mop, jobject entity) {
    if (!mop) return;
    if (s_entityHit && entity)
        env->SetObjectField(mop, s_entityHit, entity);
    JniOk(env);
    if (s_typeEnum && s_entityType)
        env->SetObjectField(mop, s_typeEnum, s_entityType);
    JniOk(env);
    if (s_typeInt)
        env->SetIntField(mop, s_typeInt, 2);
    JniOk(env);
}

static jmethodID EnsureAttack(JNIEnv* env, jobject controller) {
    if (s_attackEntity) return s_attackEntity;
    if (!controller) return nullptr;

    jclass cls = env->GetObjectClass(controller);
    if (!cls) return nullptr;
    std::string pSig = Mapper::Get("net/minecraft/entity/player/EntityPlayer", 2);
    std::string eSig = Mapper::Get("net/minecraft/entity/Entity", 2);
    if (!pSig.empty() && !eSig.empty()) {
        std::string sig = "(" + pSig + eSig + ")V";
        const char* nm[] = { "attackEntity", "func_78764_a" };
        for (const char* n : nm) {
            s_attackEntity = env->GetMethodID(cls, n, sig.c_str());
            JniOk(env);
            if (s_attackEntity) {
                env->DeleteLocalRef(cls);
                return s_attackEntity;
            }
        }
        std::string sp = Mapper::Get("net/minecraft/client/entity/EntityPlayerSP", 2);
        if (!sp.empty()) {
            sig = "(" + sp + eSig + ")V";
            s_attackEntity = env->GetMethodID(cls, "attackEntity", sig.c_str());
            JniOk(env);
        }
    }
    env->DeleteLocalRef(cls);
    if (s_attackEntity) return s_attackEntity;
    if (!s_attackScan) {
        s_attackScan = true;
        s_attackEntity = ScanAttackEntity(env, controller);
    }
    return s_attackEntity;
}

static void SendAttack(JNIEnv* env, jobject local, jobject target) {
    if (!local || !target) return;
    jobject pc = Minecraft::GetPlayerController(env);
    JniOk(env);
    if (!pc) return;
    jmethodID m = EnsureAttack(env, pc);
    if (m) {
        env->CallVoidMethod(pc, m, local, target);
        JniOk(env);
    }
    env->DeleteLocalRef(pc);
}

static bool FindLookTarget(JNIEnv* env, Player* local, jobject worldObj,
    double reach, double& outDist, jobject& outEnt)
{
    outEnt = nullptr;
    outDist = reach + 1.0;

    Vec3D lp = local->GetPos(env);
    JniOk(env);
    float eye = local->GetEyeHeight(env);
    JniOk(env);
    if (eye < 0.5f) eye = 1.62f;
    const double ox = lp.x, oy = lp.y + (double)eye, oz = lp.z;

    float yaw = local->GetRotationYaw(env);
    float pitch = local->GetRotationPitch(env);
    JniOk(env);
    float cf = std::cos(-yaw * 0.017453292f - 3.14159265f);
    float sf = std::sin(-yaw * 0.017453292f - 3.14159265f);
    float cp = -std::cos(-pitch * 0.017453292f);
    float sp = std::sin(-pitch * 0.017453292f);
    const double dx = (double)(sf * cp);
    const double dy = (double)sp;
    const double dz = (double)(cf * cp);

    const int myId = local->GetEntityId(env);
    JniOk(env);

    auto players = ((World*)worldObj)->GetPlayerEntities(env);
    JniOk(env);
    jobject best = nullptr;
    double closest = reach + 0.001;

    for (auto* p : players) {
        if (!p) continue;
        if (p->GetEntityId(env) == myId) { JniOk(env); env->DeleteLocalRef((jobject)p); continue; }
        JniOk(env);
        if (p->IsDead(env)) { JniOk(env); env->DeleteLocalRef((jobject)p); continue; }
        JniOk(env);
        if (FriendsSettings::enabled && FriendsSettings::IsFriend(env, p)) {
            env->DeleteLocalRef((jobject)p);
            continue;
        }
        if (EnemiesSettings::BlocksTarget(env, p)) {
            env->DeleteLocalRef((jobject)p);
            continue;
        }
        if (AntiBot_IsBot(env, (jobject)p)) { env->DeleteLocalRef((jobject)p); continue; }

        Vec3D pos = p->GetPos(env);
        JniOk(env);
        double minX = pos.x - 0.3, maxX = pos.x + 0.3;
        double minY = pos.y, maxY = pos.y + 1.8;
        double minZ = pos.z - 0.3, maxZ = pos.z + 0.3;

        jobject bbObj = p->GetBoundingBox(env);
        JniOk(env);
        if (bbObj) {
            jclass bbCls = env->GetObjectClass(bbObj);
            auto rd = [&](const char* key) {
                jfieldID fld = bbCls ? env->GetFieldID(bbCls, Mapper::Get(key).c_str(), "D") : nullptr;
                JniOk(env);
                return fld ? env->GetDoubleField(bbObj, fld) : 0.0;
            };
            double a = rd("minX"), b = rd("minY"), c = rd("minZ");
            double d = rd("maxX"), e = rd("maxY"), f = rd("maxZ");
            if (d > a && e > b && f > c) {
                minX = a; minY = b; minZ = c;
                maxX = d; maxY = e; maxZ = f;
            }
            if (bbCls) env->DeleteLocalRef(bbCls);
            env->DeleteLocalRef(bbObj);
        }

        double hitDist = 0.0;
        if (!RayAabb(ox, oy, oz, dx, dy, dz,
            minX - kBorder, minY - kBorder, minZ - kBorder,
            maxX + kBorder, maxY + kBorder, maxZ + kBorder,
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

    outEnt = best;
    outDist = closest;
    return best != nullptr;
}

static void ApplyReach(JNIEnv* env, bool canAttack) {
    if (!env || Overlay::isOpen) return;
    JniOk(env);
    if (!s_playerCls) s_tried = false;
    Ensure(env);
    if (!s_playerCls) return;

    jobject screen = Minecraft::GetCurrentScreen(env);
    JniOk(env);
    if (screen) {
        env->DeleteLocalRef(screen);
        if (canAttack)
            s_lmbPrev = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        return;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    if (!playerObj || !worldObj) return;
    auto* local = (Player*)playerObj;

    if (ReachSettings::onlySprinting && !local->IsSprinting(env)) {
        JniOk(env);
        if (canAttack)
            s_lmbPrev = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        return;
    }
    JniOk(env);

    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool lmbEdge = false;
    if (canAttack) {
        lmbEdge = lmb && !s_lmbPrev;
        s_lmbPrev = lmb;
    }

    const double reach = (double)(std::max)(3.f, ReachSettings::distance);
    double hitDist = 0.0;
    jobject best = nullptr;
    if (!FindLookTarget(env, local, worldObj, reach, hitDist, best) || !best)
        return;

    if (hitDist <= kVanillaEntityReach) {
        env->DeleteLocalRef(best);
        return;
    }

    if (!ExtraReachThisHit(env, local)) {
        env->DeleteLocalRef(best);
        return;
    }

    if (s_mopCtor2 || s_mopCtor1) {
        jobject mop = nullptr;
        if (s_mopCtor2 && (s_vecHelper || s_vecCtor)) {
            jobject hitVec = MakeVec(env, 0, 0, 0);
            if (hitVec) {
                mop = env->NewObject(s_mopCls, s_mopCtor2, best, hitVec);
                JniOk(env);
                env->DeleteLocalRef(hitVec);
            }
        }
        if (!mop && s_mopCtor1) {
            mop = env->NewObject(s_mopCls, s_mopCtor1, best);
            JniOk(env);
        }
        if (mop) {
            MarkEntityMop(env, mop, best);
            Minecraft::SetObjectMouseOver(env, mop);
            Minecraft::SetPointedEntity(env, best);
            env->DeleteLocalRef(mop);
        }
    }

    if (canAttack && lmbEdge)
        SendAttack(env, playerObj, best);

    env->DeleteLocalRef(best);
}

void Reach::Run(JNIEnv* env) {
    if (!enabled) { Sleep(20); return; }
    ApplyReach(env, true);
}

void Reach::OnRender(JNIEnv* env) {
    if (!enabled) return;
    ApplyReach(env, false);
}
