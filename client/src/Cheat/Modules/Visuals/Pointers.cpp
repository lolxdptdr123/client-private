#include "pch.h"
#include "Pointers.h"

#include "../Combat/AntiBot.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"
#include "../Misc/Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Classes/Timer.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"

#include "../../../../vendors/imgui/imgui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static ImVec4 Lerp4(const float a[4], const float b[4], float t) {
    t = (std::max)(0.f, (std::min)(1.f, t));
    return ImVec4(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t,
        a[2] + (b[2] - a[2]) * t, a[3] + (b[3] - a[3]) * t);
}

static ImVec4 NameTagColor(const std::string& name, bool isFriend, bool isEnemy) {
    if (isFriend) return ImVec4(PointersSettings::friendColor[0], PointersSettings::friendColor[1],
        PointersSettings::friendColor[2], PointersSettings::friendColor[3]);
    if (isEnemy) return ImVec4(PointersSettings::enemyColor[0], PointersSettings::enemyColor[1],
        PointersSettings::enemyColor[2], PointersSettings::enemyColor[3]);
    for (size_t i = 0; i + 1 < name.size(); i++) {
        if ((unsigned char)name[i] != 0xC2 && name[i] != '&' && name[i] != '\xA7') continue;
        char code = (char)std::tolower((unsigned char)name[i + 1]);
        if (name[i] == (char)0xC2 && i + 2 < name.size()) code = (char)std::tolower((unsigned char)name[i + 2]);
        switch (code) {
        case '0': return ImVec4(0.f, 0.f, 0.f, 1.f);
        case '1': return ImVec4(0.f, 0.f, 0.67f, 1.f);
        case '2': return ImVec4(0.f, 0.67f, 0.f, 1.f);
        case '3': return ImVec4(0.f, 0.67f, 0.67f, 1.f);
        case '4': return ImVec4(0.67f, 0.f, 0.f, 1.f);
        case '5': return ImVec4(0.67f, 0.f, 0.67f, 1.f);
        case '6': return ImVec4(1.f, 0.67f, 0.f, 1.f);
        case '7': return ImVec4(0.67f, 0.67f, 0.67f, 1.f);
        case '8': return ImVec4(0.33f, 0.33f, 0.33f, 1.f);
        case '9': return ImVec4(0.33f, 0.33f, 1.f, 1.f);
        case 'a': return ImVec4(0.33f, 1.f, 0.33f, 1.f);
        case 'b': return ImVec4(0.33f, 1.f, 1.f, 1.f);
        case 'c': return ImVec4(1.f, 0.33f, 0.33f, 1.f);
        case 'd': return ImVec4(1.f, 0.33f, 1.f, 1.f);
        case 'e': return ImVec4(1.f, 1.f, 0.33f, 1.f);
        case 'f': return ImVec4(1.f, 1.f, 1.f, 1.f);
        default: break;
        }
    }
    return ImVec4(1.f, 1.f, 1.f, 1.f);
}

static void DrawArrow(ImDrawList* dl, ImVec2 pos, float ang, float size, ImU32 col, bool style3d) {
    float c = cosf(ang), s = sinf(ang);
    ImVec2 fwd(s, -c);
    ImVec2 right(c, s);
    float depth = style3d ? 0.55f : 1.f;
    ImVec2 tip = ImVec2(pos.x + fwd.x * size, pos.y + fwd.y * size);
    ImVec2 l = ImVec2(pos.x - fwd.x * size * 0.55f * depth - right.x * size * 0.45f,
        pos.y - fwd.y * size * 0.55f * depth - right.y * size * 0.45f);
    ImVec2 r = ImVec2(pos.x - fwd.x * size * 0.55f * depth + right.x * size * 0.45f,
        pos.y - fwd.y * size * 0.55f * depth + right.y * size * 0.45f);
    dl->AddTriangleFilled(tip, l, r, col);
    dl->AddTriangle(tip, l, r, IM_COL32(0, 0, 0, 180), 1.2f);
}

void Pointers::OnImGuiRender(JNIEnv* env) {
    if (!enabled || !env) return;
    JniOk(env);

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    if (!playerObj || !worldObj) return;
    auto* local = (Player*)playerObj;

    Vec3D lp = local->GetPos(env);
    JniOk(env);
    float yaw = local->GetRotationYaw(env);
    float prevYaw = local->GetPrevRotationYaw(env);
    JniOk(env);

    float partial = 1.f;
    jobject timerObj = Minecraft::GetTimer(env);
    if (timerObj) {
        partial = ((Timer*)timerObj)->GetRenderPartialTicks(env);
        env->DeleteLocalRef(timerObj);
        JniOk(env);
    }
    float yawUse = prevYaw + (yaw - prevYaw) * partial;
    float yawRad = yawUse * 0.017453292f;

    const ImGuiIO& io = ImGui::GetIO();
    const float cx = io.DisplaySize.x * 0.5f;
    const float cy = io.DisplaySize.y * 0.5f;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) dl = ImGui::GetForegroundDrawList();
    if (!dl) return;

    auto proj = ActiveRenderInfo::GetProjection(env);
    auto mv = ActiveRenderInfo::GetModelView(env);
    jobject rmObj = Minecraft::GetRenderManager(env);
    Vec3D cam{};
    if (rmObj) {
        cam = ((RenderManager*)rmObj)->GetRenderPos(env);
        JniOk(env);
    }
    const int sw = (int)io.DisplaySize.x;
    const int sh = (int)io.DisplaySize.y;

    const float range = (std::max)(8.f, PointersSettings::range);
    const float ign = (std::max)(0.f, PointersSettings::ignoreFov);
    float nearD = (std::min)(PointersSettings::nearDist, PointersSettings::farDist);
    float farD = (std::max)(PointersSettings::nearDist, PointersSettings::farDist);
    if (farD - nearD < 1.f) farD = nearD + 1.f;

    auto players = ((World*)worldObj)->GetPlayerEntities(env);
    JniOk(env);
    for (auto* p : players) {
        if (!p) continue;
        if (env->IsSameObject((jobject)p, playerObj)) { env->DeleteLocalRef((jobject)p); continue; }
        JniOk(env);
        if (p->IsDead(env)) { JniOk(env); env->DeleteLocalRef((jobject)p); continue; }
        JniOk(env);
        if (AntiBot_IsBot(env, (jobject)p)) { env->DeleteLocalRef((jobject)p); continue; }

        bool isFriend = FriendsSettings::IsFriend(env, p);
        JniOk(env);
        if (PointersSettings::hideFriendlies && isFriend) {
            env->DeleteLocalRef((jobject)p);
            continue;
        }
        bool isEnemy = EnemiesSettings::IsEnemy(env, p);
        JniOk(env);

        Vec3D pos = p->GetPos(env);
        Vec3D last = p->GetLastTickPos(env);
        JniOk(env);
        float ix = (float)(last.x + (pos.x - last.x) * (double)partial);
        float iy = (float)(last.y + (pos.y - last.y) * (double)partial);
        float iz = (float)(last.z + (pos.z - last.z) * (double)partial);

        float dx = ix - (float)lp.x;
        float dz = iz - (float)lp.z;
        float dist = sqrtf(dx * dx + dz * dz);
        if (dist < 0.2f || dist > range) { env->DeleteLocalRef((jobject)p); continue; }

        float ang = atan2f(-dx, dz) - yawRad;
        while (ang > 3.14159265f) ang -= 6.2831853f;
        while (ang < -3.14159265f) ang += 6.2831853f;

        if (sw > 2 && sh > 2 && proj.size() == 16 && mv.size() == 16) {
            Vec2 scr{};
            Vec3 wpos{ ix - (float)cam.x, iy + 0.9f - (float)cam.y, iz - (float)cam.z };
            if (WorldToScreen(wpos, scr, mv, proj, sw, sh)) {
                float vx = scr.x - cx;
                float vy = scr.y - cy;
                if (vx * vx + vy * vy > 4.f)
                    ang = atan2f(vx, -vy);
            }
        }

        float angDeg = fabsf(ang) * 57.2957795f;
        if (ign > 0.f && angDeg < ign) { env->DeleteLocalRef((jobject)p); continue; }

        float t = (dist - nearD) / (farD - nearD);
        float rad = PointersSettings::radius;
        if (PointersSettings::distanceRadius)
            rad += t * PointersSettings::radius * 0.65f;
        float size = 10.f * PointersSettings::scale;

        ImVec2 pos2(cx + sinf(ang) * rad, cy - cosf(ang) * rad);

        ImVec4 col;
        if (PointersSettings::colorMode == 1) {
            std::string nm = p->GetName(env, false);
            JniOk(env);
            col = NameTagColor(nm, isFriend, isEnemy);
        } else if (PointersSettings::colorMode == 2) {
            const float* c = isFriend ? PointersSettings::friendColor : PointersSettings::enemyColor;
            col = ImVec4(c[0], c[1], c[2], c[3]);
        } else {
            col = Lerp4(PointersSettings::nearColor, PointersSettings::farColor, t);
        }

        DrawArrow(dl, pos2, ang, size, ImGui::ColorConvertFloat4ToU32(col), PointersSettings::style == 1);
        env->DeleteLocalRef((jobject)p);
    }
}
