#include "pch.h"
#include "PlayerEsp.h"

#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Classes/Timer.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Game/Method.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"
#include "../Misc/Friends.h"
#include "../Misc/Enemies.h"

#include "../../../../vendors/imgui/imgui.h"
#include <gl/GL.h>
#pragma comment(lib, "opengl32.lib")
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

struct PespEnch { int id; int lvl; };
struct PespPot { int id; int duration; int amp; };
struct PespSlot {
    int itemId = -1;
    int meta = 0;
    bool enchanted = false;
    std::string name;
    std::vector<PespEnch> enchants;
    int count = 0;
};
struct PespPlayer {
    Vec3 feet{};
    Vec3 head{};
    float bodyYaw = 0.f;
    float headYaw = 0.f;
    float limbSwing = 0.f;
    float limbSwingAmount = 0.f;
    float distance = 0.f;
    int entityId = 0;
    PespSlot armor[4]{};
    PespSlot held{};
    int gappleCount = 0;
    int potionInvCount = 0;
    std::vector<PespPot> potions;
};

static std::vector<PespPlayer> s_players;
static std::vector<float> s_mv, s_proj;

static const int kArmorEnch[] = { 0, 1, 2, 3, 4, 7, 34 };
static const int kWeaponEnch[] = { 16, 17, 18, 19, 20, 21, 32, 33, 34, 35, 48, 49, 50, 51 };

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static float ReadFloat(JNIEnv* env, jobject obj, const char* key) {
    Klass* cls = (Klass*)env->GetObjectClass(obj);
    if (!cls) return 0.f;
    Field* f = cls->GetField(env, Mapper::Get(key).c_str(), "F");
    if (env->ExceptionCheck()) { env->ExceptionClear(); f = nullptr; }
    env->DeleteLocalRef((jclass)cls);
    return f ? f->GetFloatField(env, obj) : 0.f;
}

#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0BC0
#endif

static const char* PotionTexturePath(int meta) {
    if (meta & 16384)
        return "textures/items/potion_bottle_splash.png";
    return "textures/items/potion_bottle_drinkable.png";
}

static const char* ItemTexturePath(int id) {
    switch (id) {
        case 298: return "textures/items/leather_helmet.png";
        case 299: return "textures/items/leather_chestplate.png";
        case 300: return "textures/items/leather_leggings.png";
        case 301: return "textures/items/leather_boots.png";
        case 302: return "textures/items/chainmail_helmet.png";
        case 303: return "textures/items/chainmail_chestplate.png";
        case 304: return "textures/items/chainmail_leggings.png";
        case 305: return "textures/items/chainmail_boots.png";
        case 306: return "textures/items/iron_helmet.png";
        case 307: return "textures/items/iron_chestplate.png";
        case 308: return "textures/items/iron_leggings.png";
        case 309: return "textures/items/iron_boots.png";
        case 310: return "textures/items/diamond_helmet.png";
        case 311: return "textures/items/diamond_chestplate.png";
        case 312: return "textures/items/diamond_leggings.png";
        case 313: return "textures/items/diamond_boots.png";
        case 314: return "textures/items/gold_helmet.png";
        case 315: return "textures/items/gold_chestplate.png";
        case 316: return "textures/items/gold_leggings.png";
        case 317: return "textures/items/gold_boots.png";
        case 268: return "textures/items/wood_sword.png";
        case 272: return "textures/items/stone_sword.png";
        case 267: return "textures/items/iron_sword.png";
        case 276: return "textures/items/diamond_sword.png";
        case 283: return "textures/items/gold_sword.png";
        case 271: return "textures/items/wood_axe.png";
        case 275: return "textures/items/stone_axe.png";
        case 258: return "textures/items/iron_axe.png";
        case 279: return "textures/items/diamond_axe.png";
        case 286: return "textures/items/gold_axe.png";
        case 261: return "textures/items/bow_standby.png";
        case 346: return "textures/items/fishing_rod_uncast.png";
        case 322: return "textures/items/apple_golden.png";
        case 368: return "textures/items/ender_pearl.png";
        case 373: return "textures/items/potion_bottle_drinkable.png";
        case 282: return "textures/items/mushroom_stew.png";
        case 332: return "textures/items/snowball.png";
        case 344: return "textures/items/egg.png";
        case 262: return "textures/items/arrow.png";
        case 280: return "textures/items/stick.png";
        default: return nullptr;
    }
}

static bool BindMcTexture(JNIEnv* env, const char* path) {
    if (!env || !path) return false;
    JniOk(env);
    jobject mc = Minecraft::GetTheMinecraft(env);
    if (!mc) return false;
    Klass* mcCls = g_Instance->FindClass(Mapper::Get("net/minecraft/client/Minecraft"));
    if (!mcCls) return false;
    Field* fEng = mcCls->GetField(env, Mapper::Get("renderEngine").c_str(),
        Mapper::Get("net/minecraft/client/renderer/texture/TextureManager", 2).c_str());
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    if (!fEng) return false;
    jobject tm = fEng->GetObjectField(env, mc);
    if (!tm) return false;
    Klass* rlCls = g_Instance->FindClass(Mapper::Get("net/minecraft/util/ResourceLocation"));
    if (!rlCls) { env->DeleteLocalRef(tm); return false; }
    jmethodID ctor = env->GetMethodID((jclass)rlCls, "<init>", "(Ljava/lang/String;)V");
    if (env->ExceptionCheck()) { env->ExceptionClear(); ctor = nullptr; }
    if (!ctor) { env->DeleteLocalRef(tm); return false; }
    jstring jpath = env->NewStringUTF(path);
    jobject loc = env->NewObject((jclass)rlCls, ctor, jpath);
    env->DeleteLocalRef(jpath);
    if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(tm); return false; }
    if (!loc) { env->DeleteLocalRef(tm); return false; }
    Klass* tmCls = g_Instance->FindClass(Mapper::Get("net/minecraft/client/renderer/texture/TextureManager"));
    if (!tmCls) { env->DeleteLocalRef(loc); env->DeleteLocalRef(tm); return false; }
    std::string sig = "(" + Mapper::Get("net/minecraft/util/ResourceLocation", 2) + ")V";
    Method* bind = tmCls->GetMethod(env, Mapper::Get("bindTexture").c_str(), sig.c_str());
    if (env->ExceptionCheck()) { env->ExceptionClear(); bind = nullptr; }
    if (!bind) { env->DeleteLocalRef(loc); env->DeleteLocalRef(tm); return false; }
    bind->CallVoidMethod(env, tm, false, loc);
    bool ok = !env->ExceptionCheck();
    if (!ok) env->ExceptionClear();
    env->DeleteLocalRef(loc);
    env->DeleteLocalRef(tm);
    return ok;
}

static void DrawTexturedQuad(float x, float y, float sz) {
    glBegin(GL_QUADS);
    glTexCoord2f(0.f, 0.f); glVertex2f(x, y);
    glTexCoord2f(1.f, 0.f); glVertex2f(x + sz, y);
    glTexCoord2f(1.f, 1.f); glVertex2f(x + sz, y + sz);
    glTexCoord2f(0.f, 1.f); glVertex2f(x, y + sz);
    glEnd();
}

static const char* EnchantAbbrev(int id) {
    switch (id) {
        case 0:  return "P";   case 1:  return "FP";  case 2:  return "FF";
        case 3:  return "BP";  case 4:  return "PP";  case 7:  return "T";
        case 16: return "S";   case 17: return "Sm";  case 18: return "BA";
        case 19: return "KB";  case 20: return "FA";  case 21: return "Lo";
        case 32: return "E";   case 33: return "ST";  case 34: return "U";
        case 35: return "F";   case 48: return "Pw";  case 49: return "Pu";
        case 50: return "Fl";  case 51: return "Inf"; default: return "?";
    }
}

static std::string FormatDuration(int ticks) {
    int s = ticks / 20;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
    return buf;
}

static const char* PotionEffectName(int id) {
    switch (id) {
        case 1:  return "Speed";
        case 2:  return "Slow";
        case 3:  return "Haste";
        case 4:  return "Fatigue";
        case 5:  return "Strength";
        case 6:  return "Heal";
        case 7:  return "Harm";
        case 8:  return "Jump";
        case 9:  return "Nausea";
        case 10: return "Regen";
        case 11: return "Res";
        case 12: return "FireRes";
        case 13: return "Water";
        case 14: return "Invis";
        case 15: return "Blind";
        case 16: return "NV";
        case 17: return "Hunger";
        case 18: return "Weak";
        case 19: return "Poison";
        case 20: return "Wither";
        case 21: return "HP+";
        case 22: return "Abs";
        case 23: return "Sat";
        default: return nullptr;
    }
}

static ImU32 MaterialColor(int id) {
    if (id >= 310 && id <= 313) return IM_COL32(100, 220, 255, 255);
    if (id >= 306 && id <= 309) return IM_COL32(200, 200, 200, 255);
    if (id >= 314 && id <= 317) return IM_COL32(255, 215, 50, 255);
    if (id >= 302 && id <= 305) return IM_COL32(160, 160, 170, 255);
    if (id >= 298 && id <= 301) return IM_COL32(160, 100, 60, 255);
    if (id == 276) return IM_COL32(100, 220, 255, 255);
    if (id == 267) return IM_COL32(200, 200, 200, 255);
    if (id == 283) return IM_COL32(255, 215, 50, 255);
    if (id == 272) return IM_COL32(160, 160, 160, 255);
    if (id == 268) return IM_COL32(180, 140, 80, 255);
    if (id == 261) return IM_COL32(150, 110, 60, 255);
    if (id == 322) return IM_COL32(255, 180, 50, 255);
    return IM_COL32(180, 180, 180, 255);
}

static void FillEnchants(ItemStack* st, JNIEnv* env, const int* ids, int n, std::vector<PespEnch>& out) {
    for (int i = 0; i < n; i++) {
        int lvl = st->GetEnchantmentLevel(ids[i], env);
        if (lvl > 0) out.push_back({ ids[i], lvl });
    }
}

static PespSlot ReadStack(jobject stackObj, JNIEnv* env, bool weapon) {
    PespSlot s;
    if (!stackObj) return s;
    auto* st = (ItemStack*)stackObj;
    s.itemId = st->GetItemId(env);
    JniOk(env);
    s.meta = st->GetMetadata(env);
    JniOk(env);
    s.enchanted = st->IsEnchanted(env);
    JniOk(env);
    s.count = st->GetStackSize(env);
    JniOk(env);
    if (s.enchanted) {
        if (weapon) FillEnchants(st, env, kWeaponEnch, (int)(sizeof(kWeaponEnch) / sizeof(kWeaponEnch[0])), s.enchants);
        else FillEnchants(st, env, kArmorEnch, (int)(sizeof(kArmorEnch) / sizeof(kArmorEnch[0])), s.enchants);
    }
    return s;
}

static void CollectPotions(JNIEnv* env, jobject entity, std::vector<PespPot>& out) {
    if (!env || !entity) return;
    JniOk(env);
    Klass* living = (Klass*)env->GetObjectClass(entity);
    if (!living) return;
    Method* m = living->GetMethod(env, Mapper::Get("getActivePotionEffects").c_str(), "()Ljava/util/Collection;");
    env->DeleteLocalRef((jclass)living);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!m) return;
    jobject coll = m->CallObjectMethod(env, entity);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!coll) return;
    jclass cc = env->FindClass("java/util/Collection");
    if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(coll); return; }
    if (!cc) { env->DeleteLocalRef(coll); return; }
    jmethodID iterId = env->GetMethodID(cc, "iterator", "()Ljava/util/Iterator;");
    env->DeleteLocalRef(cc);
    jobject iter = iterId ? env->CallObjectMethod(coll, iterId) : nullptr;
    env->DeleteLocalRef(coll);
    if (!iter) return;
    jclass ic = env->FindClass("java/util/Iterator");
    if (!ic) { env->DeleteLocalRef(iter); return; }
    jmethodID hasN = env->GetMethodID(ic, "hasNext", "()Z");
    jmethodID next = env->GetMethodID(ic, "next", "()Ljava/lang/Object;");
    env->DeleteLocalRef(ic);
    Klass* peCls = g_Instance->FindClass(Mapper::Get("net/minecraft/potion/PotionEffect"));
    Method* gp = peCls ? peCls->GetMethod(env, Mapper::Get("getPotionID").c_str(), "()I") : nullptr;
    Method* gd = peCls ? peCls->GetMethod(env, Mapper::Get("getDuration").c_str(), "()I") : nullptr;
    Method* ga = peCls ? peCls->GetMethod(env, Mapper::Get("getAmplifier").c_str(), "()I") : nullptr;
    while (hasN && next && env->CallBooleanMethod(iter, hasN)) {
        jobject pe = env->CallObjectMethod(iter, next);
        if (!pe || !gp || !gd || !ga) { if (pe) env->DeleteLocalRef(pe); continue; }
        out.push_back({ gp->CallIntMethod(env, pe), gd->CallIntMethod(env, pe), ga->CallIntMethod(env, pe) });
        env->DeleteLocalRef(pe);
    }
    env->DeleteLocalRef(iter);
}

static bool W2S(const Vec3& w, Vec2& s) {
    return WorldToScreen(w, s, s_mv, s_proj,
        (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
}

void PlayerEsp::OnRender(JNIEnv* env) {
    (void)env;
}

static void CollectPlayerEsp(JNIEnv* env) {
    s_players.clear();
    if (!env) return;
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
        if (PlayerEspSettings::hideFriends && FriendsSettings::IsFriend(env, ent)) {
            env->DeleteLocalRef(e); continue;
        }
        if (PlayerEspSettings::enemiesOnly && !EnemiesSettings::IsEnemy(env, ent)) {
            env->DeleteLocalRef(e); continue;
        }

        Vec3D pos = ent->GetPos(env);
        Vec3D last = ent->GetLastTickPos(env);
        float ix = (float)(last.x + (pos.x - last.x) * (double)partial - cam.x);
        float iy = (float)(last.y + (pos.y - last.y) * (double)partial - cam.y);
        float iz = (float)(last.z + (pos.z - last.z) * (double)partial - cam.z);

        PespPlayer pd;
        pd.feet = { ix, iy, iz };
        pd.head = { ix, iy + 2.2f, iz };
        float dx = pd.head.x - localPos.x, dy = pd.head.y - localPos.y, dz = pd.head.z - localPos.z;
        pd.distance = sqrtf(dx * dx + dy * dy + dz * dz);
        if (pd.distance > PlayerEspSettings::maxRenderDistance) { env->DeleteLocalRef(e); continue; }

        pd.entityId = ent->GetEntityId(env);
        float bodyNow = ent->GetRenderYawOffset(env);
        float bodyPrev = ent->GetPrevRenderYawOffset(env);
        float delta = bodyNow - bodyPrev;
        while (delta >= 180.f) delta -= 360.f;
        while (delta < -180.f) delta += 360.f;
        pd.bodyYaw = bodyPrev + delta * partial;
        pd.headYaw = ent->GetRotationYawHead(env);
        float lsaNow = ReadFloat(env, e, "limbSwingAmount");
        float lsaPrev = ReadFloat(env, e, "prevLimbSwingAmount");
        pd.limbSwingAmount = lsaPrev + (lsaNow - lsaPrev) * partial;
        pd.limbSwing = ReadFloat(env, e, "limbSwing") - lsaNow * (1.f - partial);

        int gapples = 0, pots = 0;
        jobject invObj = ent->GetInventoryPlayer(env);
        JniOk(env);
        if (invObj) {
            auto* inv = (InventoryPlayer*)invObj;
            for (int slot = 0; slot < 36; slot++) {
                jobject st = inv->GetStackInSlot(slot, env);
                JniOk(env);
                if (!st) continue;
                auto* stack = (ItemStack*)st;
                int id = stack->GetItemId(env);
                JniOk(env);
                int n = stack->GetStackSize(env);
                JniOk(env);
                if (id == 322 && n > 0) gapples += n;
                else if (id == 373 && n > 0) pots += n;
                env->DeleteLocalRef(st);
            }
            if (PlayerEspSettings::armor) {
                for (int i = 0; i < 4; i++) {
                    jobject st = inv->GetStackInSlot(36 + i, env);
                    JniOk(env);
                    if (st) {
                        pd.armor[i] = ReadStack(st, env, false);
                        JniOk(env);
                        env->DeleteLocalRef(st);
                    }
                }
            }
            env->DeleteLocalRef(invObj);
        }
        pd.gappleCount = gapples;
        pd.potionInvCount = pots;

        if (PlayerEspSettings::heldItem) {
            jobject held = ent->GetHeldItem(env);
            JniOk(env);
            if (held) {
                pd.held = ReadStack(held, env, true);
                JniOk(env);
                env->DeleteLocalRef(held);
            }
        }

        if (PlayerEspSettings::potions) {
            CollectPotions(env, e, pd.potions);
            JniOk(env);
        }

        s_players.push_back(std::move(pd));
        env->DeleteLocalRef(e);
    }
    JniOk(env);
}

static void RenderSkeleton(ImDrawList* dl, const PespPlayer& p) {
    constexpr float HEAD_TOP = 1.875f, NECK = 1.406f, SHOULDER_Y = 1.406f;
    constexpr float HAND_Y = 0.703f, HIP_Y = 0.703f, FOOT_Y = 0.f;
    constexpr float SHOULDER_OFFSET = 0.293f, HIP_OFFSET = 0.111f;
    float yawRad = (180.f - p.bodyYaw) * 0.01745329f;
    float cosY = cosf(yawRad), sinY = sinf(yawRad);
    const Vec3& f = p.feet;
    const float lsa = (std::min)(p.limbSwingAmount, 1.f);
    const float lsPhase = p.limbSwing * 0.6662f;
    const float cosPhase = cosf(lsPhase);
    const float cosPhasePi = cosf(lsPhase + 3.14159265f);
    const float armLen = SHOULDER_Y - HAND_Y;
    const float legLen = HIP_Y - FOOT_Y;
    const bool hasHeld = p.held.itemId >= 0;
    auto limbEnd = [&](float offsetX, float limbLen, float pivotY, float swingAng) {
        float c = cosf(swingAng), sn = sinf(swingAng);
        float localY = pivotY - limbLen * c;
        float localZ = -limbLen * sn;
        return Vec3{
            f.x + offsetX * cosY + localZ * sinY,
            f.y + localY,
            f.z + (-offsetX * sinY + localZ * cosY)
        };
    };
    Vec3 head{ f.x, f.y + HEAD_TOP, f.z };
    Vec3 neck{ f.x, f.y + NECK, f.z };
    Vec3 shL{ f.x + SHOULDER_OFFSET * cosY, f.y + SHOULDER_Y, f.z - SHOULDER_OFFSET * sinY };
    Vec3 shR{ f.x - SHOULDER_OFFSET * cosY, f.y + SHOULDER_Y, f.z + SHOULDER_OFFSET * sinY };
    Vec3 hip{ f.x, f.y + HIP_Y, f.z };
    Vec3 hiL{ f.x + HIP_OFFSET * cosY, f.y + HIP_Y, f.z - HIP_OFFSET * sinY };
    Vec3 hiR{ f.x - HIP_OFFSET * cosY, f.y + HIP_Y, f.z + HIP_OFFSET * sinY };
    Vec3 haL = limbEnd(+SHOULDER_OFFSET, armLen, SHOULDER_Y, cosPhase * lsa + (hasHeld ? 0.25f : 0.f));
    Vec3 haR = limbEnd(-SHOULDER_OFFSET, armLen, SHOULDER_Y, cosPhasePi * lsa);
    Vec3 foL = limbEnd(+HIP_OFFSET, legLen, HIP_Y, cosPhasePi * 1.4f * lsa);
    Vec3 foR = limbEnd(-HIP_OFFSET, legLen, HIP_Y, cosPhase * 1.4f * lsa);
    Vec2 sHead, sNeck, sShL, sShR, sHaL, sHaR, sHip, sHiL, sHiR, sFoL, sFoR;
    if (!W2S(head, sHead) || !W2S(neck, sNeck) || !W2S(shL, sShL) || !W2S(shR, sShR)
        || !W2S(haL, sHaL) || !W2S(haR, sHaR) || !W2S(hip, sHip)
        || !W2S(hiL, sHiL) || !W2S(hiR, sHiR) || !W2S(foL, sFoL) || !W2S(foR, sFoR))
        return;
    const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
        PlayerEspSettings::skeletonColor[0], PlayerEspSettings::skeletonColor[1],
        PlayerEspSettings::skeletonColor[2], PlayerEspSettings::skeletonColor[3]));
    const float th = PlayerEspSettings::skeletonThickness;
    auto line = [&](const Vec2& a, const Vec2& b) {
        dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), IM_COL32(0, 0, 0, 180), th + 1.5f);
        dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), col, th);
    };
    line(sHead, sNeck); line(sShL, sShR); line(sNeck, sHip);
    line(sShL, sHaL); line(sShR, sHaR); line(sHiL, sHiR); line(sHiL, sFoL); line(sHiR, sFoR);
    dl->AddCircleFilled(ImVec2(sHead.x, sHead.y), th + 1.5f, IM_COL32(0, 0, 0, 180));
    dl->AddCircleFilled(ImVec2(sHead.x, sHead.y), th + 0.5f, col);
}

static void RenderOutline3D(ImDrawList* dl, const PespPlayer& player) {
    enum { PT_HEAD = 0, PT_BODY = 1, PT_ARM_L = 2, PT_ARM_R = 3, PT_LEG_L = 4, PT_LEG_R = 5 };
    struct BodyPart { float cx, cy, cz, hx, hy, hz; int type; };
    static const BodyPart PARTS[] = {
        { 0.00f, 1.641f, 0.00f, 0.234f, 0.234f, 0.234f, PT_HEAD },
        { 0.00f, 1.055f, 0.00f, 0.234f, 0.352f, 0.117f, PT_BODY },
        { 0.352f, 1.055f, 0.00f, 0.117f, 0.352f, 0.117f, PT_ARM_L },
        { -0.352f, 1.055f, 0.00f, 0.117f, 0.352f, 0.117f, PT_ARM_R },
        { 0.117f, 0.352f, 0.00f, 0.117f, 0.352f, 0.117f, PT_LEG_L },
        { -0.117f, 0.352f, 0.00f, 0.117f, 0.352f, 0.117f, PT_LEG_R },
    };
    const bool hasHeld = player.held.itemId >= 0;
    const bool hasHelmet = player.armor[3].itemId >= 0;
    const bool hasChest = player.armor[2].itemId >= 0;
    const bool hasLegs = player.armor[1].itemId >= 0;
    const bool hasBoots = player.armor[0].itemId >= 0;
    auto padFor = [&](int type) {
        constexpr float ARMOR_PAD = 0.03f;
        if (type == PT_HEAD) return hasHelmet ? ARMOR_PAD : 0.f;
        if (type == PT_BODY || type == PT_ARM_L || type == PT_ARM_R) return hasChest ? ARMOR_PAD : 0.f;
        return (hasLegs || hasBoots) ? ARMOR_PAD : 0.f;
    };
    float bRad = (180.f - player.bodyYaw) * 0.01745329f;
    float bCos = cosf(bRad), bSin = sinf(bRad);
    float hRad = (180.f - player.headYaw) * 0.01745329f;
    float hCos = cosf(hRad), hSin = sinf(hRad);
    const Vec3& f = player.feet;
    constexpr float NECK_Y = 1.5f, SHOULDER_PIVOT_Y = 1.406f, HIP_PIVOT_Y = 0.703f;
    const float lsa = (std::min)(player.limbSwingAmount, 1.f);
    const float lsPhase = player.limbSwing * 0.6662f;
    const float cosPhase = cosf(lsPhase);
    const float cosPhasePi = cosf(lsPhase + 3.14159265f);
    const float armL = cosPhase * lsa, armR = cosPhasePi * lsa;
    const float legL = cosPhasePi * 1.4f * lsa, legR = cosPhase * 1.4f * lsa;
    auto rotX = [](float& ly, float& lz, float ang, float pivotY) {
        float py = ly - pivotY, c = cosf(ang), s = sinf(ang);
        ly = pivotY + py * c - lz * s;
        lz = py * s + lz * c;
    };
    const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
        PlayerEspSettings::outlineColor[0], PlayerEspSettings::outlineColor[1],
        PlayerEspSettings::outlineColor[2], PlayerEspSettings::outlineColor[3]));
    const float th = PlayerEspSettings::outlineThickness;
    static const int EDGES[12][2] = {
        {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}
    };
    for (const auto& part : PARTS) {
        float pad = padFor(part.type);
        float xs[2] = { part.cx - part.hx - pad, part.cx + part.hx + pad };
        float ys[2] = { part.cy - part.hy - pad, part.cy + part.hy + pad };
        float zs[2] = { part.cz - part.hz - pad, part.cz + part.hz + pad };
        float swing = 0.f, pivot = 0.f; bool limb = false;
        if (part.type == PT_ARM_L) { swing = armL; pivot = SHOULDER_PIVOT_Y; limb = true; }
        if (part.type == PT_ARM_R) { swing = armR; pivot = SHOULDER_PIVOT_Y; limb = true; }
        if (part.type == PT_LEG_L) { swing = legL; pivot = HIP_PIVOT_Y; limb = true; }
        if (part.type == PT_LEG_R) { swing = legR; pivot = HIP_PIVOT_Y; limb = true; }
        Vec2 screen[8];
        bool ok = true;
        for (int i = 0; i < 8; i++) {
            float lx = xs[(i >> 0) & 1], ly = ys[(i >> 1) & 1], lz = zs[(i >> 2) & 1];
            if (limb && swing != 0.f) rotX(ly, lz, swing, pivot);
            if (hasHeld && part.type == PT_ARM_L) rotX(ly, lz, 0.25f, SHOULDER_PIVOT_Y);
            Vec3 world;
            if (part.type == PT_HEAD) {
                float py = ly - NECK_Y;
                world = { f.x + (lx * hCos + lz * hSin), f.y + NECK_Y + py, f.z + (-lx * hSin + lz * hCos) };
            } else {
                world = { f.x + (lx * bCos + lz * bSin), f.y + ly, f.z + (-lx * bSin + lz * bCos) };
            }
            if (!W2S(world, screen[i])) { ok = false; break; }
        }
        if (!ok) continue;
        if (PlayerEspSettings::outlineGlow) {
            const float* oc = PlayerEspSettings::outlineColor;
            for (int gp = 5; gp >= 1; gp--) {
                float t = (float)gp / 5.f;
                ImU32 gc = ImGui::ColorConvertFloat4ToU32(ImVec4(oc[0], oc[1], oc[2], oc[3] * (1.f - t) * 0.35f));
                for (auto& e : EDGES)
                    dl->AddLine(ImVec2(screen[e[0]].x, screen[e[0]].y), ImVec2(screen[e[1]].x, screen[e[1]].y),
                        gc, th + PlayerEspSettings::outlineGlowRadius * t);
            }
        }
        for (auto& e : EDGES) {
            if (!PlayerEspSettings::outlineGlow)
                dl->AddLine(ImVec2(screen[e[0]].x, screen[e[0]].y), ImVec2(screen[e[1]].x, screen[e[1]].y),
                    IM_COL32(0, 0, 0, 180), th + 1.5f);
            dl->AddLine(ImVec2(screen[e[0]].x, screen[e[0]].y), ImVec2(screen[e[1]].x, screen[e[1]].y), col, th);
        }
    }
}

static void RenderOutline2D(ImDrawList* dl, const Vec2& head, const Vec2& feet) {
    float minX = (std::min)(head.x, feet.x) - 12.f;
    float maxX = (std::max)(head.x, feet.x) + 12.f;
    float minY = (std::min)(head.y, feet.y);
    float maxY = (std::max)(head.y, feet.y);
    const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
        PlayerEspSettings::outlineColor[0], PlayerEspSettings::outlineColor[1],
        PlayerEspSettings::outlineColor[2], PlayerEspSettings::outlineColor[3]));
    float th = PlayerEspSettings::outlineThickness;
    dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), IM_COL32(0, 0, 0, 180), 0.f, 0, th + 1.5f);
    dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), col, 0.f, 0, th);
}

static std::string EnchString(const std::vector<PespEnch>& e) {
    std::string s;
    for (auto& x : e) { s += EnchantAbbrev(x.id); s += std::to_string(x.lvl); }
    return s;
}

static void RenderArmorHud(JNIEnv* env, ImDrawList* dl, float centerX, float nametagY, float scale, const PespPlayer& p) {
    const float s = scale * PlayerEspSettings::displayScale;
    const float enchFS = (std::max)(8.f * s, 6.f);
    const float iconSz = (std::max)(16.f * s, 12.f);
    const float slotGap = 3.f * s, padX = 5.f * s, padY = 1.5f * s;
    struct Row { int itemId; int meta; std::string ench; ImU32 col; bool enchanted; };
    std::vector<Row> rows;
    for (int i = 3; i >= 0; i--) {
        if (p.armor[i].itemId < 0) continue;
        rows.push_back({ p.armor[i].itemId, p.armor[i].meta, EnchString(p.armor[i].enchants), MaterialColor(p.armor[i].itemId), p.armor[i].enchanted });
    }
    if (PlayerEspSettings::heldItem && p.held.itemId >= 0 && p.held.itemId != 322 && p.held.itemId != 373)
        rows.push_back({ p.held.itemId, p.held.meta, EnchString(p.held.enchants), MaterialColor(p.held.itemId), p.held.enchanted });
    if (PlayerEspSettings::gapple && p.gappleCount > 0)
        rows.push_back({ 322, 0, "x" + std::to_string(p.gappleCount), IM_COL32(255, 180, 50, 255), false });
    if (PlayerEspSettings::potions && p.potionInvCount > 0)
        rows.push_back({ 373, 0, "x" + std::to_string(p.potionInvCount), IM_COL32(140, 80, 200, 255), false });
    if (rows.empty()) return;

    bool anyEnch = false;
    float totalW = padX;
    std::vector<float> slotW;
    for (size_t i = 0; i < rows.size(); i++) {
        float w = iconSz;
        if (!rows[i].ench.empty()) {
            anyEnch = true;
            float sc = enchFS / ImGui::GetFontSize();
            w = (std::max)(w, ImGui::CalcTextSize(rows[i].ench.c_str()).x * sc);
        }
        slotW.push_back(w);
        if (i) totalW += slotGap;
        totalW += w;
    }
    totalW += padX;
    const float enchH = anyEnch ? enchFS : 0.f;
    const float barH = (std::max)(1.f, s);
    const float rowH = padY + enchH + iconSz + barH + padY;
    const float x0 = centerX - totalW * 0.5f;
    const float y1 = nametagY - 2.f;
    const float y0 = y1 - rowH;
    const float iconY = y0 + padY + enchH;

    const ImGuiIO& io = ImGui::GetIO();
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, io.DisplaySize.x, io.DisplaySize.y, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float cur = x0 + padX;
    for (size_t i = 0; i < rows.size(); i++) {
        float ix = cur + (slotW[i] - iconSz) * 0.5f;
        const char* path = (rows[i].itemId == 373) ? PotionTexturePath(rows[i].meta) : ItemTexturePath(rows[i].itemId);
        if (env && path && BindMcTexture(env, path)) {
            glEnable(GL_TEXTURE_2D);
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.001f);
            glColor4f(1.f, 1.f, 1.f, 1.f);
            DrawTexturedQuad(ix, iconY, iconSz);
            if (rows[i].enchanted) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                float t = (float)(GetTickCount() % 2000) / 2000.f;
                float pulse = 0.3f + 0.15f * sinf(t * 6.2831853f);
                glColor4f(0.5f, 0.2f, 1.f, pulse);
                DrawTexturedQuad(ix, iconY, iconSz);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            glDisable(GL_ALPHA_TEST);
            glDisable(GL_TEXTURE_2D);
        } else {
            float c[4] = {
                ((rows[i].col >> IM_COL32_R_SHIFT) & 0xFF) / 255.f,
                ((rows[i].col >> IM_COL32_G_SHIFT) & 0xFF) / 255.f,
                ((rows[i].col >> IM_COL32_B_SHIFT) & 0xFF) / 255.f,
                1.f
            };
            glDisable(GL_TEXTURE_2D);
            glColor4f(c[0], c[1], c[2], 1.f);
            glBegin(GL_QUADS);
            glVertex2f(ix, iconY); glVertex2f(ix + iconSz, iconY);
            glVertex2f(ix + iconSz, iconY + iconSz); glVertex2f(ix, iconY + iconSz);
            glEnd();
        }
        float bc[4] = {
            ((rows[i].col >> IM_COL32_R_SHIFT) & 0xFF) / 255.f,
            ((rows[i].col >> IM_COL32_G_SHIFT) & 0xFF) / 255.f,
            ((rows[i].col >> IM_COL32_B_SHIFT) & 0xFF) / 255.f,
            0.55f
        };
        glDisable(GL_TEXTURE_2D);
        glColor4f(bc[0], bc[1], bc[2], bc[3]);
        glBegin(GL_QUADS);
        glVertex2f(ix, iconY + iconSz); glVertex2f(ix + iconSz, iconY + iconSz);
        glVertex2f(ix + iconSz, iconY + iconSz + barH); glVertex2f(ix, iconY + iconSz + barH);
        glEnd();
        cur += slotW[i] + slotGap;
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();

    if (anyEnch) {
        cur = x0 + padX;
        float sc = enchFS / ImGui::GetFontSize();
        for (size_t i = 0; i < rows.size(); i++) {
            if (!rows[i].ench.empty()) {
                ImVec2 es = ImGui::CalcTextSize(rows[i].ench.c_str());
                dl->AddText(ImGui::GetFont(), enchFS,
                    ImVec2(cur + (slotW[i] - es.x * sc) * 0.5f, iconY - enchFS),
                    IM_COL32(85, 255, 255, 255), rows[i].ench.c_str());
            }
            cur += slotW[i] + slotGap;
        }
    }
}

static void RenderPotions(ImDrawList* dl, float centerX, float aboveY, float scale, const PespPlayer& p) {
    if (p.potions.empty()) return;
    const float s = scale * PlayerEspSettings::displayScale;
    const float fs = (std::max)(8.f * s, 6.f);
    const float padX = 4.f * s, padY = 2.f * s, pillGap = 3.f * s;
    std::vector<std::string> texts;
    float totalW = padX;
    for (size_t i = 0; i < p.potions.size(); i++) {
        char buf[48];
        if (const char* n = PotionEffectName(p.potions[i].id))
            snprintf(buf, sizeof(buf), "%s %s", n, FormatDuration(p.potions[i].duration).c_str());
        else
            snprintf(buf, sizeof(buf), "#%d %s", p.potions[i].id, FormatDuration(p.potions[i].duration).c_str());
        texts.emplace_back(buf);
        float w = ImGui::CalcTextSize(buf).x * (fs / ImGui::GetFontSize()) + padX * 2.f;
        if (i) totalW += pillGap;
        totalW += w;
    }
    totalW += padX;
    const float pillH = fs + padY * 2.f;
    const float x0 = centerX - totalW * 0.5f;
    const float y1 = aboveY - 2.f;
    const float y0 = y1 - pillH;
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + totalW, y1), IM_COL32(0, 0, 0, 178), 3.f);
    float cur = x0 + padX;
    float sc = fs / ImGui::GetFontSize();
    for (size_t i = 0; i < texts.size(); i++) {
        float tw = ImGui::CalcTextSize(texts[i].c_str()).x * sc;
        dl->AddText(ImGui::GetFont(), fs, ImVec2(cur, y0 + padY), IM_COL32(230, 230, 230, 255), texts[i].c_str());
        cur += tw + padX * 2.f + pillGap;
    }
}

void PlayerEsp::OnImGuiRender(JNIEnv* env) {
    if (!enabled || !env) return;
    CollectPlayerEsp(env);
    JniOk(env);
    if (!enabled || s_players.empty()) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    const float sh = ImGui::GetIO().DisplaySize.y;

    for (const auto& p : s_players) {
        Vec2 nametag, head, feet;
        const bool haveTag = W2S(p.head, nametag);
        const bool haveHead = W2S(Vec3{ p.head.x, p.head.y + 0.3f, p.head.z }, head);
        const bool haveFeet = W2S(p.feet, feet);
        if (!haveTag && !haveHead && !haveFeet) continue;
        if (!haveTag) {
            if (haveHead) nametag = head;
            else nametag = feet;
        }
        float boxH, centerX;
        if (haveHead && haveFeet) {
            boxH = fabsf(feet.y - head.y);
            centerX = (head.x + feet.x) * 0.5f;
        } else {
            const float d = (std::max)(p.distance, 0.5f);
            boxH = (std::max)(50.f, (std::min)(1.8f / d * sh * 0.7f, sh * 0.9f));
            centerX = nametag.x;
        }
        float scale = boxH / (0.33038348082f * sh);
        if (scale < 0.2f) scale = 0.2f;
        if (scale > 1.f) scale = 1.f;

        if (PlayerEspSettings::outline) {
            if (PlayerEspSettings::outlineMode == 1 && haveHead && haveFeet)
                RenderOutline2D(dl, head, feet);
            else
                RenderOutline3D(dl, p);
        }
        if (PlayerEspSettings::skeleton)
            RenderSkeleton(dl, p);

        float armorTop = nametag.y;
        if (PlayerEspSettings::armor || PlayerEspSettings::heldItem
            || (PlayerEspSettings::gapple && p.gappleCount > 0)
            || (PlayerEspSettings::potions && p.potionInvCount > 0)) {
            RenderArmorHud(env, dl, centerX, nametag.y, scale, p);
            const float s = scale * PlayerEspSettings::displayScale;
            armorTop = nametag.y - 2.f - ((std::max)(14.f * s, 10.f) + 8.f * s + 12.f);
        }
        if (PlayerEspSettings::potions)
            RenderPotions(dl, centerX, armorTop, scale, p);
    }
}
