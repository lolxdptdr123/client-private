#include "pch.h"
#include "Tracer.h"

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
#include <string>
#include <vector>
#include <algorithm>

struct TrPlayer {
    Vec3 chest{};
    bool isFriend = false;
    bool isEnemy = false;
};

static std::vector<TrPlayer> s_players;
static std::vector<float> s_mv, s_proj;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static bool W2S(const Vec3& w, Vec2& s) {
    return WorldToScreen(w, s, s_mv, s_proj,
        (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
}

void Tracer::OnRender(JNIEnv* env) {
    (void)env;
}

void Tracer::OnImGuiRender(JNIEnv* env) {
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
        if (TracerSettings::hideFriends && isFriend) { env->DeleteLocalRef(e); continue; }
        const bool isEnemy = EnemiesSettings::IsEnemy(env, ent);
        if (TracerSettings::enemiesOnly && !isEnemy) { env->DeleteLocalRef(e); continue; }

        Vec3D pos = ent->GetPos(env);
        Vec3D last = ent->GetLastTickPos(env);
        float ix = (float)(last.x + (pos.x - last.x) * (double)partial - cam.x);
        float iy = (float)(last.y + (pos.y - last.y) * (double)partial - cam.y);
        float iz = (float)(last.z + (pos.z - last.z) * (double)partial - cam.z);

        float dx = ix - localPos.x, dy = iy - localPos.y, dz = iz - localPos.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist > TracerSettings::maxRenderDistance) { env->DeleteLocalRef(e); continue; }

        s_players.push_back({ { ix, iy + 0.9f, iz }, isFriend, isEnemy });
        env->DeleteLocalRef(e);
    }
    JniOk(env);

    if (s_players.empty()) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;

    const ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x, sh = io.DisplaySize.y;
    if (sw <= 1.f || sh <= 1.f) return;

    ImVec2 origin(sw * 0.5f, sh * 0.5f);
    const float width = (std::max)(1.f, TracerSettings::tracerWidth);

    for (const auto& p : s_players) {
        Vec2 screen;
        if (!W2S(p.chest, screen)) continue;
        if (!std::isfinite(screen.x) || !std::isfinite(screen.y)) continue;

        ImVec2 end(screen.x, screen.y);
        const float* src = p.isFriend ? TracerSettings::friendColor
            : (p.isEnemy ? TracerSettings::enemyColor : TracerSettings::tracerColor);
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(src[0], src[1], src[2], src[3]));
        dl->AddLine(origin, end, IM_COL32(0, 0, 0, 180), width + 2.f);
        dl->AddLine(origin, end, col, width);
    }
}
