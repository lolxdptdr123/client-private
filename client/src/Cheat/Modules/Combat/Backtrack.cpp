#include "pch.h"
#include "Backtrack.h"

#include "AntiBot.h"
#include "SwordCheck.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"
#include "../Misc/Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/MovingObjectPosition.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Classes/Timer.h"
#include "../../../Game/Classes/AxisAlignedBB.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"
#include "../../../Cheat/Hooks/WSA.h"

#include <gl/GL.h>
#include <algorithm>
#include <cmath>
#include <mutex>

#pragma comment(lib, "opengl32.lib")

namespace {
    struct Ghost {
        bool   valid = false;
        int    entityId = -1;
        double fx = 0, fy = 0, fz = 0;
        double rx = 0, ry = 0, rz = 0;
        double rlx = 0, rly = 0, rlz = 0;
        float  hw = 0.3f;
        float  hh = 1.8f;
        float  alpha = 0.f;
        ULONGLONG until = 0;
    };

    std::mutex  g_mu;
    Ghost       g_g;
    ULONGLONG   g_lastAttack = 0;
    ULONGLONG   g_windowStart = 0;
    ULONGLONG   g_lastFlush = 0;
    bool        g_inWindow = false;
    bool        g_wasSword = false;
    bool        g_wasEnabled = false;
    bool        g_lmbPrev = false;
    bool        g_swingPrev = false;
    bool        g_clickedTick = false;
    int         g_localHtStart = 0;
    int         g_nonSprint = 0;

    static void JniOk(JNIEnv* env) {
        if (env && env->ExceptionCheck()) env->ExceptionClear();
    }

    static ULONGLONG Now() { return GetTickCount64(); }

    static double Dist3(double ax, double ay, double az, double bx, double by, double bz) {
        const double dx = ax - bx, dy = ay - by, dz = az - bz;
        return sqrt(dx * dx + dy * dy + dz * dz);
    }

    static void SetD(JNIEnv* env, jobject obj, jclass cls, const char* key, double v) {
        std::string n = Mapper::Get(key);
        if (n.empty()) n = key;
        jfieldID f = env->GetFieldID(cls, n.c_str(), "D");
        JniOk(env);
        if (f) env->SetDoubleField(obj, f, v);
        JniOk(env);
    }

    static void WritePos(JNIEnv* env, Player* p, double x, double y, double z, float hw, float hh) {
        if (!p || !env) return;
        jobject obj = (jobject)p;
        jclass cls = env->GetObjectClass(obj);
        if (!cls) return;
        SetD(env, obj, cls, "posX", x);
        SetD(env, obj, cls, "posY", y);
        SetD(env, obj, cls, "posZ", z);
        SetD(env, obj, cls, "lastTickPosX", x);
        SetD(env, obj, cls, "lastTickPosY", y);
        SetD(env, obj, cls, "lastTickPosZ", z);
        SetD(env, obj, cls, "prevPosX", x);
        SetD(env, obj, cls, "prevPosY", y);
        SetD(env, obj, cls, "prevPosZ", z);
        env->DeleteLocalRef(cls);

        p->SetMotionX(0.0, env);
        p->SetMotionY(0.0, env);
        p->SetMotionZ(0.0, env);
        JniOk(env);

        jobject bb = p->GetBoundingBox(env);
        JniOk(env);
        if (bb) {
            AxisAlignedBB_t n{
                (float)(x - hw), (float)y, (float)(z - hw),
                (float)(x + hw), (float)(y + hh), (float)(z + hw)
            };
            ((AxisAlignedBB*)bb)->SetNativeBoundingBox(n, env);
            JniOk(env);
            env->DeleteLocalRef(bb);
        }
    }

    static jobject AimedEntity(JNIEnv* env) {
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

    static bool IsPlayerEnt(JNIEnv* env, jobject ent, jobject local) {
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

    static Player* FindById(JNIEnv* env, World* world, int id) {
        if (!world || id < 0) return nullptr;
        for (Player* p : world->GetPlayerEntities(env)) {
            if (!p) continue;
            if (p->GetEntityId(env) == id) return p;
            JniOk(env);
        }
        return nullptr;
    }

    static bool HoldingPearl(JNIEnv* env, Player* lp) {
        jobject st = lp->GetHeldItem(env);
        JniOk(env);
        if (!st) return false;
        bool pearl = ((ItemStack*)st)->IsEnderPearl(env);
        JniOk(env);
        env->DeleteLocalRef(st);
        return pearl;
    }

    static int SessionMs() {
        const int mode = BacktrackSettings::mode;
        if (mode == 0)
            return (std::max)(BacktrackSettings::delayInTicks * 50, BacktrackSettings::cooldown);
        if (mode == 1)
            return BacktrackSettings::forceFlushMs >= 1001 ? 60000 : BacktrackSettings::forceFlushMs;
        int d = BacktrackSettings::maxDelay;
        if (d < BacktrackSettings::minDelay) d = BacktrackSettings::minDelay;
        return (std::max)(1, d);
    }

    static void Restore(JNIEnv* env, World* world) {
        Ghost g;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            g = g_g;
            g_g.valid = false;
            g_g.entityId = -1;
            g_g.alpha = 0.f;
        }
        g_inWindow = false;
        g_windowStart = 0;
        g_lastFlush = Now();
        Backtrack_LagFlush();
        if (!env || !world || g.entityId < 0) return;
        Player* tgt = FindById(env, world, g.entityId);
        if (!tgt) return;
        WritePos(env, tgt, g.rx, g.ry, g.rz, g.hw, g.hh);
    }

    static void Flush(JNIEnv* env, World* world) {
        Restore(env, world);
    }

    static void CaptureRealThenFreeze(JNIEnv* env, Player* tgt) {
        if (!tgt) return;
        Vec3D cur = tgt->GetPos(env);
        JniOk(env);
        std::lock_guard<std::mutex> lock(g_mu);
        if (!g_g.valid) return;
        const double d = Dist3(cur.x, cur.y, cur.z, g_g.fx, g_g.fy, g_g.fz);
        if (d > 0.002) {
            g_g.rlx = g_g.rx; g_g.rly = g_g.ry; g_g.rlz = g_g.rz;
            g_g.rx = cur.x; g_g.ry = cur.y; g_g.rz = cur.z;
        }
        WritePos(env, tgt, g_g.fx, g_g.fy, g_g.fz, g_g.hw, g_g.hh);
    }

    static void SnapshotTarget(JNIEnv* env, Player* tgt) {
        Vec3D pos = tgt->GetPos(env);
        JniOk(env);
        float hw = 0.3f, hh = 1.8f;
        jobject bb = tgt->GetBoundingBox(env);
        JniOk(env);
        if (bb) {
            AxisAlignedBB_t n = ((AxisAlignedBB*)bb)->GetNativeBoundingBox(env);
            JniOk(env);
            hw = (std::max)(0.2f, (n.maxX - n.minX) * 0.5f);
            hh = (std::max)(0.6f, n.maxY - n.minY);
            env->DeleteLocalRef(bb);
        }
        std::lock_guard<std::mutex> lock(g_mu);
        g_g.valid = true;
        g_g.entityId = tgt->GetEntityId(env);
        g_g.fx = g_g.rx = g_g.rlx = pos.x;
        g_g.fy = g_g.ry = g_g.rly = pos.y;
        g_g.fz = g_g.rz = g_g.rlz = pos.z;
        g_g.hw = hw;
        g_g.hh = hh;
        g_g.alpha = 0.f;
        g_g.until = Now() + (ULONGLONG)SessionMs();
    }

    static bool InDistBand(double d) {
        return d >= BacktrackSettings::distance && d <= BacktrackSettings::distanceMax;
    }

    static void OnAttack(JNIEnv* env, Player* local, World* world, int id) {
        if (id < 0) return;
        Player* tgt = FindById(env, world, id);
        if (!tgt) return;
        if (FriendsSettings::IsFriend(env, tgt)) return;
        if (EnemiesSettings::BlocksTarget(env, tgt)) return;
        if (AntiBot_IsBot(env, (jobject)tgt)) return;
        if (tgt->IsDead(env)) return;
        JniOk(env);

        const int mode = BacktrackSettings::mode;
        Vec3D lp = local->GetPos(env);
        Vec3D tp = tgt->GetPos(env);
        JniOk(env);
        const double dist = Dist3(lp.x, lp.y, lp.z, tp.x, tp.y, tp.z);

        if (mode == 0 && BacktrackSettings::distanceCheck && !InDistBand(dist))
            return;
        if (mode == 2 && BacktrackSettings::onlyWhenNeeded && dist > 6.0)
            return;

        const ULONGLONG now = Now();
        int curId = -1;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            curId = g_g.valid ? g_g.entityId : -1;
        }

        if (curId == id) {
            g_lastAttack = now;
            std::lock_guard<std::mutex> lock(g_mu);
            g_g.until = now + (ULONGLONG)SessionMs();
            return;
        }
        if (curId != -1 && g_lastAttack > 0 && (now - g_lastAttack) < 350)
            return;

        Flush(env, world);
        SnapshotTarget(env, tgt);
        {
            std::lock_guard<std::mutex> lock(g_mu);
            WritePos(env, tgt, g_g.fx, g_g.fy, g_g.fz, g_g.hw, g_g.hh);
        }
        g_lastAttack = now;
        g_windowStart = now;
        g_inWindow = true;
        Backtrack_LagStart(0);
        g_wasSword = SC_IsHoldingSword(env);
        g_localHtStart = local->GetHurtTime(env);
        g_nonSprint = 0;
        JniOk(env);
    }

    static void TickLag(JNIEnv* env, Player* local, World* world) {
        const int mode = BacktrackSettings::mode;
        const ULONGLONG now = Now();

        bool valid = false;
        int id = -1;
        ULONGLONG until = 0;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            valid = g_g.valid;
            id = g_g.entityId;
            until = g_g.until;
        }
        if (!valid) return;

        if (now >= until) { Flush(env, world); return; }
        if (HoldingPearl(env, local)) { Flush(env, world); return; }

        const bool sword = SC_IsHoldingSword(env);
        if (g_wasSword && !sword) { Flush(env, world); return; }
        g_wasSword = sword;

        if (mode != 1) {
            const int ht = local->GetHurtTime(env);
            JniOk(env);
            if (ht > g_localHtStart) { Flush(env, world); return; }
        }

        Player* tgt = FindById(env, world, id);
        if (!tgt || tgt->IsDead(env)) { Flush(env, world); return; }
        JniOk(env);

        CaptureRealThenFreeze(env, tgt);

        Vec3D lp = local->GetPos(env);
        JniOk(env);
        double freezeDist = 0, realDist = 0;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            freezeDist = Dist3(lp.x, lp.y, lp.z, g_g.fx, g_g.fy, g_g.fz);
            realDist = Dist3(lp.x, lp.y, lp.z, g_g.rx, g_g.ry, g_g.rz);
        }

        if (mode == 0) {
            if (g_lastAttack > 0 && (now - g_lastAttack) > (ULONGLONG)BacktrackSettings::cooldown) {
                Flush(env, world);
                return;
            }
            if (BacktrackSettings::distanceCheck && !InDistBand(freezeDist)) {
                Flush(env, world);
                return;
            }
        } else if (mode == 1) {
            if (!local->IsSprinting(env)) g_nonSprint++;
            else g_nonSprint = 0;
            JniOk(env);
            if (BacktrackSettings::onlySprinting && g_nonSprint >= 4) { Flush(env, world); return; }
            if (realDist > 10.0) { Flush(env, world); return; }
            if (BacktrackSettings::forceFlushMs < 1001 && g_lastAttack > 0 &&
                (now - g_lastAttack) > (ULONGLONG)BacktrackSettings::forceFlushMs) {
                Flush(env, world);
                return;
            }
            if (realDist < freezeDist && (freezeDist - realDist) >= 0.36) {
                Flush(env, world);
                return;
            }
        } else {
            if (BacktrackSettings::onlyWhenNeeded && realDist > 6.0) { Flush(env, world); return; }

            const int abort = BacktrackSettings::disableOn;
            bool shouldAbort = false;
            if (abort == 1 && g_lastAttack > 0 && (now - g_lastAttack) < 50)
                shouldAbort = true;
            if (abort == 2 && freezeDist <= BacktrackSettings::stopOnAttackRange)
                shouldAbort = true;
            if (abort == 3 && g_clickedTick)
                shouldAbort = true;
            if (shouldAbort) { Flush(env, world); g_clickedTick = false; return; }

            if (!BacktrackSettings::continueAtHurtTime) {
                const int tht = tgt->GetHurtTime(env);
                JniOk(env);
                if (tht > 0 && tht < BacktrackSettings::stopAtHurt) {
                    Flush(env, world);
                    g_clickedTick = false;
                    return;
                }
            }

            if (g_lastFlush > 0 && (now - g_lastFlush) < (ULONGLONG)BacktrackSettings::delayBetweenLags) {
                g_clickedTick = false;
                return;
            }

            if (!g_inWindow) {
                g_inWindow = true;
                g_windowStart = now;
                Backtrack_LagStart(0);
            }
            int delay = BacktrackSettings::maxDelay;
            if (delay < BacktrackSettings::minDelay) delay = BacktrackSettings::minDelay;
            if (g_inWindow && (now - g_windowStart) >= (ULONGLONG)delay) {
                Flush(env, world);
                g_clickedTick = false;
                return;
            }
        }

        g_clickedTick = false;
    }

    static void DrawFilledBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
        glBegin(GL_QUADS);
        glVertex3f(minX, minY, minZ); glVertex3f(maxX, minY, minZ); glVertex3f(maxX, minY, maxZ); glVertex3f(minX, minY, maxZ);
        glVertex3f(minX, maxY, minZ); glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, maxY, maxZ); glVertex3f(minX, maxY, maxZ);
        glVertex3f(minX, minY, minZ); glVertex3f(minX, maxY, minZ); glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, minY, minZ);
        glVertex3f(minX, minY, maxZ); glVertex3f(minX, maxY, maxZ); glVertex3f(maxX, maxY, maxZ); glVertex3f(maxX, minY, maxZ);
        glVertex3f(minX, minY, minZ); glVertex3f(minX, maxY, minZ); glVertex3f(minX, maxY, maxZ); glVertex3f(minX, minY, maxZ);
        glVertex3f(maxX, minY, minZ); glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, maxY, maxZ); glVertex3f(maxX, minY, maxZ);
        glEnd();
    }

    static void DrawLineBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
        glBegin(GL_LINES);
        glVertex3f(minX, minY, minZ); glVertex3f(maxX, minY, minZ);
        glVertex3f(maxX, minY, minZ); glVertex3f(maxX, minY, maxZ);
        glVertex3f(maxX, minY, maxZ); glVertex3f(minX, minY, maxZ);
        glVertex3f(minX, minY, maxZ); glVertex3f(minX, minY, minZ);
        glVertex3f(minX, maxY, minZ); glVertex3f(maxX, maxY, minZ);
        glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, maxY, maxZ);
        glVertex3f(maxX, maxY, maxZ); glVertex3f(minX, maxY, maxZ);
        glVertex3f(minX, maxY, maxZ); glVertex3f(minX, maxY, minZ);
        glVertex3f(minX, minY, minZ); glVertex3f(minX, maxY, minZ);
        glVertex3f(maxX, minY, minZ); glVertex3f(maxX, maxY, minZ);
        glVertex3f(maxX, minY, maxZ); glVertex3f(maxX, maxY, maxZ);
        glVertex3f(minX, minY, maxZ); glVertex3f(minX, maxY, maxZ);
        glEnd();
    }
}

void Backtrack::Run(JNIEnv* env) {
    if (!env) return;
    if (!enabled) {
        if (g_wasEnabled) {
            jobject worldObj = Minecraft::GetTheWorld(env);
            Flush(env, worldObj ? (World*)worldObj : nullptr);
            if (worldObj) env->DeleteLocalRef(worldObj);
        }
        g_wasEnabled = false;
        g_lmbPrev = false;
        g_swingPrev = false;
        return;
    }
    g_wasEnabled = true;
    if (Overlay::isOpen) return;

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    JniOk(env);
    if (!playerObj || !worldObj) {
        Flush(env, worldObj ? (World*)worldObj : nullptr);
        if (playerObj) env->DeleteLocalRef(playerObj);
        if (worldObj) env->DeleteLocalRef(worldObj);
        return;
    }

    auto* local = (Player*)playerObj;
    auto* world = (World*)worldObj;

    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool lmbEdge = lmb && !g_lmbPrev;
    if (lmb && !g_lmbPrev) g_clickedTick = true;
    g_lmbPrev = lmb;

    const bool swing = local->IsSwingInProgress(env);
    JniOk(env);
    const bool swingEdge = swing && !g_swingPrev;
    g_swingPrev = swing;

    if (lmbEdge || swingEdge) {
        jobject ent = AimedEntity(env);
        if (ent) {
            if (IsPlayerEnt(env, ent, playerObj)) {
                auto* p = (Player*)ent;
                if (!FriendsSettings::IsFriend(env, p) && !EnemiesSettings::BlocksTarget(env, p) && !AntiBot_IsBot(env, ent))
                    OnAttack(env, local, world, p->GetEntityId(env));
            }
            env->DeleteLocalRef(ent);
        }
    }

    TickLag(env, local, world);

    env->DeleteLocalRef(playerObj);
    env->DeleteLocalRef(worldObj);
}

void Backtrack::OnRender(JNIEnv* env) {
    if (!enabled || !env) return;

    jobject worldObj = Minecraft::GetTheWorld(env);
    if (worldObj) {
        int id = -1;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            if (g_g.valid) id = g_g.entityId;
        }
        if (id >= 0) {
            Player* tgt = FindById(env, (World*)worldObj, id);
            if (tgt) CaptureRealThenFreeze(env, tgt);
        }
        env->DeleteLocalRef(worldObj);
    }

    if (!BacktrackSettings::drawBox) return;

    Ghost g;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (!g_g.valid) return;
        g_g.alpha = g_g.alpha + (1.f - g_g.alpha) * 0.15f;
        g = g_g;
    }

    jobject rmObj = Minecraft::GetRenderManager(env);
    if (!rmObj) return;
    float partial = 0.f;
    jobject timerObj = Minecraft::GetTimer(env);
    if (timerObj) {
        partial = ((Timer*)timerObj)->GetRenderPartialTicks(env);
        env->DeleteLocalRef(timerObj);
    }
    auto proj = ActiveRenderInfo::GetProjection(env);
    auto mv = ActiveRenderInfo::GetModelView(env);
    if (proj.size() < 16 || mv.size() < 16) {
        env->DeleteLocalRef(rmObj);
        return;
    }
    Vec3D cam = ((RenderManager*)rmObj)->GetRenderPos(env);
    env->DeleteLocalRef(rmObj);

    const float x = (float)(g.rlx + (g.rx - g.rlx) * (double)partial - cam.x);
    const float y = (float)(g.rly + (g.ry - g.rly) * (double)partial - cam.y);
    const float z = (float)(g.rlz + (g.rz - g.rlz) * (double)partial - cam.z);
    const float minX = x - g.hw, maxX = x + g.hw;
    const float minY = y, maxY = y + g.hh;
    const float minZ = z - g.hw, maxZ = z + g.hw;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(proj.data());
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(mv.data());

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glLineWidth(2.f);

    const float* fc = BacktrackSettings::boxColor;
    const float* oc = BacktrackSettings::outlineColor;
    glColor4f(fc[0], fc[1], fc[2], fc[3] * g.alpha);
    DrawFilledBox(minX, minY, minZ, maxX, maxY, maxZ);
    glColor4f(oc[0], oc[1], oc[2], oc[3] * g.alpha);
    DrawLineBox(minX, minY, minZ, maxX, maxY, maxZ);

    glDepthMask(GL_TRUE);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}
