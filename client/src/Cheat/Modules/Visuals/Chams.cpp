#include "pch.h"
#include "Chams.h"

#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Classes/Timer.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"
#include "../Combat/AntiBot.h"

#include <gl/GL.h>
#pragma comment(lib, "opengl32.lib")

#ifndef GL_FOG
#define GL_FOG 0x0B60
#endif
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0BC0
#endif
#ifndef GL_COLOR_MATERIAL
#define GL_COLOR_MATERIAL 0x0B57
#endif
#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#endif
#ifndef GL_COMBINE_RGB
#define GL_COMBINE_RGB 0x8571
#endif
#ifndef GL_COMBINE_ALPHA
#define GL_COMBINE_ALPHA 0x8572
#endif
#ifndef GL_SOURCE0_RGB
#define GL_SOURCE0_RGB 0x8580
#endif
#ifndef GL_SOURCE0_ALPHA
#define GL_SOURCE0_ALPHA 0x8588
#endif
#ifndef GL_CONSTANT_ARB
#define GL_CONSTANT_ARB 0x8576
#endif
#ifndef GL_LEQUAL
#define GL_LEQUAL 0x0203
#endif
#ifndef GL_ALWAYS
#define GL_ALWAYS 0x0207
#endif
#ifndef GL_MODULATE
#define GL_MODULATE 0x2100
#endif

static bool g_renderingChams = false;

static jclass GlobalClass(JNIEnv* env, const char* mapped) {
    Klass* k = g_Instance->FindClass(mapped);
    if (!k) return nullptr;
    jclass g = (jclass)env->NewGlobalRef((jclass)k);
    return g;
}

struct ChamsClassCache {
    jclass living = nullptr;
    jclass player = nullptr;
    jclass mob = nullptr;
    jclass animal = nullptr;
    jclass villager = nullptr;
    jclass armorStand = nullptr;
    bool built = false;
} static s_cc;

static void BuildChamsClasses(JNIEnv* env) {
    if (s_cc.built) return;
    s_cc.built = true;
    auto load = [&](const char* key) {
        std::string n = Mapper::Get(key);
        return n.empty() ? nullptr : GlobalClass(env, n.c_str());
    };
    s_cc.living = load("net/minecraft/entity/EntityLivingBase");
    s_cc.player = load("net/minecraft/entity/player/EntityPlayer");
    s_cc.mob = load("net/minecraft/entity/monster/EntityMob");
    s_cc.animal = load("net/minecraft/entity/passive/EntityAnimal");
    s_cc.villager = load("net/minecraft/entity/passive/EntityVillager");
    s_cc.armorStand = load("net/minecraft/entity/item/EntityArmorStand");
}

static bool PassesFilter(JNIEnv* env, jobject e) {
    bool matched = false;
    if (s_cc.player && env->IsInstanceOf(e, s_cc.player)) { matched = true; if (!ChamsSettings::players) return false; }
    else if (s_cc.mob && env->IsInstanceOf(e, s_cc.mob)) { matched = true; if (!ChamsSettings::mobs) return false; }
    else if (s_cc.animal && env->IsInstanceOf(e, s_cc.animal)) { matched = true; if (!ChamsSettings::animals) return false; }
    else if (s_cc.villager && env->IsInstanceOf(e, s_cc.villager)) { matched = true; if (!ChamsSettings::villagers) return false; }
    else if (s_cc.armorStand && env->IsInstanceOf(e, s_cc.armorStand)) { matched = true; if (!ChamsSettings::armorStands) return false; }
    if (!matched) {
        if (s_cc.player || s_cc.mob || s_cc.animal || s_cc.villager)
            return ChamsSettings::players;
        return true;
    }
    return true;
}

static void GetEntityColor(JNIEnv* env, jobject e, float out[4]) {
    out[0] = ChamsSettings::colorNeutral[0];
    out[1] = ChamsSettings::colorNeutral[1];
    out[2] = ChamsSettings::colorNeutral[2];
    out[3] = ChamsSettings::colorNeutral[3];
    if (FriendsSettings::IsFriend(env, (Player*)e)) {
        out[0] = ChamsSettings::colorFriend[0];
        out[1] = ChamsSettings::colorFriend[1];
        out[2] = ChamsSettings::colorFriend[2];
        out[3] = ChamsSettings::colorFriend[3];
    } else if (EnemiesSettings::IsEnemy(env, (Player*)e)) {
        out[0] = ChamsSettings::colorEnemy[0];
        out[1] = ChamsSettings::colorEnemy[1];
        out[2] = ChamsSettings::colorEnemy[2];
        out[3] = ChamsSettings::colorEnemy[3];
    }
}

static void ApplyTexEnv(const float color[4], bool texture) {
    if (texture) {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, 0x8581, GL_CONSTANT_ARB);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_CONSTANT_ARB);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
    } else {
        glColor4f(color[0], color[1], color[2], color[3]);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_CONSTANT_ARB);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
    }
}

// RenderEntitySimple mutates (and often pops) the attrib stack, so PushAttrib
// cannot be trusted. Force the fixed-function defaults Minecraft 1.7/1.8 expect
// at the start of the next world pass — otherwise COMBINE+constant alpha and a
// leftover depth range make every block blend like glass.
static void ResetWorldGl() {
    const float white[4] = { 1.f, 1.f, 1.f, 1.f };
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, white);
    glColor4f(1.f, 1.f, 1.f, 1.f);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDepthRange(0.0, 1.0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glEnable(GL_ALPHA_TEST);
    glDisable(GL_COLOR_MATERIAL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Chams::OnRender(JNIEnv* env) {
    if (!enabled || !env) return;
    if (!ChamsSettings::glowMode && !ChamsSettings::renderTexture) return;
    if (g_renderingChams) return;

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    jobject rmObj = Minecraft::GetRenderManager(env);
    if (!playerObj || !worldObj || !rmObj) return;

    BuildChamsClasses(env);

    float partial = 0.f;
    jobject timerObj = Minecraft::GetTimer(env);
    if (timerObj) {
        partial = ((Timer*)timerObj)->GetRenderPartialTicks(env);
        env->DeleteLocalRef(timerObj);
    }

    auto proj = ActiveRenderInfo::GetProjection(env);
    auto mv = ActiveRenderInfo::GetModelView(env);
    if (proj.size() < 16 || mv.size() < 16) return;

    auto entities = ((World*)worldObj)->GetLoadedEntities(env);
    auto* rm = (RenderManager*)rmObj;

    GLint prevMatrixMode = GL_MODELVIEW;
    glGetIntegerv(GL_MATRIX_MODE, &prevMatrixMode);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(proj.data());
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(mv.data());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_FOG);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    g_renderingChams = true;

    for (jobject e : entities) {
        if (!e) continue;
        if (s_cc.living && !env->IsInstanceOf(e, s_cc.living)) { env->DeleteLocalRef(e); continue; }
        if (env->IsSameObject(e, playerObj)) { env->DeleteLocalRef(e); continue; }

        auto* ent = (Player*)e;
        if (ent->IsDead(env)) { env->DeleteLocalRef(e); continue; }
        if (AntiBot_IsBot(env, e)) { env->DeleteLocalRef(e); continue; }
        if (ent->IsInvisible(env) && !ChamsSettings::invisibles) { env->DeleteLocalRef(e); continue; }
        if (!PassesFilter(env, e)) { env->DeleteLocalRef(e); continue; }

        if (ChamsSettings::hideFriends && FriendsSettings::IsFriend(env, ent)) {
            env->DeleteLocalRef(e); continue;
        }
        if (ChamsSettings::enemiesOnly && s_cc.player && env->IsInstanceOf(e, s_cc.player)
            && !EnemiesSettings::IsEnemy(env, ent)) {
            env->DeleteLocalRef(e); continue;
        }

        float col[4];
        GetEntityColor(env, e, col);

        if (ChamsSettings::glowMode) {
            glColor4f(1.f, 1.f, 1.f, 1.f);
            rm->RenderEntitySimple(e, partial, env);
        } else {
            glDisable(GL_LIGHTING);
            ApplyTexEnv(col, true);
            rm->RenderEntitySimple(e, partial, env);
        }

        env->DeleteLocalRef(e);
    }

    g_renderingChams = false;

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(prevMatrixMode);
    ResetWorldGl();
}
