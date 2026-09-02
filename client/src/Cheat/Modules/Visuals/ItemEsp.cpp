#include "pch.h"
#include "ItemEsp.h"

#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Classes/Timer.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Method.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"

#include "../../../../vendors/imgui/imgui.h"
#include <gl/GL.h>
#include <cmath>
#include <string>
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

struct ItemEspLabel {
    float x, y;
    std::string text;
};

static std::vector<ItemEspLabel> s_itemLabels;
static jclass s_entityItemCls = nullptr;
static bool s_entityItemTried = false;

static void EnsureEntityItemClass(JNIEnv* env) {
    if (s_entityItemTried) return;
    s_entityItemTried = true;
    std::string n = Mapper::Get("net/minecraft/entity/item/EntityItem");
    if (n.empty()) return;
    Klass* k = g_Instance->FindClass(n.c_str());
    if (!k) return;
    s_entityItemCls = (jclass)env->NewGlobalRef((jclass)k);
}

static jobject GetDroppedStack(JNIEnv* env, jobject entity) {
    Klass* cls = g_Instance->FindClass(Mapper::Get("net/minecraft/entity/item/EntityItem"));
    if (!cls) return nullptr;
    std::string sig = "()" + Mapper::Get("net/minecraft/item/ItemStack", 2);
    Method* m = cls->GetMethod(env, Mapper::Get("getEntityItem").c_str(), sig.c_str());
    if (!m) return nullptr;
    jobject stack = m->CallObjectMethod(env, entity);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return stack;
}

static void DrawBox3D(const Vec3 corners[8], bool filled) {
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

void ItemEsp::OnRender(JNIEnv* env) {
    s_itemLabels.clear();
    if (!enabled || !env) return;

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    jobject rmObj = Minecraft::GetRenderManager(env);
    if (!playerObj || !worldObj || !rmObj) return;

    jobject screen = Minecraft::GetCurrentScreen(env);
    if (screen) {
        env->DeleteLocalRef(screen);
        return;
    }

    EnsureEntityItemClass(env);
    if (!s_entityItemCls) return;

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
    Vec3D lp = ((Player*)playerObj)->GetPos(env);

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
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    constexpr float hw = 0.15f, h = 0.25f;
    int n = 0;
    auto entities = ((World*)worldObj)->GetLoadedEntities(env);
    for (jobject e : entities) {
        if (!e) continue;
        if (++n > 500) { env->DeleteLocalRef(e); continue; }
        if (!env->IsInstanceOf(e, s_entityItemCls)) { env->DeleteLocalRef(e); continue; }

        auto* ent = (Player*)e;
        Vec3D pos = ent->GetPos(env);
        Vec3D last = ent->GetLastTickPos(env);
        double ex = last.x + (pos.x - last.x) * (double)partial;
        double ey = last.y + (pos.y - last.y) * (double)partial;
        double ez = last.z + (pos.z - last.z) * (double)partial;

        float dx = (float)(lp.x - ex), dy = (float)(lp.y - ey), dz = (float)(lp.z - ez);
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist > ItemEspSettings::maxDistance) { env->DeleteLocalRef(e); continue; }

        float rx = (float)(ex - cam.x);
        float ry = (float)(ey - cam.y);
        float rz = (float)(ez - cam.z);

        Vec3 box[8] = {
            { rx - hw, ry,     rz - hw }, { rx + hw, ry,     rz - hw },
            { rx + hw, ry,     rz + hw }, { rx - hw, ry,     rz + hw },
            { rx - hw, ry + h, rz - hw }, { rx + hw, ry + h, rz - hw },
            { rx + hw, ry + h, rz + hw }, { rx - hw, ry + h, rz + hw }
        };

        glDisable(GL_DEPTH_TEST);
        glLineWidth(1.5f);
        glColor4f(ItemEspSettings::color[0], ItemEspSettings::color[1],
            ItemEspSettings::color[2], ItemEspSettings::color[3]);
        DrawBox3D(box, false);
        glColor4f(ItemEspSettings::color[0], ItemEspSettings::color[1],
            ItemEspSettings::color[2], ItemEspSettings::color[3] * 0.25f);
        DrawBox3D(box, true);
        glEnable(GL_DEPTH_TEST);

        Vec2 screen;
        Vec3 namePos{ rx, ry + h + 0.1f, rz };
        if (WorldToScreen(namePos, screen, mv, proj, vp[2], vp[3])) {
            std::string itemName;
            jobject stack = GetDroppedStack(env, e);
            if (stack) {
                itemName = ((ItemStack*)stack)->GetDisplayName(env);
                env->DeleteLocalRef(stack);
            }
            if (!itemName.empty()) {
                char label[128];
                snprintf(label, sizeof(label), "%s [%.0fm]", itemName.c_str(), dist);
                s_itemLabels.push_back({ screen.x + (float)vp[0], screen.y + (float)vp[1], label });
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

void ItemEsp::OnImGuiRender(JNIEnv* env) {
    (void)env;
    if (!enabled || s_itemLabels.empty()) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
        ItemEspSettings::color[0], ItemEspSettings::color[1],
        ItemEspSettings::color[2], ItemEspSettings::color[3]));

    for (const auto& l : s_itemLabels) {
        ImVec2 sz = ImGui::CalcTextSize(l.text.c_str());
        float tx = l.x - sz.x * 0.5f;
        float ty = l.y - sz.y;
        dl->AddRectFilled(ImVec2(tx - 2.f, ty - 1.f), ImVec2(tx + sz.x + 2.f, ty + sz.y + 1.f),
            IM_COL32(0, 0, 0, 140), 2.f);
        dl->AddText(ImVec2(tx, ty), col, l.text.c_str());
    }
}
