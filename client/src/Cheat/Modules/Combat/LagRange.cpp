#include "pch.h"
#include "LagRange.h"

#include "SprintReset.h"
#include "../Misc/Weapons.h"
#include "../Settings.h"
#include "../../Hooks/WSA.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Classes/Timer.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"

#include <gl/GL.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>

#pragma comment(lib, "opengl32.lib")

namespace {
    constexpr double kPi = 3.14159265358979323846;

    std::mutex g_mu;
    bool       g_wasOn = false;
    bool       g_lagging = false;
    bool       g_wasSprinting = false;
    bool       g_swingPrev = false;
    int        g_lastTicks = 0;
    ULONGLONG  g_lagStart = 0;
    Vec3D      g_lastLocal{};
    Vec3D      g_lastServer{};
    Vec3D      g_lastDraw{};
    Vec3D      g_curDraw{};
    float      g_alpha = 0.f;
    bool       g_hasLocal = false;

    struct TimedPos {
        ULONGLONG ts;
        Vec3D     pos;
    };
    std::deque<TimedPos> g_hist;

    static void JniOk(JNIEnv* env) {
        if (env && env->ExceptionCheck()) env->ExceptionClear();
    }

    static ULONGLONG Now() { return GetTickCount64(); }

    static float LerpF(float a, float b, float t) { return a + (b - a) * t; }

    static void ResetDraw() {
        std::lock_guard<std::mutex> lock(g_mu);
        g_hist.clear();
        g_hasLocal = false;
        g_lagging = false;
        g_lagStart = 0;
        g_wasSprinting = false;
        g_swingPrev = false;
        g_lastTicks = 0;
        g_alpha = 0.f;
    }

    static void StopLag() {
        LagRange_Flush();
        std::lock_guard<std::mutex> lock(g_mu);
        g_lagging = false;
        g_hist.clear();
        g_lagStart = 0;
    }

    static void StartLag(const Vec3D& pos) {
        {
            std::lock_guard<std::mutex> lock(g_mu);
            g_lastServer = pos;
            g_lastDraw = pos;
            g_curDraw = pos;
            g_lagStart = Now();
            g_lagging = true;
            g_hist.clear();
            g_hist.push_back({ g_lagStart, pos });
        }
        LagRange_SetDelay(LagRangeSettings::mode == 1 ? LagRangeSettings::delay : 0);
        LagRange_Start();
    }

    static bool IsSplashPotion(JNIEnv* env, Player* local) {
        jobject stackObj = local->GetHeldItem(env);
        JniOk(env);
        if (!stackObj) return false;
        auto* stack = (ItemStack*)stackObj;
        bool potion = stack->Is("net/minecraft/item/ItemPotion", env);
        JniOk(env);
        int meta = 0;
        if (potion) {
            meta = stack->GetMetadata(env);
            JniOk(env);
        }
        env->DeleteLocalRef(stackObj);
        return potion && (meta & 16384) != 0;
    }

    static bool ShouldActivate(JNIEnv* env, Player* local, const Vec3D& me, bool lagging, const Vec3D& lastLocal) {
        if (local->GetHurtTime(env) > 0) { JniOk(env); return false; }
        JniOk(env);
        if (local->IsDead(env)) { JniOk(env); return false; }
        JniOk(env);

        jobject worldObj = Minecraft::GetTheWorld(env);
        JniOk(env);
        if (!worldObj) return false;
        auto players = ((World*)worldObj)->GetPlayerEntities(env);
        JniOk(env);
        env->DeleteLocalRef(worldObj);

        const int localId = local->GetEntityId(env);
        JniOk(env);
        const float myYaw = local->GetRotationYaw(env);
        JniOk(env);
        const float myPitch = local->GetRotationPitch(env);
        JniOk(env);

        int closestId = -1;
        double closestDist = 999.0;
        Vec3D closest{};

        for (Player* p : players) {
            if (!p) continue;
            int id = p->GetEntityId(env);
            JniOk(env);
            if (id == localId) { env->DeleteLocalRef((jobject)p); continue; }
            if (p->GetHealth(env) <= 0.f) { JniOk(env); env->DeleteLocalRef((jobject)p); continue; }
            JniOk(env);
            Vec3D pos = p->GetPos(env);
            JniOk(env);
            double dx = me.x - pos.x;
            double dy = me.y - pos.y;
            double dz = me.z - pos.z;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < closestDist) {
                closestDist = dist;
                closestId = id;
                closest = pos;
            }
            env->DeleteLocalRef((jobject)p);
        }

        if (closestId == -1) return false;
        if (closestDist < (double)LagRangeSettings::flushDistance
            || closestDist > (double)LagRangeSettings::activationDistance)
            return false;

        double dirX = closest.x - me.x;
        double dirY = (closest.y + 0.9) - (me.y + 1.62);
        double dirZ = closest.z - me.z;
        double horizontalDist = std::sqrt(dirX * dirX + dirZ * dirZ);

        double targetYaw = std::atan2(-dirX, dirZ) * 180.0 / kPi;
        double targetPitch = -std::atan2(dirY, horizontalDist) * 180.0 / kPi;

        float normalizedYaw = std::fmod(myYaw, 360.0f);
        if (normalizedYaw < 0) normalizedYaw += 360.0f;
        double normalizedTargetYaw = std::fmod(targetYaw, 360.0);
        if (normalizedTargetYaw < 0) normalizedTargetYaw += 360.0;

        double yawDiff = std::abs((double)normalizedYaw - normalizedTargetYaw);
        if (yawDiff > 180.0) yawDiff = 360.0 - yawDiff;
        double pitchDiff = std::abs((double)myPitch - targetPitch);
        double totalAngle = std::sqrt(yawDiff * yawDiff + pitchDiff * pitchDiff);
        if (totalAngle > 45.0) return false;

        if (lagging) return true;

        double distToTarget = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
        if (distToTarget < 0.001) return false;

        Vec3D velocity{
            me.x - lastLocal.x,
            me.y - lastLocal.y,
            me.z - lastLocal.z
        };
        Vec3D dirNorm{ dirX / distToTarget, dirY / distToTarget, dirZ / distToTarget };
        double approachSpeed = velocity.x * dirNorm.x + velocity.y * dirNorm.y + velocity.z * dirNorm.z;
        return approachSpeed > 0.01;
    }

    static Vec3D FeetPos(const Vec3D& p) {
        Vec3D o = p;
        if (g_GameVersion == LUNAR_1_7_10)
            o.y -= 1.62;
        return o;
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

void LagRange::Run(JNIEnv* env) {
    if (!env) return;

    if (!enabled) {
        if (g_wasOn) StopLag();
        g_wasOn = false;
        ResetDraw();
        return;
    }

    if (Blink_OutActive()) {
        if (g_lagging) StopLag();
        return;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!playerObj) {
        if (g_wasOn) StopLag();
        g_wasOn = false;
        return;
    }
    auto* local = (Player*)playerObj;
    g_wasOn = true;

    if (local->IsDead(env)) {
        JniOk(env);
        if (g_lagging) StopLag();
        env->DeleteLocalRef(playerObj);
        return;
    }
    JniOk(env);

    const int ticks = local->GetTicksExisted(env);
    JniOk(env);
    if (ticks + 5 < g_lastTicks) {
        if (g_lagging) StopLag();
    }
    g_lastTicks = ticks;
    if (ticks < 100) {
        if (g_lagging) StopLag();
        env->DeleteLocalRef(playerObj);
        return;
    }

    Vec3D currentPos = local->GetPos(env);
    JniOk(env);
    Vec3D feet = FeetPos(currentPos);

    if (!g_hasLocal) {
        g_lastLocal = currentPos;
        g_hasLocal = true;
    }

    if (IsSplashPotion(env, local)) {
        if (g_lagging) StopLag();
        g_lastLocal = currentPos;
        env->DeleteLocalRef(playerObj);
        return;
    }

    if (LagRangeSettings::onlyWeapon && !Weapons_IsHolding(env)) {
        if (g_lagging) StopLag();
        g_lastLocal = currentPos;
        env->DeleteLocalRef(playerObj);
        return;
    }

    const bool sprinting = local->IsSprinting(env);
    JniOk(env);
    if (LagRangeSettings::onlySprinting && !sprinting) {
        if (g_lagging) StopLag();
        g_lastLocal = currentPos;
        env->DeleteLocalRef(playerObj);
        return;
    }

    if (g_lagging) {
        if ((g_wasSprinting && !sprinting) || SprintReset_IsStopping()) {
            StopLag();
            g_wasSprinting = sprinting;
            g_lastLocal = currentPos;
            env->DeleteLocalRef(playerObj);
            return;
        }
        g_wasSprinting = sprinting;
    } else {
        g_wasSprinting = sprinting;
    }

    const bool swing = local->IsSwingInProgress(env);
    JniOk(env);
    if (g_lagging && swing && !g_swingPrev) {
        StopLag();
        g_swingPrev = swing;
        g_lastLocal = currentPos;
        env->DeleteLocalRef(playerObj);
        return;
    }
    g_swingPrev = swing;

    if (g_lagging && g_lagStart > 0 && (Now() - g_lagStart) > 1000) {
        StopLag();
        g_lastLocal = currentPos;
        env->DeleteLocalRef(playerObj);
        return;
    }

    if (!ShouldActivate(env, local, currentPos, g_lagging, g_lastLocal)) {
        if (g_lagging) StopLag();
        g_lastLocal = currentPos;
        env->DeleteLocalRef(playerObj);
        return;
    }

    if (!g_lagging)
        StartLag(feet);

    const ULONGLONG now = Now();
    LagRange_SetDelay(LagRangeSettings::mode == 1 ? LagRangeSettings::delay : 0);
    LagRange_Tick();

    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_lastLocal = currentPos;
        if (LagRangeSettings::mode == 1 && g_lagging) {
            g_hist.push_back({ now, feet });
            ULONGLONG cutoff = now - (ULONGLONG)(std::max)(1, LagRangeSettings::delay) * 2ull;
            while (!g_hist.empty() && g_hist.front().ts < cutoff)
                g_hist.pop_front();

            ULONGLONG releaseTime = now - (ULONGLONG)(std::max)(0, LagRangeSettings::delay);
            g_lastDraw = g_curDraw;
            Vec3D target = g_lastServer;
            bool found = false;
            for (size_t i = 1; i < g_hist.size(); i++) {
                if (g_hist[i].ts >= releaseTime) {
                    const auto& prev = g_hist[i - 1];
                    const auto& next = g_hist[i];
                    ULONGLONG dt = next.ts - prev.ts;
                    if (dt > 0) {
                        double t = (double)(releaseTime - prev.ts) / (double)dt;
                        t = (std::max)(0.0, (std::min)(1.0, t));
                        target.x = prev.pos.x + (next.pos.x - prev.pos.x) * t;
                        target.y = prev.pos.y + (next.pos.y - prev.pos.y) * t;
                        target.z = prev.pos.z + (next.pos.z - prev.pos.z) * t;
                    } else {
                        target = prev.pos;
                    }
                    found = true;
                    break;
                }
            }
            if (!found && !g_hist.empty())
                target = g_hist.back().pos;
            g_curDraw = target;
        }
    }

    env->DeleteLocalRef(playerObj);
}

void LagRange::OnRender(JNIEnv* env) {
    if (!env || !LagRangeSettings::drawBox) return;

    bool lagging;
    Vec3D lastDraw, curDraw, server;
    float alpha;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        lagging = g_lagging && enabled;
        lastDraw = g_lastDraw;
        curDraw = g_curDraw;
        server = g_lastServer;
        if (!lagging)
            g_alpha = LerpF(g_alpha, 0.f, 0.15f);
        else
            g_alpha = LerpF(g_alpha, 1.f, 0.15f);
        alpha = g_alpha;
    }
    if (alpha < 0.01f) return;

    jobject rmObj = Minecraft::GetRenderManager(env);
    if (!rmObj) return;
    auto proj = ActiveRenderInfo::GetProjection(env);
    auto mv = ActiveRenderInfo::GetModelView(env);
    if (proj.size() < 16 || mv.size() < 16) {
        env->DeleteLocalRef(rmObj);
        return;
    }
    Vec3D cam = ((RenderManager*)rmObj)->GetRenderPos(env);
    env->DeleteLocalRef(rmObj);

    Vec3D drawPos = server;
    if (LagRangeSettings::mode == 1) {
        float pt = 1.f;
        jobject timerObj = Minecraft::GetTimer(env);
        if (timerObj) {
            pt = ((Timer*)timerObj)->GetRenderPartialTicks(env);
            JniOk(env);
            env->DeleteLocalRef(timerObj);
        }
        drawPos.x = lastDraw.x + (curDraw.x - lastDraw.x) * (double)pt;
        drawPos.y = lastDraw.y + (curDraw.y - lastDraw.y) * (double)pt;
        drawPos.z = lastDraw.z + (curDraw.z - lastDraw.z) * (double)pt;
    }

    const float fx = (float)(drawPos.x - cam.x);
    const float fy = (float)(drawPos.y - cam.y);
    const float fz = (float)(drawPos.z - cam.z);
    const float minX = fx - 0.3f, maxX = fx + 0.3f;
    const float minY = fy, maxY = fy + 1.8f;
    const float minZ = fz - 0.3f, maxZ = fz + 0.3f;

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

    const float* fill = LagRangeSettings::boxColor;
    const float* line = LagRangeSettings::outlineColor;
    glColor4f(fill[0], fill[1], fill[2], fill[3] * alpha);
    DrawFilledBox(minX, minY, minZ, maxX, maxY, maxZ);
    glColor4f(line[0], line[1], line[2], line[3] * alpha);
    DrawLineBox(minX, minY, minZ, maxX, maxY, maxZ);

    glDepthMask(GL_TRUE);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}
