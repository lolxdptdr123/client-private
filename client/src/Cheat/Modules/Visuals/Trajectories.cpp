#include "pch.h"
#include "Trajectories.h"

#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Classes/Timer.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Classes/AxisAlignedBB.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Game/Method.h"
#include "../../../Cheat/Hack.h"

#include <gl/GL.h>
#pragma comment(lib, "opengl32.lib")
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

#ifndef GL_LINE_SMOOTH
#define GL_LINE_SMOOTH 0x0B20
#endif
#ifndef GL_LINE_STRIP
#define GL_LINE_STRIP 0x0003
#endif

enum class ProjType { None, Snowball, Egg, Bow, Pearl, Potion, Rod };

struct TrajPt { float x, y, z; };
struct EntBox { double minX, minY, minZ, maxX, maxY, maxZ; };

static constexpr float kDeg2Rad = 3.14159265358979323846f / 180.f;
static constexpr int   kMaxSteps = 300;
static constexpr int   kSkipTicks = 1;

static jclass s_playerCls = nullptr;
static jclass s_vecCls = nullptr;
static jmethodID s_vecCtor = nullptr;
static jmethodID s_vecHelper = nullptr;
static jmethodID s_rayTrace = nullptr;
static jmethodID s_isUsingItem = nullptr;
static jmethodID s_getEyeHeight = nullptr;
static jfieldID s_itemInUseCount = nullptr;
static jfieldID s_hitVec = nullptr;
static jfieldID s_typeOfHit = nullptr;
static jobject s_blockHitType = nullptr;
static bool s_jniTried = false;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static void EnsurePlayerClass(JNIEnv* env) {
    if (s_playerCls || !env) return;
    std::string n = Mapper::Get("net/minecraft/entity/player/EntityPlayer");
    if (n.empty()) return;
    Klass* k = g_Instance->FindClass(n.c_str());
    if (!k) return;
    s_playerCls = (jclass)env->NewGlobalRef((jclass)k);
}

static void EnsureJni(JNIEnv* env) {
    if (s_jniTried || !env) return;
    s_jniTried = true;

    std::string vecN = Mapper::Get("net/minecraft/util/Vec3");
    if (!vecN.empty()) {
        Klass* vk = g_Instance->FindClass(vecN.c_str());
        if (vk) {
            s_vecCls = (jclass)env->NewGlobalRef((jclass)vk);
            s_vecCtor = env->GetMethodID((jclass)vk, "<init>", "(DDD)V");
            JniOk(env);
            std::string helper = Mapper::Get("createVectorHelper");
            if (!helper.empty()) {
                std::string sig = "(DDD)L" + vecN + ";";
                s_vecHelper = env->GetStaticMethodID((jclass)vk, helper.c_str(), sig.c_str());
                JniOk(env);
            }
        }
    }

    std::string worldN = Mapper::Get("net/minecraft/client/multiplayer/WorldClient");
    std::string mopN = Mapper::Get("net/minecraft/util/MovingObjectPosition");
    if (!vecN.empty() && !mopN.empty()) {
        std::string sig = "(L" + vecN + ";L" + vecN + ";)L" + mopN + ";";
        std::string rt = Mapper::Get("rayTraceBlocks");
        if (!worldN.empty()) {
            Klass* wk = g_Instance->FindClass(worldN.c_str());
            if (wk) {
                s_rayTrace = env->GetMethodID((jclass)wk, rt.c_str(), sig.c_str());
                JniOk(env);
            }
        }
        if (!s_rayTrace) {
            Klass* wk = g_Instance->FindClass(Mapper::Get("net/minecraft/world/World").c_str());
            if (wk) {
                s_rayTrace = env->GetMethodID((jclass)wk, rt.c_str(), sig.c_str());
                JniOk(env);
            }
        }
    }

    if (s_playerCls) {
        s_isUsingItem = env->GetMethodID(s_playerCls, Mapper::Get("isUsingItem").c_str(), "()Z");
        JniOk(env);
        s_getEyeHeight = env->GetMethodID(s_playerCls, Mapper::Get("getEyeHeight").c_str(), "()F");
        JniOk(env);
        s_itemInUseCount = env->GetFieldID(s_playerCls, Mapper::Get("itemInUseCount").c_str(), "I");
        JniOk(env);
    }

    if (!mopN.empty()) {
        Klass* mk = g_Instance->FindClass(mopN.c_str());
        if (mk) {
            std::string typeSig = Mapper::Get("net/minecraft/util/MovingObjectPosition$MovingObjectType", 2);
            s_typeOfHit = env->GetFieldID((jclass)mk, Mapper::Get("typeOfHit").c_str(), typeSig.c_str());
            JniOk(env);
            std::string hitSig = "L" + vecN + ";";
            s_hitVec = env->GetFieldID((jclass)mk, Mapper::Get("hitVec").c_str(), hitSig.c_str());
            JniOk(env);
        }
        std::string enumN = Mapper::Get("net/minecraft/util/MovingObjectPosition$MovingObjectType");
        if (!enumN.empty()) {
            Klass* ek = g_Instance->FindClass(enumN.c_str());
            if (ek) {
                std::string esig = "L" + enumN + ";";
                jfieldID blk = env->GetStaticFieldID((jclass)ek, Mapper::Get("BLOCK").c_str(), esig.c_str());
                JniOk(env);
                if (blk) {
                    jobject o = env->GetStaticObjectField((jclass)ek, blk);
                    if (o) s_blockHitType = env->NewGlobalRef(o);
                    JniOk(env);
                }
            }
        }
    }
}

static jobject MakeVec3(JNIEnv* env, double x, double y, double z) {
    if (!s_vecCls) return nullptr;
    if (s_vecCtor) {
        jobject v = env->NewObject(s_vecCls, s_vecCtor, x, y, z);
        JniOk(env);
        if (v) return v;
    }
    if (s_vecHelper) {
        jobject v = env->CallStaticObjectMethod(s_vecCls, s_vecHelper, x, y, z);
        JniOk(env);
        return v;
    }
    return nullptr;
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

static float BowCharge(int remaining) {
    if (remaining <= 0) return 1.f;
    int chargeTime = 72000 - remaining;
    float f = (float)chargeTime / 20.f;
    f = (f * f + f * 2.f) / 3.f;
    if (f < 0.f) f = 0.f;
    if (f > 1.f) f = 1.f;
    return f;
}

static bool RayAabb(double ox, double oy, double oz, double dx, double dy, double dz,
    double minX, double minY, double minZ, double maxX, double maxY, double maxZ, double& tOut)
{
    double tmin = 0.0, tmax = 1.0;
    auto slab = [&](double o, double d, double lo, double hi) -> bool {
        if (std::abs(d) < 1e-9) return (o >= lo && o <= hi);
        double t1 = (lo - o) / d, t2 = (hi - o) / d;
        if (t1 > t2) std::swap(t1, t2);
        tmin = (std::max)(tmin, t1);
        tmax = (std::min)(tmax, t2);
        return tmin <= tmax;
    };
    if (!slab(ox, dx, minX, maxX)) return false;
    if (!slab(oy, dy, minY, maxY)) return false;
    if (!slab(oz, dz, minZ, maxZ)) return false;
    tOut = tmin;
    return true;
}

static jclass s_potionItemCls = nullptr;
static jmethodID s_isSplash = nullptr;

static void EnsurePotionClass(JNIEnv* env) {
    if (s_potionItemCls || !env) return;
    std::string n = Mapper::Get("net/minecraft/item/ItemPotion");
    if (n.empty()) n = "net/minecraft/item/ItemPotion";
    Klass* k = g_Instance->FindClass(n.c_str());
    if (!k) return;
    s_potionItemCls = (jclass)env->NewGlobalRef((jclass)k);
    s_isSplash = env->GetStaticMethodID((jclass)k, Mapper::Get("isSplash").c_str(), "(I)Z");
    JniOk(env);
}

static int ReadStackDamage(JNIEnv* env, jobject stack) {
    if (!stack) return 0;
    int meta = 0;
    jclass cls = env->GetObjectClass(stack);
    if (!cls) return 0;

    auto tryField = [&](const char* name) {
        jfieldID f = env->GetFieldID(cls, name, "I");
        JniOk(env);
        if (!f) return;
        int v = env->GetIntField(stack, f);
        JniOk(env);
        meta |= v;
    };
    tryField(Mapper::Get("metadata").c_str());
    tryField("itemDamage");
    tryField("damage");

    auto tryMethod = [&](const char* name) {
        jmethodID m = env->GetMethodID(cls, name, "()I");
        JniOk(env);
        if (!m) return;
        int v = env->CallIntMethod(stack, m);
        JniOk(env);
        meta |= v;
    };
    tryMethod(Mapper::Get("getItemDamage").c_str());
    tryMethod("getItemDamage");
    tryMethod("getMetadata");

    env->DeleteLocalRef(cls);
    return meta;
}

static bool IsSplashPotion(JNIEnv* env, ItemStack* held, int meta) {
    if (meta & 16384) return true;
    if (s_isSplash && s_potionItemCls) {
        jboolean v = env->CallStaticBooleanMethod(s_potionItemCls, s_isSplash, meta);
        JniOk(env);
        if (v) return true;
    }
    std::string name = held->GetDisplayName(env);
    JniOk(env);
    for (char& c : name) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (name.find("splash") != std::string::npos) return true;
    if (name.find("jetable") != std::string::npos) return true;
    if (name.find("throwable") != std::string::npos) return true;
    return false;
}

static ProjType ClassifyHeld(JNIEnv* env, ItemStack* held, int& filterIdx) {
    filterIdx = -1;
    if (!held) return ProjType::None;
    EnsurePotionClass(env);

    int id = held->GetItemId(env);
    JniOk(env);

    jobject itemObj = held->GetItem(env);
    JniOk(env);
    bool isPotionItem = false;
    if (itemObj && s_potionItemCls)
        isPotionItem = env->IsInstanceOf(itemObj, s_potionItemCls) == JNI_TRUE;
    if (itemObj) env->DeleteLocalRef(itemObj);

    if (id == 261) { filterIdx = 0; return ProjType::Bow; }
    if (isPotionItem || id == 373) {
        int meta = ReadStackDamage(env, (jobject)held);
        // Splash bit 16384. If damage can't be read (0), still draw — Lunar
        // sometimes leaves itemDamage at 0 on the held stack.
        if (IsSplashPotion(env, held, meta) || meta == 0) {
            filterIdx = 1;
            return ProjType::Potion;
        }
        return ProjType::None;
    }
    if (held->IsEnderPearl(env) || id == 368) { JniOk(env); filterIdx = 2; return ProjType::Pearl; }
    if (id == 332) { filterIdx = 3; return ProjType::Snowball; }
    if (id == 344) { filterIdx = 4; return ProjType::Egg; }
    if (id == 346) { filterIdx = 5; return ProjType::Rod; }
    return ProjType::None;
}

static bool FilterOn(int idx) {
    switch (idx) {
        case 0: return TrajectoriesSettings::bow;
        case 1: return TrajectoriesSettings::potion;
        case 2: return TrajectoriesSettings::pearl;
        case 3: return TrajectoriesSettings::snowball;
        case 4: return TrajectoriesSettings::egg;
        case 5: return TrajectoriesSettings::rod;
        default: return false;
    }
}

void Trajectories::OnRender(JNIEnv* env) {
    if (!enabled || !env) return;
    JniOk(env);

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    jobject rmObj = Minecraft::GetRenderManager(env);
    if (!playerObj || !worldObj || !rmObj) return;

    jobject screen = Minecraft::GetCurrentScreen(env);
    if (screen) { env->DeleteLocalRef(screen); return; }

    EnsurePlayerClass(env);
    EnsureJni(env);

    auto* local = (Player*)playerObj;
    jobject heldObj = nullptr;
    jobject invObj = local->GetInventoryPlayer(env);
    JniOk(env);
    if (invObj) {
        int slot = ((InventoryPlayer*)invObj)->GetSlot(env);
        JniOk(env);
        heldObj = ((InventoryPlayer*)invObj)->GetStackInSlot(slot, env);
        JniOk(env);
        env->DeleteLocalRef(invObj);
    }
    if (!heldObj) {
        heldObj = local->GetHeldItem(env);
        JniOk(env);
    }
    if (!heldObj) return;

    int filterIdx = -1;
    ProjType type = ClassifyHeld(env, (ItemStack*)heldObj, filterIdx);
    env->DeleteLocalRef(heldObj);
    if (type == ProjType::None || !FilterOn(filterIdx)) return;

    float bowCharge = 1.f;
    if (type == ProjType::Bow) {
        bool usingItem = false;
        if (s_isUsingItem)
            usingItem = env->CallBooleanMethod(playerObj, s_isUsingItem);
        JniOk(env);
        if (s_isUsingItem && !usingItem) return;
        int remaining = 0;
        if (s_itemInUseCount)
            remaining = env->GetIntField(playerObj, s_itemInUseCount);
        JniOk(env);
        if (s_itemInUseCount)
            bowCharge = BowCharge(remaining);
        if (bowCharge < 0.1f) return;
    }

    float partial = 0.f;
    jobject timerObj = Minecraft::GetTimer(env);
    if (timerObj) {
        partial = ((Timer*)timerObj)->GetRenderPartialTicks(env);
        env->DeleteLocalRef(timerObj);
    }

    auto proj = ActiveRenderInfo::GetProjection(env);
    auto mv = ActiveRenderInfo::GetModelView(env);
    if (proj.size() < 16 || mv.size() < 16) return;

    Vec3D cam = ((RenderManager*)rmObj)->GetRenderPos(env);
    Vec3D lp = local->GetPos(env);
    Vec3D ll = local->GetLastTickPos(env);

    float yaw = local->GetPrevRotationYaw(env) + (local->GetRotationYaw(env) - local->GetPrevRotationYaw(env)) * partial;
    float pitch = local->GetPrevRotationPitch(env) + (local->GetRotationPitch(env) - local->GetPrevRotationPitch(env)) * partial;
    float yawR = yaw * kDeg2Rad;
    float pitchR = pitch * kDeg2Rad;

    float eye = 1.62f;
    if (s_getEyeHeight) {
        eye = env->CallFloatMethod(playerObj, s_getEyeHeight);
        JniOk(env);
    } else if (local->IsSneaking(env)) {
        eye = 1.54f;
    }

    double posX = ll.x + (lp.x - ll.x) * (double)partial;
    double posY = ll.y + (lp.y - ll.y) * (double)partial + (double)eye;
    double posZ = ll.z + (lp.z - ll.z) * (double)partial;
    posX -= (double)std::cos(yawR) * 0.16;
    posY -= 0.10000000149011612;
    posZ -= (double)std::sin(yawR) * 0.16;

    float pitchOffset = (type == ProjType::Potion) ? -20.f : 0.f;
    double rawX = (double)(-std::sin(yawR) * std::cos(pitchR));
    double rawZ = (double)( std::cos(yawR) * std::cos(pitchR));
    double rawY = (double)(-std::sin((pitch + pitchOffset) * kDeg2Rad));
    double len = std::sqrt(rawX * rawX + rawY * rawY + rawZ * rawZ);
    if (len < 1e-6) return;

    double speed = 1.5;
    if (type == ProjType::Potion) speed = 0.5;
    else if (type == ProjType::Bow) speed = (double)bowCharge * 2.0 * 1.5;

    double vX = (rawX / len) * speed;
    double vY = (rawY / len) * speed;
    double vZ = (rawZ / len) * speed;

    double gravity = 0.03, drag = 0.99;
    if (type == ProjType::Potion || type == ProjType::Bow) gravity = 0.05;
    else if (type == ProjType::Rod) { gravity = 0.03999999910593033; drag = 0.92; }

    std::vector<EntBox> entityBoxes;
    if (s_playerCls) {
        auto entities = ((World*)worldObj)->GetLoadedEntities(env);
        int n = 0;
        for (jobject e : entities) {
            if (!e) continue;
            if (++n > 200) { env->DeleteLocalRef(e); continue; }
            if (!env->IsInstanceOf(e, s_playerCls)) { env->DeleteLocalRef(e); continue; }
            if (env->IsSameObject(e, playerObj)) { env->DeleteLocalRef(e); continue; }
            auto* ent = (Player*)e;
            if (ent->IsDead(env)) { env->DeleteLocalRef(e); continue; }

            Vec3D ep = ent->GetPos(env);
            Vec3D el = ent->GetLastTickPos(env);
            double ex = el.x + (ep.x - el.x) * (double)partial;
            double ey = el.y + (ep.y - el.y) * (double)partial;
            double ez = el.z + (ep.z - el.z) * (double)partial;

            double halfW = 0.3, height = 1.8;
            jobject bbObj = ent->GetBoundingBox(env);
            JniOk(env);
            if (bbObj) {
                AxisAlignedBB_t bb = ((AxisAlignedBB*)bbObj)->GetNativeBoundingBox(env);
                halfW = (bb.maxX - bb.minX) * 0.5;
                height = bb.maxY - bb.minY;
                env->DeleteLocalRef(bbObj);
            }
            entityBoxes.push_back({ ex - halfW, ey, ez - halfW, ex + halfW, ey + height, ez + halfW });
            env->DeleteLocalRef(e);
        }
    }

    std::vector<TrajPt> points;
    points.reserve(kMaxSteps);
    bool hitBlock = false;
    bool hitEntity = false;
    int hitEntityIdx = -1;

    for (int i = 0; i < kMaxSteps; i++) {
        if (i >= kSkipTicks) {
            points.push_back({
                (float)(posX - cam.x),
                (float)(posY - cam.y),
                (float)(posZ - cam.z)
            });
        }

        double nextX = posX + vX, nextY = posY + vY, nextZ = posZ + vZ;
        double segDx = nextX - posX, segDy = nextY - posY, segDz = nextZ - posZ;

        double bestT = 1e9;
        int bestIdx = -1;
        for (int e = 0; e < (int)entityBoxes.size(); e++) {
            const auto& eb = entityBoxes[e];
            double t;
            if (RayAabb(posX, posY, posZ, segDx, segDy, segDz,
                eb.minX, eb.minY, eb.minZ, eb.maxX, eb.maxY, eb.maxZ, t) && t < bestT) {
                bestT = t; bestIdx = e;
            }
        }
        if (bestIdx >= 0) {
            points.push_back({
                (float)(posX + segDx * bestT - cam.x),
                (float)(posY + segDy * bestT - cam.y),
                (float)(posZ + segDz * bestT - cam.z)
            });
            hitEntity = true;
            hitEntityIdx = bestIdx;
            break;
        }

        if (s_rayTrace) {
            jobject from = MakeVec3(env, posX, posY, posZ);
            jobject to = MakeVec3(env, nextX, nextY, nextZ);
            if (from && to) {
                jobject hit = env->CallObjectMethod(worldObj, s_rayTrace, from, to);
                JniOk(env);
                bool isBlock = false;
                if (hit && s_typeOfHit && s_blockHitType) {
                    jobject th = env->GetObjectField(hit, s_typeOfHit);
                    JniOk(env);
                    if (th) {
                        isBlock = env->IsSameObject(th, s_blockHitType) == JNI_TRUE;
                        env->DeleteLocalRef(th);
                    }
                } else if (hit && s_hitVec) {
                    isBlock = true;
                }
                if (isBlock) {
                    double lx = nextX, ly = nextY, lz = nextZ;
                    if (s_hitVec) {
                        jobject hv = env->GetObjectField(hit, s_hitVec);
                        JniOk(env);
                        if (hv) {
                            lx = ReadD(env, hv, "xCoord");
                            ly = ReadD(env, hv, "yCoord");
                            lz = ReadD(env, hv, "zCoord");
                            env->DeleteLocalRef(hv);
                        }
                    }
                    points.push_back({
                        (float)(lx - cam.x),
                        (float)(ly - cam.y),
                        (float)(lz - cam.z)
                    });
                    hitBlock = true;
                    env->DeleteLocalRef(hit);
                    env->DeleteLocalRef(from);
                    env->DeleteLocalRef(to);
                    break;
                }
                if (hit) env->DeleteLocalRef(hit);
            }
            if (from) env->DeleteLocalRef(from);
            if (to) env->DeleteLocalRef(to);
        }

        posX = nextX; posY = nextY; posZ = nextZ;
        vX *= drag; vY *= drag; vZ *= drag;
        vY -= gravity;
        if (posY < -64.0) break;
    }

    if (points.size() < 2) return;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadMatrixf(proj.data());
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadMatrixf(mv.data());

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    const float* c = TrajectoriesSettings::arcColor;
    glLineWidth((std::max)(1.f, TrajectoriesSettings::lineWidth));
    glColor4f(c[0], c[1], c[2], c[3]);
    glBegin(GL_LINE_STRIP);
    for (const auto& p : points) glVertex3f(p.x, p.y, p.z);
    glEnd();

    if (hitBlock) {
        const auto& lpnt = points.back();
        constexpr float s = 0.35f;
        glColor4f(c[0], c[1], c[2], c[3] * 0.4f);
        glBegin(GL_QUADS);
        glVertex3f(lpnt.x - s, lpnt.y + 0.01f, lpnt.z - s);
        glVertex3f(lpnt.x + s, lpnt.y + 0.01f, lpnt.z - s);
        glVertex3f(lpnt.x + s, lpnt.y + 0.01f, lpnt.z + s);
        glVertex3f(lpnt.x - s, lpnt.y + 0.01f, lpnt.z + s);
        glEnd();
        glLineWidth(TrajectoriesSettings::lineWidth + 1.f);
        glColor4f(c[0], c[1], c[2], c[3]);
        glBegin(GL_LINE_STRIP);
        glVertex3f(lpnt.x - s, lpnt.y + 0.01f, lpnt.z - s);
        glVertex3f(lpnt.x + s, lpnt.y + 0.01f, lpnt.z - s);
        glVertex3f(lpnt.x + s, lpnt.y + 0.01f, lpnt.z + s);
        glVertex3f(lpnt.x - s, lpnt.y + 0.01f, lpnt.z + s);
        glVertex3f(lpnt.x - s, lpnt.y + 0.01f, lpnt.z - s);
        glEnd();
    }

    if (hitEntity && hitEntityIdx >= 0) {
        const auto& eb = entityBoxes[hitEntityIdx];
        float x0 = (float)(eb.minX - cam.x), y0 = (float)(eb.minY - cam.y), z0 = (float)(eb.minZ - cam.z);
        float x1 = (float)(eb.maxX - cam.x), y1 = (float)(eb.maxY - cam.y), z1 = (float)(eb.maxZ - cam.z);
        glColor4f(c[0], c[1], c[2], 0.15f);
        glBegin(GL_QUADS);
        glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
        glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
        glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
        glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
        glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
        glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0);
        glEnd();
        glLineWidth(TrajectoriesSettings::lineWidth);
        glColor4f(c[0], c[1], c[2], c[3]);
        glBegin(GL_LINES);
        glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0);
        glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1);
        glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
        glVertex3f(x0, y0, z1); glVertex3f(x0, y0, z0);
        glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0);
        glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
        glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
        glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
        glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0);
        glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0);
        glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1);
        glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1);
        glEnd();
    }

    glDepthMask(GL_TRUE);
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}
