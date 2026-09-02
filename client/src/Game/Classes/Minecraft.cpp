#include "pch.h"
#include "Minecraft.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"
#include "../../Cheat/Modules/Settings.h"

// ── Cache statique des Field* resolus une seule fois ─────────────────────────
// Evite de refaire FindClass + GetField a chaque appel (1000x/sec par module)

struct MinecraftFieldCache {
    Klass* clsMC = nullptr;
    Field* fTheMinecraft = nullptr;
    Field* fThePlayer = nullptr;
    Field* fTheWorld = nullptr;
    Field* fCurrentScreen = nullptr;
    Field* fObjectMO = nullptr;
    Field* fPointedEntity = nullptr;
    Field* fGameSettings = nullptr;
    Field* fTimer = nullptr;
    Field* fFontRenderer = nullptr;
    Field* fDisplayWidth = nullptr;
    Field* fDisplayHeight = nullptr;
    Field* fRightClickDelay = nullptr;
    Field* fRenderManager = nullptr;
    bool    renderMgrTried = false;
    bool    built = false;
} static s_mc;

static void BuildMCCache(JNIEnv* env) {
    if (s_mc.fTheMinecraft && s_mc.fThePlayer && s_mc.fTheWorld) return;
    s_mc.built = true;

    s_mc.clsMC = g_Instance->FindClass(Mapper::Get("net/minecraft/client/Minecraft"));
    if (!s_mc.clsMC) return;

    auto GF = [&](const char* key, const char* sigKey, int sigType = 2) -> Field* {
        std::string name = Mapper::Get(key);
        std::string sig = Mapper::Get(sigKey, sigType);
        if (name.empty() || sig.empty()) return nullptr;
        Field* f = s_mc.clsMC->GetField(env, name.c_str(), sig.c_str(), false);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
        return f;
        };
    auto GFS = [&](const char* key, const char* sigKey, int sigType = 2) -> Field* {
        std::string name = Mapper::Get(key);
        std::string sig = Mapper::Get(sigKey, sigType);
        if (name.empty() || sig.empty()) return nullptr;
        Field* f = s_mc.clsMC->GetField(env, name.c_str(), sig.c_str(), true);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
        return f;
        };
    auto GFI = [&](const char* key) -> Field* {
        std::string name = Mapper::Get(key);
        if (name.empty()) return nullptr;
        Field* f = s_mc.clsMC->GetField(env, name.c_str(), "I", false);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
        return f;
        };

    s_mc.fTheMinecraft = GFS("theMinecraft", "net/minecraft/client/Minecraft");
    s_mc.fThePlayer = GF("thePlayer", "net/minecraft/client/entity/EntityClientPlayerMP");
    if (!s_mc.fThePlayer)
        s_mc.fThePlayer = GF("thePlayer", "net/minecraft/client/entity/EntityPlayerSP");
    if (!s_mc.fThePlayer)
        s_mc.fThePlayer = GF("thePlayer", "net/minecraft/entity/player/EntityPlayer");
    s_mc.fTheWorld = GF("theWorld", "net/minecraft/client/multiplayer/WorldClient");
    if (!s_mc.fTheWorld) {
        std::string name = Mapper::Get("theWorld");
        Field* f = s_mc.clsMC->GetField(env, name.c_str(), "Lnet/minecraft/world/World;", false);
        if (env->ExceptionCheck()) { env->ExceptionClear(); f = nullptr; }
        s_mc.fTheWorld = f;
    }
    s_mc.fCurrentScreen = GF("currentScreen", "net/minecraft/client/gui/GuiScreen");
    s_mc.fObjectMO = GF("objectMouseOver", "net/minecraft/util/MovingObjectPosition");
    s_mc.fPointedEntity = GF("pointedEntity", "net/minecraft/entity/Entity");
    s_mc.fGameSettings = GF("gameSettings", "net/minecraft/client/settings/GameSettings");
    s_mc.fTimer = GF("timer", "net/minecraft/util/Timer");
    s_mc.fFontRenderer = GF("fontRendererObj", "net/minecraft/client/gui/FontRenderer");
    s_mc.fDisplayWidth = GFI("displayWidth");
    s_mc.fDisplayHeight = GFI("displayHeight");
    s_mc.fRightClickDelay = GFI("rightClickDelayTimer");
    if (!s_mc.renderMgrTried) {
        s_mc.renderMgrTried = true;
        s_mc.fRenderManager = GF("renderManager", "net/minecraft/client/renderer/entity/RenderManager");
    }
}

// Helper interne — retourne l'instance Minecraft (champ statique)
static jobject GetMCInstance(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fTheMinecraft || !s_mc.clsMC) return nullptr;
    return s_mc.fTheMinecraft->GetObjectField(env, s_mc.clsMC, true);
}

jobject Minecraft::GetTheMinecraft(JNIEnv* env) { return GetMCInstance(env); }

jobject Minecraft::GetThePlayer(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fThePlayer) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fThePlayer->GetObjectField(env, mc, false);
}

jobject Minecraft::GetTheWorld(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fTheWorld) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fTheWorld->GetObjectField(env, mc, false);
}

jobject Minecraft::GetCurrentScreen(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fCurrentScreen) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fCurrentScreen->GetObjectField(env, mc, false);
}

jobject Minecraft::GetObjectMouseOver(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fObjectMO) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fObjectMO->GetObjectField(env, mc, false);
}

jobject Minecraft::GetPointedEntity(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fPointedEntity) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fPointedEntity->GetObjectField(env, mc, false);
}

void Minecraft::SetObjectMouseOver(JNIEnv* env, jobject mop) {
    BuildMCCache(env);
    if (!s_mc.fObjectMO) return;
    jobject mc = GetMCInstance(env);
    if (!mc) return;
    s_mc.fObjectMO->SetObjectField(env, mc, mop, false);
}

void Minecraft::SetPointedEntity(JNIEnv* env, jobject entity) {
    BuildMCCache(env);
    if (!s_mc.fPointedEntity) return;
    jobject mc = GetMCInstance(env);
    if (!mc) return;
    s_mc.fPointedEntity->SetObjectField(env, mc, entity, false);
}

jobject Minecraft::GetGameSettings(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fGameSettings) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fGameSettings->GetObjectField(env, mc, false);
}

jobject Minecraft::GetTimer(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fTimer) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fTimer->GetObjectField(env, mc, false);
}

jobject Minecraft::GetFontRenderer(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fFontRenderer) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fFontRenderer->GetObjectField(env, mc, false);
}

int Minecraft::GetDisplayWidth(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fDisplayWidth) return 0;
    jobject mc = GetMCInstance(env);
    if (!mc) return 0;
    return s_mc.fDisplayWidth->GetIntField(env, mc);
}

int Minecraft::GetDisplayHeight(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fDisplayHeight) return 0;
    jobject mc = GetMCInstance(env);
    if (!mc) return 0;
    return s_mc.fDisplayHeight->GetIntField(env, mc);
}

bool Minecraft::IsFullscreen(JNIEnv* env) {
    jobject mc = GetMCInstance(env);
    if (!mc) return false;
    Klass* cls = (Klass*)env->GetObjectClass(mc);
    if (!cls) return false;
    Field* f = cls->GetField(env, Mapper::Get("fullscreen").c_str(), "Z");
    env->DeleteLocalRef((jclass)cls);
    if (!f) return false;
    return f->GetBooleanField(env, mc);
}

jobject Minecraft::GetPlayerController(JNIEnv* env) {
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    Klass* cls = (Klass*)env->GetObjectClass(mc);
    if (!cls) return nullptr;
    Field* f = cls->GetField(env, Mapper::Get("playerController").c_str(),
        Mapper::Get("net/minecraft/client/multiplayer/PlayerControllerMP", 2).c_str());
    env->DeleteLocalRef((jclass)cls);
    if (!f) return nullptr;
    return f->GetObjectField(env, mc);
}

jobject Minecraft::WindowClick(JNIEnv* env, int windowId, int slot, int mouseButton, int clickMode, jobject player) {
    jobject controller = GetPlayerController(env);
    if (!controller || !player) return nullptr;
    Klass* cls = (Klass*)env->GetObjectClass(controller);
    if (!cls) return nullptr;
    std::string sig = "(IIII" + Mapper::Get("net/minecraft/entity/player/EntityPlayer", 2) + ")"
        + Mapper::Get("net/minecraft/item/ItemStack", 2);
    Method* m = cls->GetMethod(env, Mapper::Get("windowClick").c_str(), sig.c_str());
    env->DeleteLocalRef((jclass)cls);
    if (!m) return nullptr;
    jobject r = m->CallObjectMethod(env, controller, false, windowId, slot, mouseButton, clickMode, player);
    env->DeleteLocalRef(controller);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return r;
}

void Minecraft::RightClickMouse(JNIEnv* env) {
    jobject mc = GetMCInstance(env);
    if (!mc) return;
    Klass* cls = (Klass*)env->GetObjectClass(mc);
    if (!cls) return;
    Method* m = cls->GetMethod(env, Mapper::Get("rightClickMouse").c_str(), "()V");
    env->DeleteLocalRef((jclass)cls);
    if (!m) return;
    m->CallVoidMethod(env, mc);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

int Minecraft::GetRightClickDelayTimer(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fRightClickDelay) return -1;
    jobject mc = GetMCInstance(env);
    if (!mc) return -1;
    return s_mc.fRightClickDelay->GetIntField(env, mc);
}

void Minecraft::SetRightClickDelayTimer(JNIEnv* env, int ticks) {
    BuildMCCache(env);
    if (!s_mc.fRightClickDelay) return;
    jobject mc = GetMCInstance(env);
    if (!mc) return;
    s_mc.fRightClickDelay->SetIntField(env, mc, ticks);
}

jobject Minecraft::GetRenderManager(JNIEnv* env)
{
    if (!env) return nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();

    BuildMCCache(env);

    if (s_mc.fRenderManager) {
        jobject mc = GetMCInstance(env);
        if (mc) {
            jobject rm = s_mc.fRenderManager->GetObjectField(env, mc, false);
            if (env->ExceptionCheck()) { env->ExceptionClear(); rm = nullptr; }
            if (rm) return rm;
        }
    }

    const auto renderManagerClazz = g_Instance->FindClass(Mapper::Get("net/minecraft/client/renderer/entity/RenderManager"));
    if (!renderManagerClazz) return nullptr;

    std::string fname = Mapper::Get("renderManagerInstance");
    std::string fsig = Mapper::Get("net/minecraft/client/renderer/entity/RenderManager", 2);
    const auto field = renderManagerClazz->GetField(env, fname.c_str(), fsig.c_str(), true);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!field) return nullptr;
    jobject obj = field->GetObjectField(env, renderManagerClazz, true);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return obj;
}