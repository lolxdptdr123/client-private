#include "pch.h"
#include "Blink.h"

#include "../Settings.h"
#include "../../Hooks/WSA.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"

#include "../../../../vendors/imgui/imgui.h"
#include <gl/GL.h>
#include <algorithm>
#include <mutex>

#pragma comment(lib, "opengl32.lib")

namespace {
    std::mutex g_mu;
    bool       g_wasOn = false;
    bool       g_hasPos = false;
    bool       g_fired = false;
    ULONGLONG  g_start = 0;
    double     g_x = 0, g_y = 0, g_z = 0;
    int        g_lastHt = 0;

    static void JniOk(JNIEnv* env) {
        if (env && env->ExceptionCheck()) env->ExceptionClear();
    }

    static ULONGLONG Now() { return GetTickCount64(); }

    static void ApplyDirection() {
        const int d = BlinkSettings::direction;
        if (d == 0 || d == 2) Blink_OutStart();
        else Blink_OutFlush();
        if (d == 1 || d == 2) Blink_InStart();
        else Blink_InFlush();
    }

    static void StopBlink() {
        Blink_OutFlush();
        Blink_InFlush();
        std::lock_guard<std::mutex> lock(g_mu);
        g_hasPos = false;
        g_fired = true;
    }

    static void StartBlink(JNIEnv* env, Player* local) {
        Vec3D p = local->GetPos(env);
        JniOk(env);
        double y = p.y;
        if (g_GameVersion == LUNAR_1_7_10)
            y -= 1.62;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            g_x = p.x; g_y = y; g_z = p.z;
            g_hasPos = true;
            g_fired = false;
            g_start = Now();
        }
        g_lastHt = local->GetHurtTime(env);
        JniOk(env);
        ApplyDirection();
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

void Blink::Run(JNIEnv* env) {
    if (!env) return;

    if (!enabled) {
        if (g_wasOn) StopBlink();
        g_wasOn = false;
        return;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!playerObj) {
        if (g_wasOn) StopBlink();
        g_wasOn = false;
        return;
    }
    auto* local = (Player*)playerObj;

    if (!g_wasOn) {
        StartBlink(env, local);
        g_wasOn = true;
    }

    if (g_fired) {
        enabled = false;
        env->DeleteLocalRef(playerObj);
        return;
    }

    if (BlinkSettings::disableOnLocalDamage) {
        const int ht = local->GetHurtTime(env);
        JniOk(env);
        if (ht > 0 && ht > g_lastHt) {
            StopBlink();
            enabled = false;
            env->DeleteLocalRef(playerObj);
            return;
        }
        g_lastHt = ht;
    }

    if (BlinkSettings::disableOnTargetDamage) {
        if (local->IsSwingInProgress(env)) {
            JniOk(env);
            StopBlink();
            enabled = false;
            env->DeleteLocalRef(playerObj);
            return;
        }
        JniOk(env);
    }

    if (BlinkSettings::autoSendDelay > 0) {
        bool fired = false;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            if (Now() - g_start >= (ULONGLONG)BlinkSettings::autoSendDelay)
                fired = true;
        }
        if (fired) {
            StopBlink();
            enabled = false;
        }
    }

    env->DeleteLocalRef(playerObj);
}

void Blink::OnRender(JNIEnv* env) {
    if (!enabled || !env || !BlinkSettings::drawEsp || g_fired) return;

    double x, y, z;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (!g_hasPos) return;
        x = g_x; y = g_y; z = g_z;
    }

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

    const float fx = (float)(x - cam.x);
    const float fy = (float)(y - cam.y);
    const float fz = (float)(z - cam.z);
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

    const float* c = BlinkSettings::espColor;
    glColor4f(c[0], c[1], c[2], c[3] * 0.3f);
    DrawFilledBox(minX, minY, minZ, maxX, maxY, maxZ);
    glColor4f(c[0], c[1], c[2], c[3]);
    DrawLineBox(minX, minY, minZ, maxX, maxY, maxZ);

    glDepthMask(GL_TRUE);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}

void Blink::OnImGuiRender(JNIEnv* env) {
    (void)env;
    if (!enabled || g_fired || BlinkSettings::autoSendDelay <= 0) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    const ImGuiIO& io = ImGui::GetIO();
    ULONGLONG start;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        start = g_start;
    }
    float remaining = 1.f - (float)(Now() - start) / (float)BlinkSettings::autoSendDelay;
    remaining = std::clamp(remaining, 0.f, 1.f);

    const float barW = 560.f, barH = 18.f, round = 4.f;
    const float barX = (io.DisplaySize.x - barW) * 0.5f;
    const ImVec2 bgMin(barX, 0.f), bgMax(barX + barW, barH);
    dl->AddRectFilled(bgMin, bgMax, IM_COL32(255, 255, 255, 30), round);
    if (remaining > 0.001f) {
        const ImVec2 fillMax(barX + barW * remaining, barH);
        dl->AddRectFilled(bgMin, fillMax, IM_COL32(64, 133, 255, 255), round);
    }
}
