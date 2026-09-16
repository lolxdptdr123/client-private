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

#include "../../../../vendors/imgui/imgui.h"
#include <gl/GL.h>
#pragma comment(lib, "opengl32.lib")
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <cstring>

struct PespEnch { int id; int lvl; };
struct PespPot { int id; int duration; int amp; };
struct PespSlot {
    int itemId = -1;
    int meta = 0;
    bool enchanted = false;
    bool splash = false;
    int tintRgb = -1;
    std::string name;
    std::vector<PespEnch> enchants;
    int count = 0;
    jobject stackRef = nullptr;
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
    std::vector<PespSlot> invPots;
    std::vector<PespPot> potions;
};

static std::vector<PespPlayer> s_players;
static std::vector<float> s_mv, s_proj;

static const int kArmorEnch[] = { 0, 1, 2, 3, 4, 7, 34 };
static const int kWeaponEnch[] = { 16, 17, 18, 19, 20, 21, 32, 33, 34, 35, 48, 49, 50, 51 };
static const int kToolEnch[] = { 32, 33, 34, 35 };

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

static const char* PotionTexturePath(bool splash) {
    if (splash)
        return "textures/items/potion_bottle_splash.png";
    return "textures/items/potion_bottle_drinkable.png";
}

static int PotionLiquidColor(int meta) {
    switch (meta & 15) {
        case 1:  return 0xCD5CAB; // regen
        case 2:  return 0x7CAFC6; // speed
        case 3:  return 0xE49A3A; // fire res
        case 4:  return 0x4E9331; // poison
        case 5:  return 0xF82423; // instant health
        case 6:  return 0x1F1FA1; // night vision
        case 7:  return 0x7F8392; // (unused / clear)
        case 8:  return 0x484D48; // weakness
        case 9:  return 0x932423; // strength
        case 10: return 0x5A6C81; // slowness
        case 11: return 0x22FF4C; // jump
        case 12: return 0x430A09; // harming
        case 13: return 0x2E5299; // water breathing
        case 14: return 0x7F8392; // invis
        default: return 0x385DC6; // water / mundane
    }
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
        case 270: return "textures/items/wood_pickaxe.png";
        case 274: return "textures/items/stone_pickaxe.png";
        case 257: return "textures/items/iron_pickaxe.png";
        case 278: return "textures/items/diamond_pickaxe.png";
        case 285: return "textures/items/gold_pickaxe.png";
        case 269: return "textures/items/wood_shovel.png";
        case 273: return "textures/items/stone_shovel.png";
        case 256: return "textures/items/iron_shovel.png";
        case 277: return "textures/items/diamond_shovel.png";
        case 284: return "textures/items/gold_shovel.png";
        case 290: return "textures/items/wood_hoe.png";
        case 291: return "textures/items/stone_hoe.png";
        case 292: return "textures/items/iron_hoe.png";
        case 293: return "textures/items/diamond_hoe.png";
        case 294: return "textures/items/gold_hoe.png";
        case 261: return "textures/items/bow_standby.png";
        case 346: return "textures/items/fishing_rod_uncast.png";
        case 359: return "textures/items/shears.png";
        case 259: return "textures/items/flint_and_steel.png";
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

static jobject CallItemStackI(JNIEnv* env, jobject obj, const char* classKey, const char* methodKey, int arg) {
    if (!env || !obj) return nullptr;
    Klass* k = g_Instance->FindClass(Mapper::Get(classKey));
    jclass localCls = nullptr;
    if (!k) {
        localCls = env->GetObjectClass(obj);
        k = (Klass*)localCls;
    }
    if (!k) return nullptr;
    std::string sig = "(I)" + Mapper::Get("net/minecraft/item/ItemStack", 2);
    Method* m = k->GetMethod(env, Mapper::Get(methodKey).c_str(), sig.c_str());
    if (env->ExceptionCheck()) { env->ExceptionClear(); m = nullptr; }
    jobject r = nullptr;
    if (m) {
        r = m->CallObjectMethod(env, obj, false, arg);
        if (env->ExceptionCheck()) { env->ExceptionClear(); r = nullptr; }
    }
    if (localCls) env->DeleteLocalRef(localCls);
    return r;
}

static bool BindMcTexture(JNIEnv* env, const char* path);

static jobject GetArmorStack(JNIEnv* env, Player* ent, InventoryPlayer* inv, int armorIndex) {
    jobject st = CallItemStackI(env, (jobject)ent, "net/minecraft/entity/EntityLivingBase", "getEquipmentInSlot", armorIndex + 1);
    if (!st) st = CallItemStackI(env, (jobject)ent, "net/minecraft/entity/player/EntityPlayer", "getEquipmentInSlot", armorIndex + 1);
    if (!st) st = CallItemStackI(env, (jobject)ent, "net/minecraft/entity/EntityLivingBase", "getCurrentArmor", armorIndex);
    if (!st && inv) st = inv->GetArmorItem(armorIndex, env);
    if (!st && inv) st = inv->GetStackInSlot(36 + armorIndex, env);
    return st;
}

static jobject GetHeldStack(JNIEnv* env, Player* ent) {
    jobject held = ent->GetHeldItem(env);
    if (held) return held;
    held = CallItemStackI(env, (jobject)ent, "net/minecraft/entity/EntityLivingBase", "getEquipmentInSlot", 0);
    if (held) return held;
    return CallItemStackI(env, (jobject)ent, "net/minecraft/entity/player/EntityPlayer", "getEquipmentInSlot", 0);
}

static bool BindMcTexture(JNIEnv* env, const char* path);

static ImTextureID GetMcTexId(JNIEnv* env, const char* path) {
    const ImTextureID none = (ImTextureID)0;
    if (!env || !path) return none;
    static std::unordered_map<std::string, ImTextureID> cache;
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    if (!BindMcTexture(env, path)) return none;
    GLint tex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    if (tex <= 0) return none;
    ImTextureID id = (ImTextureID)(intptr_t)tex;
    cache.emplace(path, id);
    return id;
}

static const char* ItemAbbrev(int id) {
    switch (id) {
        case 298: case 302: case 306: case 310: case 314: return "Helm";
        case 299: case 303: case 307: case 311: case 315: return "Chest";
        case 300: case 304: case 308: case 312: case 316: return "Legs";
        case 301: case 305: case 309: case 313: case 317: return "Boots";
        case 276: return "DiaSw";
        case 267: return "IronSw";
        case 283: return "GoldSw";
        case 268: return "WoodSw";
        case 272: return "StnSw";
        case 261: return "Bow";
        case 322: return "Gap";
        case 373: return "Pot";
        case 368: return "Pearl";
        default: return nullptr;
    }
}
static bool IsArmorItem(int id) { return id >= 298 && id <= 317; }
static bool IsToolOrWeapon(int id) {
    if (id == 261 || id == 346 || id == 359 || id == 259) return true;
    if (id == 267 || id == 268 || id == 272 || id == 276 || id == 283) return true;
    if (id == 258 || id == 271 || id == 275 || id == 279 || id == 286) return true;
    if (id == 256 || id == 257 || id == 269 || id == 270 || id == 273 || id == 274) return true;
    if (id == 277 || id == 278 || id == 284 || id == 285) return true;
    if (id >= 290 && id <= 294) return true;
    return false;
}

static jobject GetTextureManager(JNIEnv* env) {
    jobject mc = Minecraft::GetTheMinecraft(env);
    if (!mc) return nullptr;
    Klass* mcCls = g_Instance->FindClass(Mapper::Get("net/minecraft/client/Minecraft"));
    if (!mcCls) { env->DeleteLocalRef(mc); return nullptr; }
    Field* fEng = mcCls->GetField(env, Mapper::Get("renderEngine").c_str(),
        Mapper::Get("net/minecraft/client/renderer/texture/TextureManager", 2).c_str());
    if (env->ExceptionCheck()) { env->ExceptionClear(); fEng = nullptr; }
    jobject tm = fEng ? fEng->GetObjectField(env, mc) : nullptr;
    env->DeleteLocalRef(mc);
    return tm;
}

static jobject MakeResourceLocation(JNIEnv* env, const char* path) {
    Klass* rlCls = g_Instance->FindClass(Mapper::Get("net/minecraft/util/ResourceLocation"));
    if (!rlCls || !path) return nullptr;
    jmethodID ctor2 = env->GetMethodID((jclass)rlCls, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (env->ExceptionCheck()) { env->ExceptionClear(); ctor2 = nullptr; }
    if (ctor2) {
        jstring domain = env->NewStringUTF("minecraft");
        jstring jpath = env->NewStringUTF(path);
        jobject loc = env->NewObject((jclass)rlCls, ctor2, domain, jpath);
        env->DeleteLocalRef(domain);
        env->DeleteLocalRef(jpath);
        if (env->ExceptionCheck()) { env->ExceptionClear(); loc = nullptr; }
        if (loc) return loc;
    }
    jmethodID ctor1 = env->GetMethodID((jclass)rlCls, "<init>", "(Ljava/lang/String;)V");
    if (env->ExceptionCheck()) { env->ExceptionClear(); ctor1 = nullptr; }
    if (!ctor1) return nullptr;
    jstring jpath = env->NewStringUTF(path);
    jobject loc = env->NewObject((jclass)rlCls, ctor1, jpath);
    env->DeleteLocalRef(jpath);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return loc;
}

static bool BindMcTexture(JNIEnv* env, const char* path) {
    if (!env || !path) return false;
    JniOk(env);
    jobject tm = GetTextureManager(env);
    if (!tm) return false;
    jobject loc = MakeResourceLocation(env, path);
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

static jobject EnsureRenderItem(JNIEnv* env) {
    static jobject s_ri = nullptr;
    if (s_ri) return s_ri;
    jobject mc = Minecraft::GetTheMinecraft(env);
    if (!mc) return nullptr;
    Klass* mcCls = g_Instance->FindClass(Mapper::Get("net/minecraft/client/Minecraft"));
    if (mcCls) {
        std::string riSig = Mapper::Get("net/minecraft/client/renderer/entity/RenderItem", 3);
        Method* getRi = mcCls->GetMethod(env, Mapper::Get("getRenderItem").c_str(), riSig.c_str());
        if (env->ExceptionCheck()) { env->ExceptionClear(); getRi = nullptr; }
        if (getRi) {
            jobject ri = getRi->CallObjectMethod(env, mc);
            if (env->ExceptionCheck()) { env->ExceptionClear(); ri = nullptr; }
            if (ri) s_ri = env->NewGlobalRef(ri);
            if (ri) env->DeleteLocalRef(ri);
        }
        if (!s_ri) {
            Field* f = mcCls->GetField(env, Mapper::Get("mcRenderItem").c_str(),
                Mapper::Get("net/minecraft/client/renderer/entity/RenderItem", 2).c_str());
            if (env->ExceptionCheck()) { env->ExceptionClear(); f = nullptr; }
            if (f) {
                jobject ri = f->GetObjectField(env, mc);
                if (ri) s_ri = env->NewGlobalRef(ri);
                if (ri) env->DeleteLocalRef(ri);
            }
        }
    }
    env->DeleteLocalRef(mc);
    if (!s_ri) {
        Klass* riCls = g_Instance->FindClass(Mapper::Get("net/minecraft/client/renderer/entity/RenderItem"));
        if (riCls) {
            jmethodID ctor = env->GetMethodID((jclass)riCls, "<init>", "()V");
            if (env->ExceptionCheck()) { env->ExceptionClear(); ctor = nullptr; }
            if (ctor) {
                jobject ri = env->NewObject((jclass)riCls, ctor);
                if (env->ExceptionCheck()) { env->ExceptionClear(); ri = nullptr; }
                if (ri) {
                    s_ri = env->NewGlobalRef(ri);
                    env->DeleteLocalRef(ri);
                }
            }
        }
    }
    return s_ri;
}

static bool DrawStackWithRenderItem(JNIEnv* env, jobject stack, float x, float y, float sz) {
    if (!env || !stack) return false;
    jobject ri = EnsureRenderItem(env);
    if (!ri) return false;
    Klass* riCls = (Klass*)env->GetObjectClass(ri);
    if (!riCls) return false;
    std::string stackSig = Mapper::Get("net/minecraft/item/ItemStack", 2);
    std::string fontSig = Mapper::Get("net/minecraft/client/gui/FontRenderer", 2);
    std::string tmSig = Mapper::Get("net/minecraft/client/renderer/texture/TextureManager", 2);
    std::string name = Mapper::Get("renderItemAndEffectIntoGUI");
    Method* m18 = riCls->GetMethod(env, name.c_str(), ("(" + stackSig + "II)V").c_str());
    if (env->ExceptionCheck()) { env->ExceptionClear(); m18 = nullptr; }
    Method* m17 = nullptr;
    if (!m18)
        m17 = riCls->GetMethod(env, name.c_str(), ("(" + fontSig + tmSig + stackSig + "II)V").c_str());
    if (env->ExceptionCheck()) { env->ExceptionClear(); m17 = nullptr; }
    env->DeleteLocalRef((jclass)riCls);
    if (!m18 && !m17) return false;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(x, y, 0.f);
    glScalef(sz / 16.f, sz / 16.f, 1.f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (m18) {
        m18->CallVoidMethod(env, ri, false, stack, 0, 0);
    } else {
        jobject font = Minecraft::GetFontRenderer(env);
        jobject tm = GetTextureManager(env);
        if (font && tm)
            m17->CallVoidMethod(env, ri, false, font, tm, stack, 0, 0);
        if (font) env->DeleteLocalRef(font);
        if (tm) env->DeleteLocalRef(tm);
    }
    bool ok = !env->ExceptionCheck();
    if (!ok) env->ExceptionClear();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    return ok;
}

static void DrawPotionFallback(JNIEnv* env, float x, float y, float sz, bool splash, int tintRgb) {
    const char* base = PotionTexturePath(splash);
    if (env && base && BindMcTexture(env, base)) {
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.001f);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        DrawTexturedQuad(x, y, sz);
        if (BindMcTexture(env, "textures/items/potion_overlay.png")) {
            int rgb = tintRgb >= 0 ? tintRgb : 0x385DC6;
            glColor4f(((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, 1.f);
            DrawTexturedQuad(x, y, sz);
            glColor4f(1.f, 1.f, 1.f, 1.f);
        }
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_TEXTURE_2D);
    }
}

static bool DrawItemIcon(JNIEnv* env, float x, float y, float sz, const PespSlot& slot, ImU32 fallbackCol) {
    if (slot.stackRef && DrawStackWithRenderItem(env, slot.stackRef, x, y, sz))
        return true;

    if (slot.itemId == 373) {
        DrawPotionFallback(env, x, y, sz, slot.splash, slot.tintRgb);
        return true;
    }

    const char* path = ItemTexturePath(slot.itemId);
    if (env && path && BindMcTexture(env, path)) {
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.001f);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        DrawTexturedQuad(x, y, sz);
        if (slot.enchanted) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            float t = (float)(GetTickCount() % 2000) / 2000.f;
            float pulse = 0.3f + 0.15f * sinf(t * 6.2831853f);
            glColor4f(0.5f, 0.2f, 1.f, pulse);
            DrawTexturedQuad(x, y, sz);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1.f, 1.f, 1.f, 1.f);
        }
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_TEXTURE_2D);
        return true;
    }

    float c[4] = {
        ((fallbackCol >> IM_COL32_R_SHIFT) & 0xFF) / 255.f,
        ((fallbackCol >> IM_COL32_G_SHIFT) & 0xFF) / 255.f,
        ((fallbackCol >> IM_COL32_B_SHIFT) & 0xFF) / 255.f,
        1.f
    };
    glDisable(GL_TEXTURE_2D);
    glColor4f(c[0], c[1], c[2], 1.f);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + sz, y);
    glVertex2f(x + sz, y + sz); glVertex2f(x, y + sz);
    glEnd();
    return false;
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
    if (ticks < 0 || ticks >= 24000)
        return "inf";
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
        JniOk(env);
        if (lvl > 0) {
            bool exists = false;
            for (auto& e : out) if (e.id == ids[i]) { exists = true; break; }
            if (!exists) out.push_back({ ids[i], lvl });
        }
    }
}

static void FillEnchantsFromNbt(ItemStack* st, JNIEnv* env, std::vector<PespEnch>& out) {
    Klass* stackCls = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemStack"));
    if (!stackCls) return;
    std::string listSig = Mapper::Get("net/minecraft/nbt/NBTTagList", 3);
    Method* getList = stackCls->GetMethod(env, Mapper::Get("getEnchantmentTagList").c_str(), listSig.c_str());
    if (env->ExceptionCheck()) { env->ExceptionClear(); getList = nullptr; }
    if (!getList) return;
    jobject list = getList->CallObjectMethod(env, (jobject)st);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!list) return;
    Klass* listCls = (Klass*)env->GetObjectClass(list);
    if (!listCls) { env->DeleteLocalRef(list); return; }
    Method* tagCount = listCls->GetMethod(env, Mapper::Get("tagCount").c_str(), "()I");
    if (env->ExceptionCheck()) { env->ExceptionClear(); tagCount = nullptr; }
    std::string cmpSig = "(I)" + Mapper::Get("net/minecraft/nbt/NBTTagCompound", 2);
    Method* getAt = listCls->GetMethod(env, Mapper::Get("getCompoundTagAt").c_str(), cmpSig.c_str());
    if (env->ExceptionCheck()) { env->ExceptionClear(); getAt = nullptr; }
    env->DeleteLocalRef((jclass)listCls);
    if (!tagCount || !getAt) { env->DeleteLocalRef(list); return; }
    int n = tagCount->CallIntMethod(env, list);
    JniOk(env);
    Klass* cmpCls = g_Instance->FindClass(Mapper::Get("net/minecraft/nbt/NBTTagCompound"));
    Method* getShort = cmpCls ? cmpCls->GetMethod(env, "getShort", "(Ljava/lang/String;)S") : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); getShort = nullptr; }
    jstring idKey = env->NewStringUTF("id");
    jstring lvlKey = env->NewStringUTF("lvl");
    for (int i = 0; i < n && getShort; i++) {
        jobject tag = getAt->CallObjectMethod(env, list, false, i);
        JniOk(env);
        if (!tag) continue;
        int id = (short)env->CallShortMethod(tag, (jmethodID)getShort, idKey);
        JniOk(env);
        int lvl = (short)env->CallShortMethod(tag, (jmethodID)getShort, lvlKey);
        JniOk(env);
        env->DeleteLocalRef(tag);
        if (lvl > 0) {
            bool exists = false;
            for (auto& e : out) if (e.id == id) { exists = true; break; }
            if (!exists) out.push_back({ id, lvl });
        }
    }
    env->DeleteLocalRef(idKey);
    env->DeleteLocalRef(lvlKey);
    env->DeleteLocalRef(list);
}

static void ReleaseSlot(JNIEnv* env, PespSlot& s) {
    if (s.stackRef && env) {
        env->DeleteGlobalRef(s.stackRef);
        s.stackRef = nullptr;
    }
}

static void ReleasePlayers(JNIEnv* env) {
    for (auto& p : s_players) {
        for (int i = 0; i < 4; i++) ReleaseSlot(env, p.armor[i]);
        ReleaseSlot(env, p.held);
        for (auto& pot : p.invPots) ReleaseSlot(env, pot);
    }
}

static int TintFromPotionName(const std::string& raw) {
    std::string n = raw;
    for (char& c : n) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
    if (n.find("heal") != std::string::npos || n.find("health") != std::string::npos
        || n.find("soin") != std::string::npos || n.find("instant health") != std::string::npos)
        return 0xF82423;
    if (n.find("poison") != std::string::npos) return 0x4E9331;
    if (n.find("slow") != std::string::npos) return 0x5A6C81;
    if (n.find("swift") != std::string::npos || n.find("speed") != std::string::npos) return 0x7CAFC6;
    if (n.find("strength") != std::string::npos || n.find("force") != std::string::npos) return 0x932423;
    if (n.find("regen") != std::string::npos) return 0xCD5CAB;
    if (n.find("fire") != std::string::npos) return 0xE49A3A;
    if (n.find("night") != std::string::npos || n.find("vision") != std::string::npos) return 0x1F1FA1;
    if (n.find("invis") != std::string::npos) return 0x7F8392;
    if (n.find("weak") != std::string::npos) return 0x484D48;
    if (n.find("harm") != std::string::npos) return 0x430A09;
    if (n.find("jump") != std::string::npos || n.find("leap") != std::string::npos) return 0x22FF4C;
    if (n.find("breath") != std::string::npos) return 0x2E5299;
    if (n.find("wither") != std::string::npos) return 0x352A27;
    return 0;
}

static bool NameLooksSplash(const std::string& raw) {
    std::string n = raw;
    for (char& c : n) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
    return n.find("splash") != std::string::npos
        || n.find("jetable") != std::string::npos
        || n.find("throwable") != std::string::npos
        || n.find("splashable") != std::string::npos;
}

static void ResolvePotionVisual(JNIEnv* env, jobject stack, PespSlot& s) {
    s.splash = (s.meta & 16384) != 0;
    s.tintRgb = PotionLiquidColor(s.meta);

    Klass* potCls = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemPotion"));
    if (potCls) {
        Method* isSplash = potCls->GetMethod(env, Mapper::Get("isSplash").c_str(), "(I)Z", true);
        if (env->ExceptionCheck()) { env->ExceptionClear(); isSplash = nullptr; }
        if (isSplash) {
            bool v = isSplash->CallBoolMethod(env, potCls, true, s.meta);
            JniOk(env);
            if (v) s.splash = true;
        }
        jobject item = stack ? ((ItemStack*)stack)->GetItem(env) : nullptr;
        JniOk(env);
        if (item) {
            Method* gcd = potCls->GetMethod(env, Mapper::Get("getColorFromDamage").c_str(), "(I)I");
            JniOk(env);
            if (gcd) {
                int c = gcd->CallIntMethod(env, item, false, s.meta);
                JniOk(env);
                if (c) s.tintRgb = c;
            }
            std::string sig = "(" + Mapper::Get("net/minecraft/item/ItemStack", 2) + "I)I";
            Method* gcis = potCls->GetMethod(env, Mapper::Get("getColorFromItemStack").c_str(), sig.c_str());
            JniOk(env);
            if (gcis && stack) {
                int c = gcis->CallIntMethod(env, item, false, stack, 0);
                JniOk(env);
                if (c && c != 0xFFFFFF && c != 16777215) s.tintRgb = c;
            }
            env->DeleteLocalRef(item);
        }
    }

    std::string name;
    if (stack) {
        name = ((ItemStack*)stack)->GetDisplayName(env);
        JniOk(env);
    }
    if (NameLooksSplash(name)) s.splash = true;
    int named = TintFromPotionName(name);
    if (named && (s.meta == 0 || s.tintRgb == 0x385DC6 || s.tintRgb == PotionLiquidColor(0)))
        s.tintRgb = named;
    else if (named && s.tintRgb < 0)
        s.tintRgb = named;
    if (s.tintRgb < 0) s.tintRgb = PotionLiquidColor(s.meta);
}

static PespSlot ReadStack(jobject stackObj, JNIEnv* env, bool weapon) {
    PespSlot s;
    if (!stackObj) return s;
    auto* st = (ItemStack*)stackObj;
    s.itemId = st->GetItemId(env);
    JniOk(env);
    if (s.itemId < 0) s.itemId = 0;
    s.meta = st->GetMetadata(env);
    JniOk(env);
    s.enchanted = st->IsEnchanted(env);
    JniOk(env);
    s.count = st->GetStackSize(env);
    JniOk(env);
    if (weapon || IsToolOrWeapon(s.itemId)) {
        FillEnchants(st, env, kWeaponEnch, (int)(sizeof(kWeaponEnch) / sizeof(kWeaponEnch[0])), s.enchants);
        FillEnchants(st, env, kToolEnch, (int)(sizeof(kToolEnch) / sizeof(kToolEnch[0])), s.enchants);
    } else if (IsArmorItem(s.itemId)) {
        FillEnchants(st, env, kArmorEnch, (int)(sizeof(kArmorEnch) / sizeof(kArmorEnch[0])), s.enchants);
    } else {
        FillEnchants(st, env, kWeaponEnch, (int)(sizeof(kWeaponEnch) / sizeof(kWeaponEnch[0])), s.enchants);
        FillEnchants(st, env, kArmorEnch, (int)(sizeof(kArmorEnch) / sizeof(kArmorEnch[0])), s.enchants);
    }
    FillEnchantsFromNbt(st, env, s.enchants);
    if (!s.enchants.empty()) s.enchanted = true;
    std::string potCls = Mapper::Get("net/minecraft/item/ItemPotion");
    if (s.itemId == 373 || (!potCls.empty() && st->Is(potCls.c_str(), env))) {
        s.itemId = 373;
        ResolvePotionVisual(env, stackObj, s);
        JniOk(env);
    }
    s.stackRef = env->NewGlobalRef(stackObj);
    return s;
}

static int ReadIntOn(JNIEnv* env, jobject obj, const char* field, const char* getter) {
    if (!obj) return 0;
    jclass cls = env->GetObjectClass(obj);
    int v = 0;
    bool ok = false;
    while (cls && !ok) {
        if (field && field[0]) {
            jfieldID f = env->GetFieldID(cls, field, "I");
            if (env->ExceptionCheck()) { env->ExceptionClear(); f = nullptr; }
            if (f) {
                v = env->GetIntField(obj, f);
                JniOk(env);
                ok = true;
            }
        }
        if (!ok && getter && getter[0]) {
            jmethodID m = env->GetMethodID(cls, getter, "()I");
            if (env->ExceptionCheck()) { env->ExceptionClear(); m = nullptr; }
            if (m) {
                v = env->CallIntMethod(obj, m);
                JniOk(env);
                ok = true;
            }
        }
        jclass super = env->GetSuperclass(cls);
        env->DeleteLocalRef(cls);
        cls = super;
    }
    if (cls) env->DeleteLocalRef(cls);
    return v;
}

static void PushPotionFromEffect(JNIEnv* env, jobject pe, std::vector<PespPot>& out) {
    if (!pe) return;
    int id = ReadIntOn(env, pe, "potionID", Mapper::Get("getPotionID").c_str());
    if (!id) id = ReadIntOn(env, pe, "potionID", "getPotionID");
    int dur = ReadIntOn(env, pe, "duration", Mapper::Get("getDuration").c_str());
    if (!dur) dur = ReadIntOn(env, pe, "duration", "getDuration");
    int amp = ReadIntOn(env, pe, "amplifier", Mapper::Get("getAmplifier").c_str());
    if (!amp) amp = ReadIntOn(env, pe, "amplifier", "getAmplifier");
    if (id <= 0) return;
    for (auto& e : out) if (e.id == id) return;
    out.push_back({ id, dur, amp });
}

static void CollectFromIterator(JNIEnv* env, jobject iter, std::vector<PespPot>& out) {
    if (!iter) return;
    jclass ic = env->FindClass("java/util/Iterator");
    if (!ic) return;
    jmethodID hasN = env->GetMethodID(ic, "hasNext", "()Z");
    jmethodID next = env->GetMethodID(ic, "next", "()Ljava/lang/Object;");
    env->DeleteLocalRef(ic);
    if (!hasN || !next) return;
    while (env->CallBooleanMethod(iter, hasN)) {
        JniOk(env);
        jobject pe = env->CallObjectMethod(iter, next);
        JniOk(env);
        if (pe) {
            PushPotionFromEffect(env, pe, out);
            env->DeleteLocalRef(pe);
        }
    }
}

static void CollectFromCollection(JNIEnv* env, jobject coll, std::vector<PespPot>& out) {
    if (!coll) return;
    jclass cc = env->GetObjectClass(coll);
    if (!cc) return;
    jmethodID toArr = env->GetMethodID(cc, "toArray", "()[Ljava/lang/Object;");
    if (env->ExceptionCheck()) { env->ExceptionClear(); toArr = nullptr; }
    if (toArr) {
        jobject arrObj = env->CallObjectMethod(coll, toArr);
        JniOk(env);
        env->DeleteLocalRef(cc);
        if (!arrObj) return;
        auto arr = (jobjectArray)arrObj;
        const jsize n = env->GetArrayLength(arr);
        for (jsize i = 0; i < n; i++) {
            jobject pe = env->GetObjectArrayElement(arr, i);
            if (pe) {
                PushPotionFromEffect(env, pe, out);
                env->DeleteLocalRef(pe);
            }
        }
        env->DeleteLocalRef(arrObj);
        return;
    }
    jmethodID iterId = env->GetMethodID(cc, "iterator", "()Ljava/util/Iterator;");
    env->DeleteLocalRef(cc);
    JniOk(env);
    if (!iterId) return;
    jobject iter = env->CallObjectMethod(coll, iterId);
    JniOk(env);
    if (!iter) return;
    CollectFromIterator(env, iter, out);
    env->DeleteLocalRef(iter);
}

static void CollectPotions(JNIEnv* env, jobject entity, std::vector<PespPot>& out) {
    if (!env || !entity) return;
    JniOk(env);

    auto tryMethodOn = [&](Klass* cls) {
        if (!cls) return;
        Method* m = cls->GetMethod(env, Mapper::Get("getActivePotionEffects").c_str(), "()Ljava/util/Collection;");
        JniOk(env);
        if (!m) m = cls->GetMethod(env, Mapper::Get("getActivePotionEffects").c_str(), "()Ljava/util/List;");
        JniOk(env);
        if (!m) return;
        jobject coll = m->CallObjectMethod(env, entity);
        JniOk(env);
        if (coll) {
            CollectFromCollection(env, coll, out);
            env->DeleteLocalRef(coll);
        }
    };

    tryMethodOn(g_Instance->FindClass(Mapper::Get("net/minecraft/entity/EntityLivingBase")));
    if (out.empty()) {
        jclass oc = env->GetObjectClass(entity);
        tryMethodOn((Klass*)oc);
        if (oc) env->DeleteLocalRef(oc);
    }

    if (out.empty()) {
        jclass cls = env->GetObjectClass(entity);
        jfieldID mapF = nullptr;
        while (cls && !mapF) {
            std::string name = Mapper::Get("activePotionsMap");
            mapF = env->GetFieldID(cls, name.empty() ? "activePotionsMap" : name.c_str(), "Ljava/util/Map;");
            if (env->ExceptionCheck()) { env->ExceptionClear(); mapF = nullptr; }
            if (!mapF) {
                mapF = env->GetFieldID(cls, "activePotionsMap", "Ljava/util/HashMap;");
                if (env->ExceptionCheck()) { env->ExceptionClear(); mapF = nullptr; }
            }
            jclass super = env->GetSuperclass(cls);
            env->DeleteLocalRef(cls);
            cls = super;
        }
        if (cls) env->DeleteLocalRef(cls);
        if (mapF) {
            jobject mapObj = env->GetObjectField(entity, mapF);
            JniOk(env);
            if (mapObj) {
                jclass mc = env->FindClass("java/util/Map");
                jmethodID values = mc ? env->GetMethodID(mc, "values", "()Ljava/util/Collection;") : nullptr;
                if (mc) env->DeleteLocalRef(mc);
                jobject vals = values ? env->CallObjectMethod(mapObj, values) : nullptr;
                JniOk(env);
                if (vals) {
                    CollectFromCollection(env, vals, out);
                    env->DeleteLocalRef(vals);
                }
                env->DeleteLocalRef(mapObj);
            }
        }
    }

    if (out.empty()) {
        auto* p = (Player*)entity;
        for (int id = 1; id <= 23; id++) {
            if (!p->IsPotionActive(id, env)) continue;
            JniOk(env);
            out.push_back({ id, 0, 0 });
        }
    }
}

static bool W2S(const Vec3& w, Vec2& s) {
    return WorldToScreen(w, s, s_mv, s_proj,
        (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
}

void PlayerEsp::OnRender(JNIEnv* env) {
    (void)env;
}

static void CollectPlayerEsp(JNIEnv* env) {
    ReleasePlayers(env);
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

        int gapples = 0;
        int currentSlot = -1;
        std::unordered_map<int, int> potsByMeta;
        jobject invObj = ent->GetInventoryPlayer(env);
        JniOk(env);
        InventoryPlayer* inv = invObj ? (InventoryPlayer*)invObj : nullptr;
        if (inv) {
            currentSlot = inv->GetSlot(env);
            JniOk(env);
            for (int slot = 0; slot < 36; slot++) {
                jobject st = inv->GetStackInSlot(slot, env);
                JniOk(env);
                if (!st) continue;
                auto* stack = (ItemStack*)st;
                int id = stack->GetItemId(env);
                JniOk(env);
                int n = stack->GetStackSize(env);
                JniOk(env);
                int meta = stack->GetMetadata(env);
                JniOk(env);
                if (slot != currentSlot) {
                    if (id == 322 && n > 0) gapples += n;
                    else if (id == 373 && n > 0) potsByMeta[meta] += n;
                }
                env->DeleteLocalRef(st);
            }
        }
        if (PlayerEspSettings::armor) {
            for (int i = 0; i < 4; i++) {
                jobject st = GetArmorStack(env, ent, inv, i);
                JniOk(env);
                if (st) {
                    pd.armor[i] = ReadStack(st, env, false);
                    JniOk(env);
                    env->DeleteLocalRef(st);
                }
            }
        }
        if (invObj) env->DeleteLocalRef(invObj);
        pd.gappleCount = gapples;
        if (PlayerEspSettings::potions) {
            for (auto& kv : potsByMeta) {
                PespSlot pot;
                pot.itemId = 373;
                pot.meta = kv.first;
                pot.count = kv.second;
                ResolvePotionVisual(env, nullptr, pot);
                pd.invPots.push_back(std::move(pot));
                pd.potionInvCount += kv.second;
            }
        }

        if (PlayerEspSettings::heldItem) {
            jobject held = GetHeldStack(env, ent);
            JniOk(env);
            if (held) {
                pd.held = ReadStack(held, env, true);
                JniOk(env);
                env->DeleteLocalRef(held);
            }
        }

        CollectPotions(env, e, pd.potions);
        JniOk(env);

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
    for (size_t i = 0; i < e.size(); i++) {
        if (i) s += ' ';
        s += EnchantAbbrev(e[i].id);
        s += std::to_string(e[i].lvl);
    }
    return s;
}

static bool ShowCount(const PespSlot& slot) {
    if (slot.count <= 0) return false;
    if (slot.itemId == 373 || slot.itemId == 322 || slot.itemId == 368) return true;
    return slot.count > 1;
}

static void DrawSlotIcon(JNIEnv* env, ImDrawList* dl, float x, float y, float sz, const PespSlot& slot, ImU32 col) {
    const ImVec2 a(x, y), b(x + sz, y + sz);
    if (slot.itemId == 373) {
        ImTextureID bottle = GetMcTexId(env, PotionTexturePath(slot.splash));
        if (bottle)
            dl->AddImage(bottle, a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);
        else
            dl->AddRectFilled(a, b, col, 2.f);
        ImTextureID overlay = GetMcTexId(env, "textures/items/potion_overlay.png");
        int rgb = slot.tintRgb >= 0 ? slot.tintRgb : PotionLiquidColor(slot.meta);
        if (overlay) {
            dl->AddImage(overlay, a, b, ImVec2(0, 0), ImVec2(1, 1),
                IM_COL32((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255));
        } else {
            dl->AddRectFilled(a, b, IM_COL32((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 140), 2.f);
        }
        return;
    }

    const char* path = ItemTexturePath(slot.itemId);
    ImTextureID tex = path ? GetMcTexId(env, path) : (ImTextureID)0;
    if (tex) {
        dl->AddImage(tex, a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);
        if (slot.enchanted) {
            float t = (float)(GetTickCount() % 2000) / 2000.f;
            int aPulse = (int)((0.25f + 0.2f * sinf(t * 6.2831853f)) * 255.f);
            dl->AddRectFilled(a, b, IM_COL32(140, 60, 255, aPulse), 2.f);
        }
        return;
    }

    dl->AddRectFilled(a, b, col, 2.f);
    dl->AddRect(a, b, IM_COL32(0, 0, 0, 180), 2.f);
    const char* ab = ItemAbbrev(slot.itemId);
    if (ab) {
        float fs = (std::max)(8.f, sz * 0.35f);
        ImVec2 ts = ImGui::CalcTextSize(ab);
        float sc = fs / ImGui::GetFontSize();
        dl->AddText(ImGui::GetFont(), fs,
            ImVec2(x + (sz - ts.x * sc) * 0.5f, y + (sz - fs) * 0.5f),
            IM_COL32(255, 255, 255, 255), ab);
    }
}

static void RenderArmorHud(JNIEnv* env, ImDrawList* dl, float centerX, float nametagY, float scale, const PespPlayer& p) {
    const float s = scale * PlayerEspSettings::displayScale;
    const float enchFS = (std::max)(8.f * s, 6.f);
    const float iconSz = (std::max)(18.f * s, 14.f);
    const float slotGap = 3.f * s, padX = 5.f * s, padY = 2.f * s;
    struct Row {
        const PespSlot* slot;
        std::string ench;
        ImU32 col;
        int count;
    };
    std::vector<Row> rows;
    if (PlayerEspSettings::armor) {
        for (int i = 3; i >= 0; i--) {
            if (p.armor[i].itemId < 0 && !p.armor[i].stackRef) continue;
            rows.push_back({ &p.armor[i], EnchString(p.armor[i].enchants), MaterialColor(p.armor[i].itemId), p.armor[i].count });
        }
    }
    if (PlayerEspSettings::heldItem && (p.held.itemId >= 0 || p.held.stackRef))
        rows.push_back({ &p.held, EnchString(p.held.enchants), MaterialColor(p.held.itemId), p.held.count });
    PespSlot gappleSlot{};
    if (PlayerEspSettings::gapple && p.gappleCount > 0) {
        gappleSlot.itemId = 322;
        gappleSlot.count = p.gappleCount;
        rows.push_back({ &gappleSlot, "x" + std::to_string(p.gappleCount), IM_COL32(255, 180, 50, 255), p.gappleCount });
    }
    if (PlayerEspSettings::potions) {
        for (const auto& pot : p.invPots) {
            if (pot.itemId != 373 || pot.count <= 0) continue;
            rows.push_back({ &pot, "x" + std::to_string(pot.count), IM_COL32(140, 80, 200, 255), pot.count });
        }
    }
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
    const float barH = (std::max)(2.f, s);
    const float rowH = padY + enchH + iconSz + barH + padY;
    const float x0 = centerX - totalW * 0.5f;
    const float y1 = nametagY - 4.f;
    const float y0 = y1 - rowH;
    const float iconY = y0 + padY + enchH;

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + totalW, y1), IM_COL32(0, 0, 0, 140), 4.f);

    float cur = x0 + padX;
    float sc = enchFS / ImGui::GetFontSize();
    for (size_t i = 0; i < rows.size(); i++) {
        float ix = cur + (slotW[i] - iconSz) * 0.5f;
        DrawSlotIcon(env, dl, ix, iconY, iconSz, *rows[i].slot, rows[i].col);
        dl->AddRectFilled(ImVec2(ix, iconY + iconSz), ImVec2(ix + iconSz, iconY + iconSz + barH),
            (rows[i].col & 0x00FFFFFF) | 0x8C000000);
        if (!rows[i].ench.empty()) {
            ImVec2 es = ImGui::CalcTextSize(rows[i].ench.c_str());
            dl->AddText(ImGui::GetFont(), enchFS,
                ImVec2(cur + (slotW[i] - es.x * sc) * 0.5f, iconY - enchFS),
                IM_COL32(85, 255, 255, 255), rows[i].ench.c_str());
        }
        if (rows[i].slot && ShowCount(*rows[i].slot) && rows[i].ench.find('x') != 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", rows[i].count);
            ImVec2 cs = ImGui::CalcTextSize(buf);
            float cfs = (std::max)(8.f * s, 7.f);
            float csc = cfs / ImGui::GetFontSize();
            float tx = ix + iconSz - cs.x * csc - 1.f;
            float ty = iconY + iconSz - cfs + 1.f;
            dl->AddText(ImGui::GetFont(), cfs, ImVec2(tx + 1.f, ty + 1.f), IM_COL32(0, 0, 0, 220), buf);
            dl->AddText(ImGui::GetFont(), cfs, ImVec2(tx, ty), IM_COL32(255, 255, 255, 255), buf);
        }
        cur += slotW[i] + slotGap;
    }
}

static int PotionIconIdx(int id) {
    switch (id) {
        case 1:  return 0;
        case 2:  return 1;
        case 3:  return 2;
        case 4:  return 3;
        case 5:  return 4;
        case 8:  return 10;
        case 9:  return 11;
        case 10: return 7;
        case 11: return 14;
        case 12: return 15;
        case 13: return 16;
        case 14: return 8;
        case 15: return 13;
        case 16: return 12;
        case 17: return 9;
        case 18: return 5;
        case 19: return 6;
        case 20: return 17;
        case 21: return 23;
        case 22: return 18;
        default: return -1;
    }
}

static const char* AmpSuffix(int amp) {
    switch (amp) {
        case 1: return " II";
        case 2: return " III";
        case 3: return " IV";
        case 4: return " V";
        default: return "";
    }
}

static void RenderPotions(JNIEnv* env, ImDrawList* dl, float centerX, float topY, float scale, const PespPlayer& p) {
    if (p.potions.empty()) return;
    const float s = scale * PlayerEspSettings::displayScale;
    const float fs = (std::max)(8.f * s, 6.f);
    const float iconSz = (std::max)(fs * 1.35f, 12.f);
    const float padX = 4.f * s, padY = 2.f * s, iconGap = 3.f * s, pillGap = 3.f * s;
    ImTextureID iconTex = env ? GetMcTexId(env, "textures/gui/container/inventory.png") : (ImTextureID)0;

    struct Pill { std::string text; int iconIdx; float w; };
    std::vector<Pill> pills;
    float totalW = padX;
    const float sc = fs / ImGui::GetFontSize();
    for (size_t i = 0; i < p.potions.size(); i++) {
        const PespPot& pot = p.potions[i];
        char buf[64];
        const char* n = PotionEffectName(pot.id);
        const char* amp = AmpSuffix(pot.amp);
        if (pot.duration > 0) {
            if (n) snprintf(buf, sizeof(buf), "%s%s %s", n, amp, FormatDuration(pot.duration).c_str());
            else snprintf(buf, sizeof(buf), "#%d%s %s", pot.id, amp, FormatDuration(pot.duration).c_str());
        } else {
            if (n) snprintf(buf, sizeof(buf), "%s%s", n, amp);
            else snprintf(buf, sizeof(buf), "#%d%s", pot.id, amp);
        }
        float tw = ImGui::CalcTextSize(buf).x * sc;
        int idx = PotionIconIdx(pot.id);
        float w = ((idx >= 0 && iconTex) ? iconSz + iconGap : 0.f) + tw + padX;
        if (i) totalW += pillGap;
        totalW += w;
        pills.push_back({ buf, idx, w });
    }
    totalW += padX;
    const float pillH = (std::max)(iconSz, fs) + padY * 2.f;
    float x0 = centerX - totalW * 0.5f;
    float y0 = topY;
    const float dispW = ImGui::GetIO().DisplaySize.x;
    const float dispH = ImGui::GetIO().DisplaySize.y;
    if (y0 < 2.f) y0 = 2.f;
    if (y0 + pillH > dispH - 2.f) y0 = dispH - 2.f - pillH;
    if (x0 < 2.f) x0 = 2.f;
    if (x0 + totalW > dispW - 2.f) x0 = dispW - 2.f - totalW;

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + totalW, y0 + pillH), IM_COL32(0, 0, 0, 178), 3.f);
    float cur = x0 + padX;
    for (const auto& pill : pills) {
        const float iconY = y0 + (pillH - iconSz) * 0.5f;
        float textX = cur;
        if (iconTex && pill.iconIdx >= 0) {
            const int col = pill.iconIdx % 8;
            const int row = pill.iconIdx / 8;
            const float u0 = (col * 18.f) / 256.f;
            const float v0 = (198.f + row * 18.f) / 256.f;
            const float u1 = u0 + 18.f / 256.f;
            const float v1 = v0 + 18.f / 256.f;
            dl->AddImage(iconTex, ImVec2(cur, iconY), ImVec2(cur + iconSz, iconY + iconSz),
                ImVec2(u0, v0), ImVec2(u1, v1));
            textX = cur + iconSz + iconGap;
        }
        dl->AddText(ImGui::GetFont(), fs, ImVec2(textX, y0 + (pillH - fs) * 0.5f),
            IM_COL32(230, 230, 230, 255), pill.text.c_str());
        cur += pill.w + pillGap;
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

        if (PlayerEspSettings::armor || PlayerEspSettings::heldItem
            || (PlayerEspSettings::gapple && p.gappleCount > 0)
            || (PlayerEspSettings::potions && p.potionInvCount > 0)) {
            RenderArmorHud(env, dl, centerX, nametag.y, scale, p);
        }
        RenderPotions(env, dl, centerX, nametag.y + 6.f, scale, p);
    }
}
