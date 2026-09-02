#include "pch.h"
#include "Esp.h"

#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Classes/Timer.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"

#include "../../../../vendors/imgui/imgui.h"
#include <gl/GL.h>
#include <algorithm>
#include <cmath>
#include <vector>
#pragma comment(lib, "opengl32.lib")

#ifndef GL_LINE_SMOOTH
#define GL_LINE_SMOOTH 0x0B20
#endif
#ifndef GL_LINE_SMOOTH_HINT
#define GL_LINE_SMOOTH_HINT 0x0C52
#endif
#ifndef GL_NICEST
#define GL_NICEST 0x1102
#endif

struct EspVec3 {
    float x, y, z;
};
struct EspVec2 {
    float x, y;
};

static void SetColor4(const float c[4], float alphaOverride = -1.f) {
    glColor4f(c[0], c[1], c[2], alphaOverride >= 0.f ? alphaOverride : c[3]);
}

static void DrawBox3D(const EspVec3 corners[8], bool filled) {
    if (filled) {
        glBegin(GL_QUADS);
        glVertex3f(corners[0].x, corners[0].y, corners[0].z);
        glVertex3f(corners[1].x, corners[1].y, corners[1].z);
        glVertex3f(corners[5].x, corners[5].y, corners[5].z);
        glVertex3f(corners[4].x, corners[4].y, corners[4].z);
        glVertex3f(corners[2].x, corners[2].y, corners[2].z);
        glVertex3f(corners[6].x, corners[6].y, corners[6].z);
        glVertex3f(corners[7].x, corners[7].y, corners[7].z);
        glVertex3f(corners[3].x, corners[3].y, corners[3].z);
        glVertex3f(corners[4].x, corners[4].y, corners[4].z);
        glVertex3f(corners[5].x, corners[5].y, corners[5].z);
        glVertex3f(corners[6].x, corners[6].y, corners[6].z);
        glVertex3f(corners[7].x, corners[7].y, corners[7].z);
        glVertex3f(corners[0].x, corners[0].y, corners[0].z);
        glVertex3f(corners[3].x, corners[3].y, corners[3].z);
        glVertex3f(corners[2].x, corners[2].y, corners[2].z);
        glVertex3f(corners[1].x, corners[1].y, corners[1].z);
        glVertex3f(corners[1].x, corners[1].y, corners[1].z);
        glVertex3f(corners[2].x, corners[2].y, corners[2].z);
        glVertex3f(corners[6].x, corners[6].y, corners[6].z);
        glVertex3f(corners[5].x, corners[5].y, corners[5].z);
        glVertex3f(corners[0].x, corners[0].y, corners[0].z);
        glVertex3f(corners[4].x, corners[4].y, corners[4].z);
        glVertex3f(corners[7].x, corners[7].y, corners[7].z);
        glVertex3f(corners[3].x, corners[3].y, corners[3].z);
        glEnd();
    } else {
        glBegin(GL_LINES);
        glVertex3f(corners[0].x, corners[0].y, corners[0].z); glVertex3f(corners[1].x, corners[1].y, corners[1].z);
        glVertex3f(corners[1].x, corners[1].y, corners[1].z); glVertex3f(corners[2].x, corners[2].y, corners[2].z);
        glVertex3f(corners[2].x, corners[2].y, corners[2].z); glVertex3f(corners[3].x, corners[3].y, corners[3].z);
        glVertex3f(corners[3].x, corners[3].y, corners[3].z); glVertex3f(corners[0].x, corners[0].y, corners[0].z);
        glVertex3f(corners[4].x, corners[4].y, corners[4].z); glVertex3f(corners[5].x, corners[5].y, corners[5].z);
        glVertex3f(corners[5].x, corners[5].y, corners[5].z); glVertex3f(corners[6].x, corners[6].y, corners[6].z);
        glVertex3f(corners[6].x, corners[6].y, corners[6].z); glVertex3f(corners[7].x, corners[7].y, corners[7].z);
        glVertex3f(corners[7].x, corners[7].y, corners[7].z); glVertex3f(corners[4].x, corners[4].y, corners[4].z);
        glVertex3f(corners[0].x, corners[0].y, corners[0].z); glVertex3f(corners[4].x, corners[4].y, corners[4].z);
        glVertex3f(corners[1].x, corners[1].y, corners[1].z); glVertex3f(corners[5].x, corners[5].y, corners[5].z);
        glVertex3f(corners[2].x, corners[2].y, corners[2].z); glVertex3f(corners[6].x, corners[6].y, corners[6].z);
        glVertex3f(corners[3].x, corners[3].y, corners[3].z); glVertex3f(corners[7].x, corners[7].y, corners[7].z);
        glEnd();
    }
}

static void DrawRect2D(float x0, float y0, float x1, float y1, bool filled) {
    if (filled) {
        glBegin(GL_QUADS);
        glVertex2f(x0, y0); glVertex2f(x1, y0); glVertex2f(x1, y1); glVertex2f(x0, y1);
        glEnd();
    } else {
        glBegin(GL_LINE_LOOP);
        glVertex2f(x0, y0); glVertex2f(x1, y0); glVertex2f(x1, y1); glVertex2f(x0, y1);
        glEnd();
    }
}

static void MakeBB(const EspVec3& pos, float height, EspVec3 out[8]) {
    const float halfW = 0.3f;
    out[0] = { pos.x - halfW, pos.y,          pos.z - halfW };
    out[1] = { pos.x + halfW, pos.y,          pos.z - halfW };
    out[2] = { pos.x + halfW, pos.y,          pos.z + halfW };
    out[3] = { pos.x - halfW, pos.y,          pos.z + halfW };
    out[4] = { pos.x - halfW, pos.y + height, pos.z - halfW };
    out[5] = { pos.x + halfW, pos.y + height, pos.z - halfW };
    out[6] = { pos.x + halfW, pos.y + height, pos.z + halfW };
    out[7] = { pos.x - halfW, pos.y + height, pos.z + halfW };
}

static bool WorldToScreenEsp(const EspVec3& world, EspVec2& screen,
    const std::vector<float>& mv, const std::vector<float>& proj, const GLint vp[4])
{
    if (mv.size() != 16 || proj.size() != 16) return false;
    Vec4 clip = Multiply(Multiply(Vec4(world.x, world.y, world.z, 1.f), mv), proj);
    if (clip.w <= 0.05f) return false;
    float ndcX = clip.x / clip.w;
    float ndcY = clip.y / clip.w;
    screen.x = (ndcX + 1.f) * 0.5f * (float)vp[2] + (float)vp[0];
    screen.y = (1.f - ndcY) * 0.5f * (float)vp[3] + (float)vp[1];
    return true;
}

static void DrawHealthBar2D(float boxX, float boxMidY, float boxHeight, float health, float maxHealth) {
    if (!EspSettings::showHealthBar || maxHealth <= 0.f) return;
    float hp = health / maxHealth;
    if (hp < 0.f) hp = 0.f;
    if (hp > 1.f) hp = 1.f;
    const float barH = boxHeight * hp;
    const float w = EspSettings::healthBarWidth;
    const float off = EspSettings::healthBarOffset;
    const float x0 = boxX - off - w;
    const float x1 = boxX - off;
    const float yTop = boxMidY - boxHeight * 0.5f;
    const float yBot = boxMidY + boxHeight * 0.5f;

    SetColor4(EspSettings::healthBarBg);
    DrawRect2D(x0, yTop, x1, yBot, true);

    float col[4];
    col[0] = EspSettings::healthBarLow[0] + (EspSettings::healthBarFull[0] - EspSettings::healthBarLow[0]) * hp;
    col[1] = EspSettings::healthBarLow[1] + (EspSettings::healthBarFull[1] - EspSettings::healthBarLow[1]) * hp;
    col[2] = EspSettings::healthBarLow[2] + (EspSettings::healthBarFull[2] - EspSettings::healthBarLow[2]) * hp;
    col[3] = EspSettings::healthBarLow[3] + (EspSettings::healthBarFull[3] - EspSettings::healthBarLow[3]) * hp;
    SetColor4(col);
    DrawRect2D(x0, yBot - barH, x1, yBot, true);

    glColor4f(0.f, 0.f, 0.f, 0.8f);
    DrawRect2D(x0, yTop, x1, yBot, false);
}

void Esp::OnRender(JNIEnv* env) {
    if (!enabled || !env) return;
    if (env->ExceptionCheck()) env->ExceptionClear();

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    jobject rmObj = Minecraft::GetRenderManager(env);
    if (!playerObj || !worldObj || !rmObj) return;

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
    auto* local = (Player*)playerObj;
    Vec3D lp = local->GetPos(env);
    Vec3D ll = local->GetLastTickPos(env);
    EspVec3 localPos{
        (float)(ll.x + (lp.x - ll.x) * (double)partial - cam.x),
        (float)(ll.y + (lp.y - ll.y) * (double)partial - cam.y),
        (float)(ll.z + (lp.z - ll.z) * (double)partial - cam.z)
    };

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(proj.data());
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(mv.data());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    const bool render3D = EspSettings::renderMode == 1 || EspSettings::renderMode == 2;
    const bool render2D = EspSettings::renderMode == 0 || EspSettings::renderMode == 2;
    const ImGuiIO& io = ImGui::GetIO();

    auto players = ((World*)worldObj)->GetPlayerEntities(env);
    for (auto* ent : players) {
        jobject e = (jobject)ent;
        if (!e) continue;
        if (env->IsSameObject(e, playerObj)) { env->DeleteLocalRef(e); continue; }
        if (ent->IsDead(env)) { env->DeleteLocalRef(e); continue; }

        const bool isFriend = FriendsSettings::IsFriend(env, ent);
        if (EspSettings::hideFriends && isFriend) { env->DeleteLocalRef(e); continue; }
        const bool isEnemy = EnemiesSettings::IsEnemy(env, ent);
        if (EspSettings::enemiesOnly && !isEnemy) { env->DeleteLocalRef(e); continue; }

        Vec3D pos = ent->GetPos(env);
        Vec3D last = ent->GetLastTickPos(env);
        EspVec3 interp{
            (float)(last.x + (pos.x - last.x) * (double)partial - cam.x),
            (float)(last.y + (pos.y - last.y) * (double)partial - cam.y),
            (float)(last.z + (pos.z - last.z) * (double)partial - cam.z)
        };

        float dx = interp.x - localPos.x;
        float dy = interp.y - localPos.y;
        float dz = interp.z - localPos.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist > EspSettings::maxRenderDistance) { env->DeleteLocalRef(e); continue; }

        float height = ent->IsSneaking(env) ? 1.54f : 1.8f;
        EspVec3 bb[8];
        MakeBB(interp, height, bb);

        const float* outline3d = isFriend ? EspSettings::friendColor : (isEnemy ? EspSettings::enemyColor : EspSettings::outline3dColor);
        const float* fill3d    = isFriend ? EspSettings::friendColor : (isEnemy ? EspSettings::enemyColor : EspSettings::fill3dColor);
        const float* outline2d = isFriend ? EspSettings::friendColor : (isEnemy ? EspSettings::enemyColor : EspSettings::outline2dColor);
        const float* fill2d    = isFriend ? EspSettings::friendColor : (isEnemy ? EspSettings::enemyColor : EspSettings::fill2dColor);

        if (render3D) {
            if (EspSettings::mode3d == 1 || EspSettings::mode3d == 2) {
                SetColor4(fill3d, EspSettings::fill3dOpacity);
                DrawBox3D(bb, true);
                glDepthMask(GL_TRUE);
            }
            if (EspSettings::mode3d == 0 || EspSettings::mode3d == 2) {
                glLineWidth(EspSettings::outline3dWidth);
                SetColor4(outline3d);
                DrawBox3D(bb, false);
            }
        }

        if (render2D) {
            EspVec3 center{ 0, 0, 0 };
            for (int i = 0; i < 8; i++) {
                center.x += bb[i].x; center.y += bb[i].y; center.z += bb[i].z;
            }
            center.x /= 8.f; center.y /= 8.f; center.z /= 8.f;

            float viewZ = mv[2] * center.x + mv[6] * center.y + mv[10] * center.z + mv[14];
            if (viewZ > -1.f) { env->DeleteLocalRef(e); continue; }

            float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
            bool any = false;
            for (int i = 0; i < 8; i++) {
                EspVec2 sp;
                if (!WorldToScreenEsp(bb[i], sp, mv, proj, vp)) continue;
                any = true;
                if (sp.x < minX) minX = sp.x;
                if (sp.x > maxX) maxX = sp.x;
                if (sp.y < minY) minY = sp.y;
                if (sp.y > maxY) maxY = sp.y;
            }
            if (any) {
                minX = (std::max)(-io.DisplaySize.x, (std::min)(minX, io.DisplaySize.x * 2.f));
                maxX = (std::max)(-io.DisplaySize.x, (std::min)(maxX, io.DisplaySize.x * 2.f));
                minY = (std::max)(-io.DisplaySize.y, (std::min)(minY, io.DisplaySize.y * 2.f));
                maxY = (std::max)(-io.DisplaySize.y, (std::min)(maxY, io.DisplaySize.y * 2.f));
                const float boxW = maxX - minX;
                const float boxH = maxY - minY;
                if (boxW > 1.f && boxH > 1.f) {
                    glMatrixMode(GL_PROJECTION);
                    glPushMatrix();
                    glLoadIdentity();
                    glOrtho(0, io.DisplaySize.x, io.DisplaySize.y, 0, -1, 1);
                    glMatrixMode(GL_MODELVIEW);
                    glPushMatrix();
                    glLoadIdentity();

                    if (EspSettings::mode2d == 1 || EspSettings::mode2d == 2) {
                        SetColor4(fill2d);
                        DrawRect2D(minX, minY, maxX, maxY, true);
                    }
                    if (EspSettings::mode2d == 0 || EspSettings::mode2d == 2) {
                        glLineWidth(EspSettings::outline2dWidth);
                        SetColor4(outline2d);
                        DrawRect2D(minX, minY, maxX, maxY, false);
                    }
                    DrawHealthBar2D(minX, minY + boxH * 0.5f, boxH, ent->GetHealth(env), 20.f);

                    glMatrixMode(GL_PROJECTION);
                    glPopMatrix();
                    glMatrixMode(GL_MODELVIEW);
                    glPopMatrix();
                }
            }
        }

        env->DeleteLocalRef(e);
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopMatrix();
    glPopAttrib();
}

void Esp::OnImGuiRender(JNIEnv* env) {
    if (!enabled || !env) return;
    if (env->ExceptionCheck()) env->ExceptionClear();

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    jobject rmObj = Minecraft::GetRenderManager(env);
    if (!playerObj || !worldObj || !rmObj) return;

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
    const ImGuiIO& io = ImGui::GetIO();
    const int sw = (int)io.DisplaySize.x;
    const int sh = (int)io.DisplaySize.y;
    if (sw < 2 || sh < 2) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    auto players = ((World*)worldObj)->GetPlayerEntities(env);
    for (auto* ent : players) {
        jobject e = (jobject)ent;
        if (!e) continue;
        if (env->IsSameObject(e, playerObj)) { env->DeleteLocalRef(e); continue; }
        if (ent->IsDead(env)) { env->DeleteLocalRef(e); continue; }

        const bool isFriend = FriendsSettings::IsFriend(env, ent);
        if (EspSettings::hideFriends && isFriend) { env->DeleteLocalRef(e); continue; }
        const bool isEnemy = EnemiesSettings::IsEnemy(env, ent);
        if (EspSettings::enemiesOnly && !isEnemy) { env->DeleteLocalRef(e); continue; }

        Vec3D pos = ent->GetPos(env);
        Vec3D last = ent->GetLastTickPos(env);
        float ix = (float)(last.x + (pos.x - last.x) * (double)partial - cam.x);
        float iy = (float)(last.y + (pos.y - last.y) * (double)partial - cam.y);
        float iz = (float)(last.z + (pos.z - last.z) * (double)partial - cam.z);

        float height = ent->IsSneaking(env) ? 1.54f : 1.8f;
        Vec2 feet, head;
        if (!WorldToScreen(Vec3{ ix, iy, iz }, feet, mv, proj, sw, sh)) { env->DeleteLocalRef(e); continue; }
        if (!WorldToScreen(Vec3{ ix, iy + height, iz }, head, mv, proj, sw, sh)) { env->DeleteLocalRef(e); continue; }
        if (!std::isfinite(feet.x) || !std::isfinite(head.y)) { env->DeleteLocalRef(e); continue; }

        float boxH = fabsf(feet.y - head.y);
        float boxW = boxH * 0.45f;
        if (boxH < 4.f) { env->DeleteLocalRef(e); continue; }

        float cx = (feet.x + head.x) * 0.5f;
        float x0 = cx - boxW * 0.5f;
        float x1 = cx + boxW * 0.5f;
        float y0 = (std::min)(head.y, feet.y);
        float y1 = (std::max)(head.y, feet.y);

        const float* col = isFriend ? EspSettings::friendColor : (isEnemy ? EspSettings::enemyColor : EspSettings::outline2dColor);
        ImU32 outline = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3]));
        ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(
            EspSettings::fill2dColor[0], EspSettings::fill2dColor[1],
            EspSettings::fill2dColor[2], EspSettings::fill2dColor[3]));

        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), fill);
        dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(0, 0, 0, 220), 0.f, 0, EspSettings::outline2dWidth + 1.f);
        dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), outline, 0.f, 0, EspSettings::outline2dWidth);

        if (EspSettings::showHealthBar) {
            float hp = ent->GetHealth(env) / 20.f;
            if (hp < 0.f) hp = 0.f;
            if (hp > 1.f) hp = 1.f;
            float bw = EspSettings::healthBarWidth;
            float bx = x0 - EspSettings::healthBarOffset - bw;
            dl->AddRectFilled(ImVec2(bx, y0), ImVec2(bx + bw, y1), ImGui::ColorConvertFloat4ToU32(ImVec4(
                EspSettings::healthBarBg[0], EspSettings::healthBarBg[1],
                EspSettings::healthBarBg[2], EspSettings::healthBarBg[3])));
            ImVec4 hc = ImVec4(
                EspSettings::healthBarLow[0] + (EspSettings::healthBarFull[0] - EspSettings::healthBarLow[0]) * hp,
                EspSettings::healthBarLow[1] + (EspSettings::healthBarFull[1] - EspSettings::healthBarLow[1]) * hp,
                EspSettings::healthBarLow[2] + (EspSettings::healthBarFull[2] - EspSettings::healthBarLow[2]) * hp,
                1.f);
            dl->AddRectFilled(ImVec2(bx, y1 - boxH * hp), ImVec2(bx + bw, y1), ImGui::ColorConvertFloat4ToU32(hc));
        }

        env->DeleteLocalRef(e);
    }
}
