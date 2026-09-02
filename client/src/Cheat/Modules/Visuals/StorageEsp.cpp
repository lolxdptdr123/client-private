#include "pch.h"
#include "StorageEsp.h"

#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"

#include "../../../../vendors/imgui/imgui.h"
#include <gl/GL.h>
#include <algorithm>
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

enum class StorageKind { Chest, EnderChest, Furnace, Dispenser, Dropper, Hopper };

struct StorageBox {
    Vec3 corners[8];
    StorageKind kind;
    float color[4];
    const char* label;
};

struct StorageLabel {
    float x, y;
    std::string text;
    float color[4];
};

static std::vector<StorageBox> s_boxes;
static std::vector<StorageLabel> s_labels;
static std::vector<float> s_mv, s_proj;

static jfieldID s_xCoord = nullptr, s_yCoord = nullptr, s_zCoord = nullptr;
static jfieldID s_posField = nullptr;
static jmethodID s_getPos = nullptr;
static jmethodID s_bpGetX = nullptr, s_bpGetY = nullptr, s_bpGetZ = nullptr;
static jfieldID s_bpX = nullptr, s_bpY = nullptr, s_bpZ = nullptr;
static bool s_coordIdsReady = false;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static jfieldID TryField(JNIEnv* env, jclass cls, const char* name, const char* sig) {
    if (!cls || !name || !sig || !*name || !*sig) return nullptr;
    jfieldID id = env->GetFieldID(cls, name, sig);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return id;
}

static jmethodID TryMethod(JNIEnv* env, jclass cls, const char* name, const char* sig) {
    if (!cls || !name || !sig || !*name || !*sig) return nullptr;
    jmethodID id = env->GetMethodID(cls, name, sig);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return id;
}

static void EnsureBlockPosAccess(JNIEnv* env, jobject pos) {
    if (!pos || (s_bpGetX && s_bpGetY && s_bpGetZ)) return;
    jclass cls = env->GetObjectClass(pos);
    if (!cls) return;
    s_bpGetX = TryMethod(env, cls, Mapper::Get("getX").c_str(), "()I");
    s_bpGetY = TryMethod(env, cls, Mapper::Get("getY").c_str(), "()I");
    s_bpGetZ = TryMethod(env, cls, Mapper::Get("getZ").c_str(), "()I");
    if (!s_bpGetX) s_bpGetX = TryMethod(env, cls, "getX", "()I");
    if (!s_bpGetY) s_bpGetY = TryMethod(env, cls, "getY", "()I");
    if (!s_bpGetZ) s_bpGetZ = TryMethod(env, cls, "getZ", "()I");
    if (!s_bpGetX || !s_bpGetY || !s_bpGetZ) {
        s_bpX = TryField(env, cls, "x", "I");
        s_bpY = TryField(env, cls, "y", "I");
        s_bpZ = TryField(env, cls, "z", "I");
        if (!s_bpX || !s_bpY || !s_bpZ) {
            jclass superCls = env->GetSuperclass(cls);
            if (superCls) {
                if (!s_bpX) s_bpX = TryField(env, superCls, "x", "I");
                if (!s_bpY) s_bpY = TryField(env, superCls, "y", "I");
                if (!s_bpZ) s_bpZ = TryField(env, superCls, "z", "I");
                env->DeleteLocalRef(superCls);
            }
        }
    }
    env->DeleteLocalRef(cls);
}

static bool ReadBlockPosXYZ(JNIEnv* env, jobject pos, int& x, int& y, int& z) {
    if (!pos) return false;
    EnsureBlockPosAccess(env, pos);
    if (s_bpGetX && s_bpGetY && s_bpGetZ) {
        x = env->CallIntMethod(pos, s_bpGetX);
        y = env->CallIntMethod(pos, s_bpGetY);
        z = env->CallIntMethod(pos, s_bpGetZ);
        JniOk(env);
        return true;
    }
    if (s_bpX && s_bpY && s_bpZ) {
        x = env->GetIntField(pos, s_bpX);
        y = env->GetIntField(pos, s_bpY);
        z = env->GetIntField(pos, s_bpZ);
        JniOk(env);
        return true;
    }
    return false;
}

static void EnsureTileCoordIds(JNIEnv* env, jobject te) {
    if (s_coordIdsReady || !te) return;
    jclass cls = env->GetObjectClass(te);
    if (!cls) return;

    s_xCoord = TryField(env, cls, Mapper::Get("xCoord").c_str(), "I");
    s_yCoord = TryField(env, cls, Mapper::Get("yCoord").c_str(), "I");
    s_zCoord = TryField(env, cls, Mapper::Get("zCoord").c_str(), "I");

    std::string posSig = Mapper::Get("net/minecraft/util/BlockPos", 2);
    if (posSig.empty()) posSig = "Lnet/minecraft/util/BlockPos;";
    s_posField = TryField(env, cls, Mapper::Get("pos").c_str(), posSig.c_str());
    if (!s_posField)
        s_posField = TryField(env, cls, "pos", "Lnet/minecraft/util/BlockPos;");

    s_getPos = TryMethod(env, cls, Mapper::Get("getPos").c_str(), Mapper::Get("net/minecraft/util/BlockPos", 3).c_str());
    if (!s_getPos)
        s_getPos = TryMethod(env, cls, "getPos", "()Lnet/minecraft/util/BlockPos;");

    s_coordIdsReady = true;
    env->DeleteLocalRef(cls);
}

static bool ReadTileXYZ(JNIEnv* env, jobject te, int& x, int& y, int& z) {
    if (!te) return false;
    EnsureTileCoordIds(env, te);

    if (s_xCoord && s_yCoord && s_zCoord) {
        x = env->GetIntField(te, s_xCoord);
        y = env->GetIntField(te, s_yCoord);
        z = env->GetIntField(te, s_zCoord);
        JniOk(env);
        return true;
    }

    jobject pos = nullptr;
    if (s_posField)
        pos = env->GetObjectField(te, s_posField);
    if (env->ExceptionCheck()) { env->ExceptionClear(); pos = nullptr; }
    if (!pos && s_getPos)
        pos = env->CallObjectMethod(te, s_getPos);
    if (env->ExceptionCheck()) { env->ExceptionClear(); pos = nullptr; }
    if (!pos) return false;

    const bool ok = ReadBlockPosXYZ(env, pos, x, y, z);
    env->DeleteLocalRef(pos);
    return ok;
}

static const float* ColorFor(StorageKind k) {
    switch (k) {
        case StorageKind::Chest:      return StorageEspSettings::chestColor;
        case StorageKind::EnderChest: return StorageEspSettings::enderChestColor;
        case StorageKind::Furnace:    return StorageEspSettings::furnaceColor;
        case StorageKind::Dispenser:  return StorageEspSettings::dispenserColor;
        case StorageKind::Dropper:    return StorageEspSettings::dropperColor;
        case StorageKind::Hopper:     return StorageEspSettings::hopperColor;
        default:                      return StorageEspSettings::chestColor;
    }
}

static const char* LabelFor(StorageKind k) {
    switch (k) {
        case StorageKind::Chest:      return "Chest";
        case StorageKind::EnderChest: return "Ender Chest";
        case StorageKind::Furnace:    return "Furnace";
        case StorageKind::Dispenser:  return "Dispenser";
        case StorageKind::Dropper:    return "Dropper";
        case StorageKind::Hopper:     return "Hopper";
        default:                      return "Storage";
    }
}

static bool NameHas(const std::string& n, const char* token) {
    return n.find(token) != std::string::npos;
}

static std::string TeClassName(JNIEnv* env, jobject te) {
    jclass cls = env->GetObjectClass(te);
    if (!cls) return {};
    static jmethodID s_getName = nullptr;
    if (!s_getName) {
        jclass classCls = env->FindClass("java/lang/Class");
        if (classCls) {
            s_getName = env->GetMethodID(classCls, "getName", "()Ljava/lang/String;");
            env->DeleteLocalRef(classCls);
        }
    }
    if (!s_getName) { env->DeleteLocalRef(cls); return {}; }
    jstring js = (jstring)env->CallObjectMethod(cls, s_getName);
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
    if (!js) return {};
    const char* c = env->GetStringUTFChars(js, nullptr);
    std::string out = c ? c : "";
    if (c) env->ReleaseStringUTFChars(js, c);
    env->DeleteLocalRef(js);
    return out;
}

static bool Classify(JNIEnv* env, jobject te, StorageKind& out) {
    const std::string n = TeClassName(env, te);
    if (n.empty()) return false;

    // Dropper extends Dispenser — match the exact class name, not IsInstanceOf.
    if (NameHas(n, "TileEntityDropper") || NameHas(n, ".Dropper")) {
        if (!StorageEspSettings::dropper) return false;
        out = StorageKind::Dropper;
        return true;
    }
    if (NameHas(n, "TileEntityDispenser") || NameHas(n, ".Dispenser")) {
        if (!StorageEspSettings::dispenser) return false;
        out = StorageKind::Dispenser;
        return true;
    }
    if (NameHas(n, "TileEntityHopper") || NameHas(n, ".Hopper")) {
        if (!StorageEspSettings::hopper) return false;
        out = StorageKind::Hopper;
        return true;
    }
    if (NameHas(n, "TileEntityEnderChest") || NameHas(n, "EnderChest")) {
        if (!StorageEspSettings::enderChest) return false;
        out = StorageKind::EnderChest;
        return true;
    }
    if (NameHas(n, "TileEntityFurnace") || NameHas(n, ".Furnace")) {
        if (!StorageEspSettings::furnace) return false;
        out = StorageKind::Furnace;
        return true;
    }
    if (NameHas(n, "TileEntityChest") || NameHas(n, ".Chest")) {
        if (!StorageEspSettings::chest) return false;
        out = StorageKind::Chest;
        return true;
    }
    return false;
}

static void DrawBox3D(const Vec3 c[8], bool filled) {
    if (filled) {
        glBegin(GL_QUADS);
        glVertex3f(c[0].x, c[0].y, c[0].z); glVertex3f(c[1].x, c[1].y, c[1].z);
        glVertex3f(c[5].x, c[5].y, c[5].z); glVertex3f(c[4].x, c[4].y, c[4].z);
        glVertex3f(c[2].x, c[2].y, c[2].z); glVertex3f(c[6].x, c[6].y, c[6].z);
        glVertex3f(c[7].x, c[7].y, c[7].z); glVertex3f(c[3].x, c[3].y, c[3].z);
        glVertex3f(c[4].x, c[4].y, c[4].z); glVertex3f(c[5].x, c[5].y, c[5].z);
        glVertex3f(c[6].x, c[6].y, c[6].z); glVertex3f(c[7].x, c[7].y, c[7].z);
        glVertex3f(c[0].x, c[0].y, c[0].z); glVertex3f(c[3].x, c[3].y, c[3].z);
        glVertex3f(c[2].x, c[2].y, c[2].z); glVertex3f(c[1].x, c[1].y, c[1].z);
        glVertex3f(c[1].x, c[1].y, c[1].z); glVertex3f(c[2].x, c[2].y, c[2].z);
        glVertex3f(c[6].x, c[6].y, c[6].z); glVertex3f(c[5].x, c[5].y, c[5].z);
        glVertex3f(c[0].x, c[0].y, c[0].z); glVertex3f(c[4].x, c[4].y, c[4].z);
        glVertex3f(c[7].x, c[7].y, c[7].z); glVertex3f(c[3].x, c[3].y, c[3].z);
        glEnd();
    } else {
        glBegin(GL_LINES);
        glVertex3f(c[0].x, c[0].y, c[0].z); glVertex3f(c[1].x, c[1].y, c[1].z);
        glVertex3f(c[1].x, c[1].y, c[1].z); glVertex3f(c[2].x, c[2].y, c[2].z);
        glVertex3f(c[2].x, c[2].y, c[2].z); glVertex3f(c[3].x, c[3].y, c[3].z);
        glVertex3f(c[3].x, c[3].y, c[3].z); glVertex3f(c[0].x, c[0].y, c[0].z);
        glVertex3f(c[4].x, c[4].y, c[4].z); glVertex3f(c[5].x, c[5].y, c[5].z);
        glVertex3f(c[5].x, c[5].y, c[5].z); glVertex3f(c[6].x, c[6].y, c[6].z);
        glVertex3f(c[6].x, c[6].y, c[6].z); glVertex3f(c[7].x, c[7].y, c[7].z);
        glVertex3f(c[7].x, c[7].y, c[7].z); glVertex3f(c[4].x, c[4].y, c[4].z);
        glVertex3f(c[0].x, c[0].y, c[0].z); glVertex3f(c[4].x, c[4].y, c[4].z);
        glVertex3f(c[1].x, c[1].y, c[1].z); glVertex3f(c[5].x, c[5].y, c[5].z);
        glVertex3f(c[2].x, c[2].y, c[2].z); glVertex3f(c[6].x, c[6].y, c[6].z);
        glVertex3f(c[3].x, c[3].y, c[3].z); glVertex3f(c[7].x, c[7].y, c[7].z);
        glEnd();
    }
}

void StorageEsp::OnRender(JNIEnv* env) {
    s_boxes.clear();
    s_labels.clear();
    if (!enabled || !env) return;
    JniOk(env);

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    jobject rmObj = Minecraft::GetRenderManager(env);
    if (!playerObj || !worldObj || !rmObj) return;

    s_proj = ActiveRenderInfo::GetProjection(env);
    s_mv = ActiveRenderInfo::GetModelView(env);
    if (s_proj.size() < 16 || s_mv.size() < 16) return;

    Vec3D cam = ((RenderManager*)rmObj)->GetRenderPos(env);
    Vec3D lp = ((Player*)playerObj)->GetPos(env);

    const bool render3D = StorageEspSettings::renderMode == 1 || StorageEspSettings::renderMode == 2;
    const bool render2D = StorageEspSettings::renderMode == 0 || StorageEspSettings::renderMode == 2;

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    if (render3D) {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushMatrix();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadMatrixf(s_proj.data());
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadMatrixf(s_mv.data());
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
    }

    auto tiles = ((World*)worldObj)->GetLoadedTileEntities(env);
    int n = 0;
    for (jobject te : tiles) {
        if (!te) continue;
        if (++n > 400) { env->DeleteLocalRef(te); continue; }

        StorageKind kind;
        if (!Classify(env, te, kind)) { env->DeleteLocalRef(te); continue; }
        JniOk(env);

        int bx = 0, by = 0, bz = 0;
        if (!ReadTileXYZ(env, te, bx, by, bz)) { env->DeleteLocalRef(te); continue; }

        float dx = (float)(lp.x - ((double)bx + 0.5));
        float dy = (float)(lp.y - ((double)by + 0.5));
        float dz = (float)(lp.z - ((double)bz + 0.5));
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist > StorageEspSettings::maxDistance) { env->DeleteLocalRef(te); continue; }

        float rx = (float)((double)bx - cam.x);
        float ry = (float)((double)by - cam.y);
        float rz = (float)((double)bz - cam.z);

        StorageBox box{};
        box.kind = kind;
        box.label = LabelFor(kind);
        const float* col = ColorFor(kind);
        box.color[0] = col[0]; box.color[1] = col[1]; box.color[2] = col[2]; box.color[3] = col[3];
        box.corners[0] = { rx,     ry,     rz };
        box.corners[1] = { rx + 1, ry,     rz };
        box.corners[2] = { rx + 1, ry,     rz + 1 };
        box.corners[3] = { rx,     ry,     rz + 1 };
        box.corners[4] = { rx,     ry + 1, rz };
        box.corners[5] = { rx + 1, ry + 1, rz };
        box.corners[6] = { rx + 1, ry + 1, rz + 1 };
        box.corners[7] = { rx,     ry + 1, rz + 1 };

        if (render3D) {
            if (StorageEspSettings::mode3d == 1 || StorageEspSettings::mode3d == 2) {
                glColor4f(box.color[0], box.color[1], box.color[2], StorageEspSettings::fillAlpha3d);
                DrawBox3D(box.corners, true);
                glDepthMask(GL_TRUE);
            }
            if (StorageEspSettings::mode3d == 0 || StorageEspSettings::mode3d == 2) {
                glLineWidth(StorageEspSettings::outline3dWidth);
                glColor4f(box.color[0], box.color[1], box.color[2], box.color[3]);
                DrawBox3D(box.corners, false);
            }
        }

        if (render2D || StorageEspSettings::showLabels)
            s_boxes.push_back(box);

        env->DeleteLocalRef(te);
    }

    if (render3D) {
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glPopMatrix();
        glPopAttrib();
    }

    if (render2D || StorageEspSettings::showLabels) {
        for (const auto& b : s_boxes) {
            float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
            int hit = 0;
            for (int i = 0; i < 8; i++) {
                Vec2 s;
                if (!WorldToScreen(b.corners[i], s, s_mv, s_proj, vp[2], vp[3])) continue;
                hit++;
                minX = (std::min)(minX, s.x);
                maxX = (std::max)(maxX, s.x);
                minY = (std::min)(minY, s.y);
                maxY = (std::max)(maxY, s.y);
            }
            if (hit < 2) continue;
            minX = (std::max)(0.f, (std::min)(minX, (float)vp[2]));
            maxX = (std::max)(0.f, (std::min)(maxX, (float)vp[2]));
            minY = (std::max)(0.f, (std::min)(minY, (float)vp[3]));
            maxY = (std::max)(0.f, (std::min)(maxY, (float)vp[3]));
            if (maxX - minX < 3.f || maxY - minY < 3.f) continue;

            if (StorageEspSettings::showLabels) {
                char buf[48];
                snprintf(buf, sizeof(buf), "%s", b.label);
                s_labels.push_back({ (minX + maxX) * 0.5f + (float)vp[0], minY + (float)vp[1], buf,
                    { b.color[0], b.color[1], b.color[2], b.color[3] } });
            }
        }
    }
}

void StorageEsp::OnImGuiRender(JNIEnv* env) {
    (void)env;
    if (!enabled) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    const bool render2D = StorageEspSettings::renderMode == 0 || StorageEspSettings::renderMode == 2;
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    if (render2D) {
        for (const auto& b : s_boxes) {
            float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
            int hit = 0;
            for (int i = 0; i < 8; i++) {
                Vec2 s;
                if (!WorldToScreen(b.corners[i], s, s_mv, s_proj, vp[2], vp[3])) continue;
                hit++;
                minX = (std::min)(minX, s.x);
                maxX = (std::max)(maxX, s.x);
                minY = (std::min)(minY, s.y);
                maxY = (std::max)(maxY, s.y);
            }
            if (hit < 2) continue;
            minX += (float)vp[0]; maxX += (float)vp[0];
            minY += (float)vp[1]; maxY += (float)vp[1];
            if (maxX - minX < 3.f || maxY - minY < 3.f) continue;

            ImU32 outline = ImGui::ColorConvertFloat4ToU32(ImVec4(b.color[0], b.color[1], b.color[2], b.color[3]));
            ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(b.color[0], b.color[1], b.color[2], StorageEspSettings::fillAlpha2d));
            if (StorageEspSettings::mode2d == 1 || StorageEspSettings::mode2d == 2)
                dl->AddRectFilled(ImVec2(minX, minY), ImVec2(maxX, maxY), fill);
            if (StorageEspSettings::mode2d == 0 || StorageEspSettings::mode2d == 2)
                dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), outline, 0.f, 0, StorageEspSettings::outline2dWidth);
        }
    }

    if (!StorageEspSettings::showLabels || s_labels.empty()) return;
    const float fs = 13.f * StorageEspSettings::labelScale;
    for (const auto& l : s_labels) {
        ImVec2 sz = ImGui::CalcTextSize(l.text.c_str());
        float sc = fs / ImGui::GetFontSize();
        float tx = l.x - sz.x * sc * 0.5f;
        float ty = l.y - 15.f * StorageEspSettings::labelScale;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
            StorageEspSettings::labelColor[0], StorageEspSettings::labelColor[1],
            StorageEspSettings::labelColor[2], StorageEspSettings::labelColor[3]));
        dl->AddText(ImGui::GetFont(), fs, ImVec2(tx + 1.f, ty + 1.f), IM_COL32(0, 0, 0, 200), l.text.c_str());
        dl->AddText(ImGui::GetFont(), fs, ImVec2(tx, ty), col, l.text.c_str());
    }
}
