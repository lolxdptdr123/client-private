#include "pch.h"
#include "Indicators.h"

#include "../Misc/Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Cheat/Hack.h"

#include "../../../../vendors/imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

enum class IndKind { Fireball, Pearl, Arrow };

struct IndItem {
    IndKind kind;
    float dist;
    float ang;
    bool approaching;
};

static jclass s_arrow = nullptr;
static jclass s_fire = nullptr;
static jclass s_smallFire = nullptr;
static jclass s_largeFire = nullptr;
static jclass s_pearl = nullptr;
static bool s_tried = false;
static std::unordered_set<int> s_seen;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static jclass FindEnt(JNIEnv* env, const char* mcp, const char* simple) {
    auto tryName = [&](const std::string& n) -> jclass {
        if (n.empty()) return nullptr;
        Klass* k = g_Instance->FindClass(n.c_str());
        if (!k) return nullptr;
        return (jclass)env->NewGlobalRef((jclass)k);
    };
    jclass c = tryName(Mapper::Get(mcp));
    if (c) return c;
    c = tryName(mcp);
    if (c) return c;
    if (simple && simple[0]) {
        c = tryName(Mapper::Get(simple));
        if (c) return c;
        c = tryName(simple);
    }
    return nullptr;
}

static void Ensure(JNIEnv* env) {
    if (s_tried || !env) return;
    s_tried = true;
    s_arrow = FindEnt(env, "net/minecraft/entity/projectile/EntityArrow", "EntityArrow");
    s_fire = FindEnt(env, "net/minecraft/entity/projectile/EntityFireball", "EntityFireball");
    s_smallFire = FindEnt(env, "net/minecraft/entity/projectile/EntitySmallFireball", "EntitySmallFireball");
    s_largeFire = FindEnt(env, "net/minecraft/entity/projectile/EntityLargeFireball", "EntityLargeFireball");
    s_pearl = FindEnt(env, "net/minecraft/entity/item/EntityEnderPearl", "EntityEnderPearl");
}

static bool IsKind(JNIEnv* env, jobject e, IndKind k) {
    if (!e) return false;
    if (k == IndKind::Arrow) return s_arrow && env->IsInstanceOf(e, s_arrow);
    if (k == IndKind::Pearl) return s_pearl && env->IsInstanceOf(e, s_pearl);
    if (s_fire && env->IsInstanceOf(e, s_fire)) return true;
    if (s_largeFire && env->IsInstanceOf(e, s_largeFire)) return true;
    if (s_smallFire && env->IsInstanceOf(e, s_smallFire)) return true;
    return false;
}

static const char* KindName(IndKind k) {
    if (k == IndKind::Fireball) return "Fireball";
    if (k == IndKind::Pearl) return "Ender pearl";
    return "Arrow";
}

static ImU32 KindCol(IndKind k) {
    if (k == IndKind::Fireball) return IM_COL32(255, 120, 40, 255);
    if (k == IndKind::Pearl) return IM_COL32(80, 220, 160, 255);
    return IM_COL32(230, 230, 230, 255);
}

void Indicators::OnImGuiRender(JNIEnv* env) {
    if (!env) return;
    if (!enabled) {
        s_seen.clear();
        return;
    }
    if (Overlay::isOpen) return;
    JniOk(env);
    Ensure(env);

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    if (!playerObj || !worldObj) return;
    auto* local = (Player*)playerObj;
    Vec3D lp = local->GetPos(env);
    JniOk(env);
    float yaw = local->GetRotationYaw(env) * 0.017453292f;
    JniOk(env);

    std::vector<IndItem> items;
    std::unordered_set<int> now;
    auto ents = ((World*)worldObj)->GetLoadedEntities(env);
    JniOk(env);

    for (jobject e : ents) {
        if (!e) continue;
        if (env->IsSameObject(e, playerObj)) { env->DeleteLocalRef(e); continue; }

        IndKind kind = IndKind::Arrow;
        bool match = false;
        if (IndicatorsSettings::arrows && IsKind(env, e, IndKind::Arrow)) {
            kind = IndKind::Arrow; match = true;
        } else if (IndicatorsSettings::pearls && IsKind(env, e, IndKind::Pearl)) {
            kind = IndKind::Pearl; match = true;
        } else if (IndicatorsSettings::fireballs && IsKind(env, e, IndKind::Fireball)) {
            kind = IndKind::Fireball; match = true;
        }
        JniOk(env);
        if (!match) { env->DeleteLocalRef(e); continue; }

        auto* ent = (Player*)e;
        if (ent->IsDead(env)) { JniOk(env); env->DeleteLocalRef(e); continue; }
        JniOk(env);

        Vec3D pos = ent->GetPos(env);
        JniOk(env);
        float dx = (float)(pos.x - lp.x);
        float dy = (float)(pos.y - lp.y);
        float dz = (float)(pos.z - lp.z);
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);

        double mx = ent->GetMotionX(env);
        double my = ent->GetMotionY(env);
        double mz = ent->GetMotionZ(env);
        JniOk(env);
        float approaching = 0.f;
        if (dist > 0.001f) {
            approaching = (float)(mx * (-dx) + my * (-dy) + mz * (-dz));
        }
        bool closer = approaching > 0.02f;
        if (IndicatorsSettings::comingCloser && !closer) {
            env->DeleteLocalRef(e);
            continue;
        }

        int id = ent->GetEntityId(env);
        JniOk(env);
        now.insert(id);

        float ang = atan2f(dx, dz) - yaw;
        while (ang > 3.14159265f) ang -= 6.2831853f;
        while (ang < -3.14159265f) ang += 6.2831853f;
        items.push_back({ kind, dist, ang, closer });
        env->DeleteLocalRef(e);
    }

    s_seen.swap(now);

    if (items.empty()) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;
    const ImGuiIO& io = ImGui::GetIO();
    const float cx = io.DisplaySize.x * 0.5f;
    const float cy = io.DisplaySize.y * 0.5f;
    const float rad = 52.f;

    int row = 0;
    for (const auto& it : items) {
        ImU32 col = KindCol(it.kind);
        ImVec2 p(cx + sinf(it.ang) * rad, cy - cosf(it.ang) * rad);
        float c = cosf(it.ang), s = sinf(it.ang);
        ImVec2 fwd(s, -c), right(c, s);
        const float sz = 9.f;
        ImVec2 tip(p.x + fwd.x * sz, p.y + fwd.y * sz);
        ImVec2 l(p.x - fwd.x * sz * 0.5f - right.x * sz * 0.4f, p.y - fwd.y * sz * 0.5f - right.y * sz * 0.4f);
        ImVec2 r(p.x - fwd.x * sz * 0.5f + right.x * sz * 0.4f, p.y - fwd.y * sz * 0.5f + right.y * sz * 0.4f);
        dl->AddTriangleFilled(tip, l, r, col);

        char buf[48];
        snprintf(buf, sizeof(buf), "%s  %.0fm", KindName(it.kind), it.dist);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        ImVec2 tp(cx - ts.x * 0.5f, cy + 70.f + row * 16.f);
        dl->AddText(ImVec2(tp.x + 1.f, tp.y + 1.f), IM_COL32(0, 0, 0, 180), buf);
        dl->AddText(tp, col, buf);
        row++;
    }
}
