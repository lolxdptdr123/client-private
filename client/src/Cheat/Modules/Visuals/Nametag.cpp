#include "pch.h"
#include "Nametag.h"

#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Classes/Timer.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"

#include "../../../../vendors/imgui/imgui.h"
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cctype>
#include <string>
#include <vector>
#include <algorithm>

struct NtSeg { std::string text; ImVec4 color; };
struct NtPlayer {
    Vec3 head{};
    Vec3 feet{};
    float distance = 0.f;
    float health = 20.f;
    float maxHealth = 20.f;
    std::string name;
    bool isFriend = false;
    bool isEnemy = false;
};

static std::vector<NtPlayer> s_players;
static std::vector<float> s_mv, s_proj;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static bool W2S(const Vec3& w, Vec2& s) {
    return WorldToScreen(w, s, s_mv, s_proj,
        (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
}

static ImVec4 McColor(char code) {
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
        default:  return ImVec4(1.f, 1.f, 1.f, 1.f);
    }
}

static ImVec4 LerpCol(const float a[4], const float b[4], float t) {
    t = (std::max)(0.f, (std::min)(1.f, t));
    return ImVec4(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t,
        a[2] + (b[2] - a[2]) * t, a[3] + (b[3] - a[3]) * t);
}

static std::vector<NtSeg> ParseMcText(const std::string& text) {
    std::vector<NtSeg> segs;
    ImVec4 col = ImVec4(
        NametagSettings::nameColor[0], NametagSettings::nameColor[1],
        NametagSettings::nameColor[2], NametagSettings::nameColor[3]);
    std::string cur;
    auto flush = [&]() {
        if (!cur.empty()) { segs.push_back({ cur, col }); cur.clear(); }
    };
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = (unsigned char)text[i];
        bool sect = false;
        if (c == 0xC2 && i + 1 < text.size() && (unsigned char)text[i + 1] == 0xA7) {
            flush(); i += 2; sect = true;
        } else if (c == 0xA7) {
            flush(); i++; sect = true;
        }
        if (sect) {
            if (i < text.size()) {
                char code = (char)tolower((unsigned char)text[i]);
                if (code == 'r')
                    col = ImVec4(NametagSettings::nameColor[0], NametagSettings::nameColor[1],
                        NametagSettings::nameColor[2], NametagSettings::nameColor[3]);
                else if ((code >= '0' && code <= '9') || (code >= 'a' && code <= 'f'))
                    col = McColor(code);
            }
            continue;
        }
        cur += text[i];
    }
    flush();
    return segs;
}

static float SegWidth(ImFont* font, float size, const std::vector<NtSeg>& segs) {
    float w = 0.f;
    for (const auto& s : segs)
        w += font->CalcTextSizeA(size, FLT_MAX, 0.f, s.text.c_str()).x;
    return w;
}

static void DrawOutlineText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 col, const char* text) {
    if (NametagSettings::showOutline) {
        ImU32 oc = ImGui::ColorConvertFloat4ToU32(ImVec4(
            NametagSettings::outlineColor[0], NametagSettings::outlineColor[1],
            NametagSettings::outlineColor[2], NametagSettings::outlineColor[3]));
        float t = NametagSettings::outlineThickness;
        for (int x = -1; x <= 1; x++)
            for (int y = -1; y <= 1; y++)
                if (x || y)
                    dl->AddText(font, size, ImVec2(pos.x + x * t, pos.y + y * t), oc, text);
    }
    dl->AddText(font, size, pos, col, text);
}

void Nametag::OnRender(JNIEnv* env) {
    (void)env;
}

void Nametag::OnImGuiRender(JNIEnv* env) {
    s_players.clear();
    if (!enabled || !env) return;
    JniOk(env);

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

    s_proj = ActiveRenderInfo::GetProjection(env);
    s_mv = ActiveRenderInfo::GetModelView(env);
    if (s_proj.size() < 16 || s_mv.size() < 16) return;

    Vec3D cam = ((RenderManager*)rmObj)->GetRenderPos(env);
    auto* local = (Player*)playerObj;
    Vec3D lp = local->GetPos(env);
    Vec3D ll = local->GetLastTickPos(env);
    Vec3 localPos{
        (float)(ll.x + (lp.x - ll.x) * (double)partial - cam.x),
        (float)(ll.y + (lp.y - ll.y) * (double)partial - cam.y),
        (float)(ll.z + (lp.z - ll.z) * (double)partial - cam.z)
    };

    auto players = ((World*)worldObj)->GetPlayerEntities(env);
    for (auto* ent : players) {
        jobject e = (jobject)ent;
        if (!e) continue;
        if (env->IsSameObject(e, playerObj)) { env->DeleteLocalRef(e); continue; }
        if (ent->IsDead(env)) { env->DeleteLocalRef(e); continue; }

        const bool isFriend = FriendsSettings::IsFriend(env, ent);
        if (NametagSettings::hideFriends && isFriend) { env->DeleteLocalRef(e); continue; }
        const bool isEnemy = EnemiesSettings::IsEnemy(env, ent);
        if (NametagSettings::enemiesOnly && !isEnemy) { env->DeleteLocalRef(e); continue; }

        std::string rawName = ent->GetName(env, false);
        JniOk(env);
        std::string plainName = ent->GetName(env, true);
        JniOk(env);

        Vec3D pos = ent->GetPos(env);
        Vec3D last = ent->GetLastTickPos(env);
        float ix = (float)(last.x + (pos.x - last.x) * (double)partial - cam.x);
        float iy = (float)(last.y + (pos.y - last.y) * (double)partial - cam.y);
        float iz = (float)(last.z + (pos.z - last.z) * (double)partial - cam.z);

        NtPlayer pd;
        pd.feet = { ix, iy, iz };
        pd.head = { ix, iy + 2.2f, iz };
        float dx = pd.head.x - localPos.x, dy = pd.head.y - localPos.y, dz = pd.head.z - localPos.z;
        pd.distance = sqrtf(dx * dx + dy * dy + dz * dz);
        if (pd.distance > NametagSettings::maxRenderDistance) { env->DeleteLocalRef(e); continue; }

        pd.health = ent->GetHealth(env);
        JniOk(env);
        pd.maxHealth = 20.f;
        pd.name = rawName.empty() ? plainName : rawName;
        pd.isFriend = isFriend;
        pd.isEnemy = isEnemy;
        s_players.push_back(std::move(pd));
        env->DeleteLocalRef(e);
    }
    JniOk(env);

    if (s_players.empty()) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    ImFont* font = ImGui::GetFont();
    if (!font) return;

    const ImGuiIO& io = ImGui::GetIO();
    const float sh = io.DisplaySize.y, sw = io.DisplaySize.x;

    for (const auto& p : s_players) {
        Vec2 screen, head, feet;
        if (!W2S(p.head, screen)) continue;
        if (!std::isfinite(screen.x) || !std::isfinite(screen.y)) continue;
        if (screen.x < -100.f || screen.x > sw + 100.f || screen.y < -100.f || screen.y > sh + 100.f)
            continue;

        const bool haveFeet = W2S(p.feet, feet) && std::isfinite(feet.x) && std::isfinite(feet.y);
        const bool haveHead = W2S(Vec3{ p.head.x, p.head.y + 0.3f, p.head.z }, head)
            && std::isfinite(head.x) && std::isfinite(head.y);

        float boxH;
        if (haveFeet && haveHead)
            boxH = fabsf(head.y - feet.y);
        else {
            const float d = (std::max)(p.distance, 0.5f);
            boxH = (std::max)(50.f, (std::min)(1.8f / d * sh * 0.7f, sh * 0.9f));
        }
        float scale = boxH / (0.33038348082f * sh);
        if (scale < 0.2f) scale = 0.2f;
        if (scale > 1.f) scale = 1.f;
        const float fs = NametagSettings::textSize * NametagSettings::nametagScale * scale;
        if (fs <= 0.f || fs > 200.f || !std::isfinite(fs)) continue;

        std::vector<NtSeg> segs;
        if (NametagSettings::showHealth) {
            char hs[16];
            snprintf(hs, sizeof(hs), "%.0f ", p.health);
            float hp = (p.maxHealth > 0.f) ? p.health / p.maxHealth : 0.f;
            segs.push_back({ hs, LerpCol(NametagSettings::healthBarLow, NametagSettings::healthBarFull, hp) });
        }
        if (NametagSettings::showNames) {
            auto nameSegs = ParseMcText(p.name);
            if (p.isFriend) {
                ImVec4 fc(NametagSettings::friendColor[0], NametagSettings::friendColor[1],
                    NametagSettings::friendColor[2], NametagSettings::friendColor[3]);
                for (auto& s : nameSegs) s.color = fc;
            } else if (p.isEnemy) {
                ImVec4 ec(NametagSettings::enemyColor[0], NametagSettings::enemyColor[1],
                    NametagSettings::enemyColor[2], NametagSettings::enemyColor[3]);
                for (auto& s : nameSegs) s.color = ec;
            }
            segs.insert(segs.end(), nameSegs.begin(), nameSegs.end());
        }
        if (NametagSettings::showDistance) {
            char ds[16];
            snprintf(ds, sizeof(ds), " %.0fm", p.distance);
            segs.push_back({ ds, ImVec4(NametagSettings::distanceColor[0], NametagSettings::distanceColor[1],
                NametagSettings::distanceColor[2], NametagSettings::distanceColor[3]) });
        }
        if (segs.empty()) continue;

        float totalW = SegWidth(font, fs, segs);
        if (NametagSettings::showBackground) {
            const float pad = 4.f;
            ImVec2 bgMin(screen.x - totalW * 0.5f - pad, screen.y - pad);
            ImVec2 bgMax(screen.x + totalW * 0.5f + pad, screen.y + fs + pad);
            ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(
                NametagSettings::backgroundColor[0], NametagSettings::backgroundColor[1],
                NametagSettings::backgroundColor[2], NametagSettings::backgroundColor[3]));
            dl->AddRectFilled(bgMin, bgMax, bg, 3.f);
            ImVec4 rc = p.isFriend
                ? ImVec4(NametagSettings::friendColor[0], NametagSettings::friendColor[1],
                    NametagSettings::friendColor[2], NametagSettings::friendColor[3])
                : (p.isEnemy
                    ? ImVec4(NametagSettings::enemyColor[0], NametagSettings::enemyColor[1],
                        NametagSettings::enemyColor[2], NametagSettings::enemyColor[3])
                    : ImVec4(NametagSettings::neutralColor[0], NametagSettings::neutralColor[1],
                        NametagSettings::neutralColor[2], NametagSettings::neutralColor[3]));
            dl->AddLine(ImVec2(bgMin.x + 4.f, bgMax.y - 1.5f), ImVec2(bgMax.x - 4.f, bgMax.y - 1.5f),
                ImGui::ColorConvertFloat4ToU32(rc), 1.f);
        }

        float x = screen.x - totalW * 0.5f;
        for (const auto& s : segs) {
            ImU32 col = ImGui::ColorConvertFloat4ToU32(s.color);
            DrawOutlineText(dl, font, fs, ImVec2(x, screen.y), col, s.text.c_str());
            x += font->CalcTextSizeA(fs, FLT_MAX, 0.f, s.text.c_str()).x;
        }

        if (NametagSettings::showHealthBar) {
            float bw = NametagSettings::healthBarWidth * scale * NametagSettings::nametagScale;
            float bh = NametagSettings::healthBarHeight * scale * NametagSettings::nametagScale;
            float hp = (p.maxHealth > 0.f) ? (std::max)(0.f, (std::min)(1.f, p.health / p.maxHealth)) : 0.f;
            ImVec2 bar(screen.x - bw * 0.5f, screen.y + fs + 6.f);
            dl->AddRectFilled(bar, ImVec2(bar.x + bw, bar.y + bh), ImGui::ColorConvertFloat4ToU32(ImVec4(
                NametagSettings::healthBarBg[0], NametagSettings::healthBarBg[1],
                NametagSettings::healthBarBg[2], NametagSettings::healthBarBg[3])));
            ImVec4 hc = LerpCol(NametagSettings::healthBarLow, NametagSettings::healthBarFull, hp);
            dl->AddRectFilled(bar, ImVec2(bar.x + bw * hp, bar.y + bh), ImGui::ColorConvertFloat4ToU32(hc));
            dl->AddRect(bar, ImVec2(bar.x + bw, bar.y + bh), IM_COL32(0, 0, 0, 200));
        }
    }
}
