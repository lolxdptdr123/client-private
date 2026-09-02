#include "pch.h"
#include "Throw.h"

#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../Misc/Overlay.h"
#include "../Visuals/Notifications.h"

enum class ThrowKind { Pot, Soup, Debuff, Pearl };

static std::thread       g_throwThread;
static std::atomic<bool> g_running{ false };
static std::atomic<bool> g_busy{ false };

static HWND FindLunarWindow() {
    HWND h = FindWindowW(nullptr, L"Lunar Client 1.8.9");
    if (!h) h = FindWindowW(nullptr, L"Lunar Client 1.7.10");
    if (!h) h = FindWindowW(L"LWJGL", nullptr);
    return h;
}

static bool IsLunarFocused() {
    HWND h = FindLunarWindow();
    return h && GetForegroundWindow() == h;
}

static int DelayFromDisplay(float display) {
    int d = (int)(10.f - display + 0.5f);
    if (d < 0) d = 0;
    if (d > 10) d = 10;
    return d;
}

static bool ShouldHealSmart(float health, float potionHeal) {
    return (20.f - health) >= potionHeal;
}

static void RightClick(int holdMs) {
    HWND h = FindLunarWindow();
    if (!h) h = GetForegroundWindow();
    if (!h) return;
    PostMessageA(h, WM_RBUTTONDOWN, 0, 0);
    Sleep(holdMs);
    PostMessageA(h, WM_RBUTTONUP, 0, 0);
}

static void UnpressUseItem(JNIEnv* env) {
    jobject gs = Minecraft::GetGameSettings(env);
    if (!gs) return;
    jobject kb = ((GameSettings*)gs)->GetKeyBindUseItem(env);
    env->DeleteLocalRef(gs);
    if (!kb) return;
    ((KeyBinding*)kb)->SetPressed(false, env);
    env->DeleteLocalRef(kb);
}

static int FindHotbar(InventoryPlayer* inv, JNIEnv* env, ThrowKind kind, int exclude) {
    for (int i = 0; i < 9; i++) {
        if (i == exclude) continue;
        jobject stackObj = inv->GetStackInSlot(i, env);
        if (!stackObj) continue;
        auto* stack = (ItemStack*)stackObj;
        bool ok = false;
        if (kind == ThrowKind::Pot) {
            int dmg = stack->GetMetadata(env);
            ok = (dmg == 16421 || dmg == 16453);
        } else if (kind == ThrowKind::Soup) {
            ok = stack->IsSoup(env);
        } else if (kind == ThrowKind::Pearl) {
            ok = stack->IsEnderPearl(env);
        } else {
            int dmg = stack->GetMetadata(env);
            if ((dmg & 16384) != 0) {
                int effect = dmg & 15;
                ok = (effect == 4 || effect == 8 || effect == 10 || effect == 12);
            }
        }
        env->DeleteLocalRef(stackObj);
        if (ok) return i;
    }
    return -1;
}

static void PerformUse(JNIEnv* env, InventoryPlayer* inv, Player* player, int slot, int original, int holdMs, bool dropBowl) {
    UnpressUseItem(env);
    inv->SetSlot(slot, env);
    RightClick(holdMs);
    Sleep(50);
    if (dropBowl)
        player->DropOneItem(false, env);
    inv->SetSlot(original, env);
}

static void ExecutePot(JNIEnv* env) {
    jobject world = Minecraft::GetTheWorld(env);
    if (!world) return;
    env->DeleteLocalRef(world);
    jobject screen = Minecraft::GetCurrentScreen(env);
    if (screen) { env->DeleteLocalRef(screen); return; }

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) return;
    auto* player = (Player*)playerObj;
    float health = player->GetHealth(env);
    if (ThrowSettings::potSmart && !ShouldHealSmart(health, 8.f)) {
        env->DeleteLocalRef(playerObj);
        return;
    }

    jobject invObj = player->GetInventoryPlayer(env);
    if (!invObj) { env->DeleteLocalRef(playerObj); return; }
    auto* inv = (InventoryPlayer*)invObj;
    int original = inv->GetSlot(env);
    int count = (ThrowSettings::potDouble && health <= 4.f) ? 2 : 1;
    int delay = DelayFromDisplay(ThrowSettings::potSpeed);
    bool used = false;
    for (int i = 0; i < count; i++) {
        int slot = FindHotbar(inv, env, ThrowKind::Pot, -1);
        if (slot < 0) break;
        if (delay > 0) Sleep(delay * 50);
        PerformUse(env, inv, player, slot, original, 1, false);
        used = true;
        if (i + 1 < count) Sleep(1);
    }
    env->DeleteLocalRef(invObj);
    env->DeleteLocalRef(playerObj);
    if (used) NotificationSettings::PushInfo("Throw", "Health used", "Combat");
}

static void ExecuteSoup(JNIEnv* env) {
    jobject world = Minecraft::GetTheWorld(env);
    if (!world) return;
    env->DeleteLocalRef(world);
    jobject screen = Minecraft::GetCurrentScreen(env);
    if (screen) { env->DeleteLocalRef(screen); return; }

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) return;
    auto* player = (Player*)playerObj;
    float health = player->GetHealth(env);
    if (ThrowSettings::soupSmart && !ShouldHealSmart(health, 6.5f)) {
        env->DeleteLocalRef(playerObj);
        return;
    }

    jobject invObj = player->GetInventoryPlayer(env);
    if (!invObj) { env->DeleteLocalRef(playerObj); return; }
    auto* inv = (InventoryPlayer*)invObj;
    int original = inv->GetSlot(env);
    int count = (ThrowSettings::soupDouble && health <= 6.f) ? 2 : 1;
    int delay = DelayFromDisplay(ThrowSettings::soupSpeed);
    int exclude = -1;
    bool used = false;
    for (int i = 0; i < count; i++) {
        int slot = FindHotbar(inv, env, ThrowKind::Soup, exclude);
        if (slot < 0) break;
        if (delay > 0) Sleep(delay * 50);
        PerformUse(env, inv, player, slot, original, 1, ThrowSettings::soupAutoDrop);
        exclude = slot;
        used = true;
        if (i + 1 < count) Sleep(1);
    }
    env->DeleteLocalRef(invObj);
    env->DeleteLocalRef(playerObj);
    if (used) NotificationSettings::PushInfo("Throw", "Soup used", "Combat");
}

static void ExecutePearl(JNIEnv* env) {
    jobject world = Minecraft::GetTheWorld(env);
    if (!world) return;
    env->DeleteLocalRef(world);
    jobject screen = Minecraft::GetCurrentScreen(env);
    if (screen) { env->DeleteLocalRef(screen); return; }

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) return;
    auto* player = (Player*)playerObj;
    jobject invObj = player->GetInventoryPlayer(env);
    if (!invObj) { env->DeleteLocalRef(playerObj); return; }
    auto* inv = (InventoryPlayer*)invObj;
    int original = inv->GetSlot(env);
    int slot = FindHotbar(inv, env, ThrowKind::Pearl, -1);
    if (slot < 0) {
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return;
    }
    int delay = DelayFromDisplay(ThrowSettings::pearlSpeed);
    if (delay > 0) Sleep(delay * 50);
    PerformUse(env, inv, player, slot, original, 1, false);
    env->DeleteLocalRef(invObj);
    env->DeleteLocalRef(playerObj);
    NotificationSettings::PushInfo("Throw", "Pearl used", "Combat");
}

static void ExecuteDebuff(JNIEnv* env) {
    jobject world = Minecraft::GetTheWorld(env);
    if (!world) return;
    env->DeleteLocalRef(world);
    jobject screen = Minecraft::GetCurrentScreen(env);
    if (screen) { env->DeleteLocalRef(screen); return; }

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) return;
    auto* player = (Player*)playerObj;
    jobject invObj = player->GetInventoryPlayer(env);
    if (!invObj) { env->DeleteLocalRef(playerObj); return; }
    auto* inv = (InventoryPlayer*)invObj;
    int original = inv->GetSlot(env);
    int delay = DelayFromDisplay(ThrowSettings::debuffSpeed);
    bool used = false;

    if (ThrowSettings::debuffDouble) {
        UnpressUseItem(env);
        for (int i = 0; i < 9; i++) {
            jobject stackObj = inv->GetStackInSlot(i, env);
            if (!stackObj) continue;
            int dmg = ((ItemStack*)stackObj)->GetMetadata(env);
            env->DeleteLocalRef(stackObj);
            if ((dmg & 16384) == 0) continue;
            int effect = dmg & 15;
            if (!(effect == 4 || effect == 8 || effect == 10 || effect == 12)) continue;
            if (delay > 0) Sleep(delay * 50);
            inv->SetSlot(i, env);
            RightClick(10);
            Sleep(50);
            used = true;
        }
        inv->SetSlot(original, env);
    } else {
        int slot = FindHotbar(inv, env, ThrowKind::Debuff, -1);
        if (slot >= 0) {
            if (delay > 0) Sleep(delay * 50);
            PerformUse(env, inv, player, slot, original, 10, false);
            used = true;
        }
    }

    env->DeleteLocalRef(invObj);
    env->DeleteLocalRef(playerObj);
    if (used) NotificationSettings::PushInfo("Throw", "Debuff used", "Combat");
}

static bool Edge(int bind, bool& held) {
    if (!bind) { held = false; return false; }
    bool now = (GetAsyncKeyState(bind) & 0x8000) != 0;
    bool fire = now && !held;
    held = now;
    return fire;
}

static void ThrowThreadProc() {
    JavaVM* jvm = g_Instance->GetJVM();
    JNIEnv* env = nullptr;
    bool attached = false;
    if (jvm) {
        if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
            if (jvm->AttachCurrentThreadAsDaemon((void**)&env, nullptr) == JNI_OK)
                attached = true;
            else
                env = nullptr;
        }
    }

    bool potHeld = false, soupHeld = false, debuffHeld = false, pearlHeld = false;

    while (g_running) {
        if (!env || Overlay::isOpen || !IsLunarFocused() || !ThrowSettings::enabled) {
            potHeld = soupHeld = debuffHeld = pearlHeld = false;
            Sleep(15);
            continue;
        }

        bool firePot = ThrowSettings::potEnabled && Edge(ThrowSettings::potBind, potHeld);
        bool fireSoup = ThrowSettings::soupEnabled && Edge(ThrowSettings::soupBind, soupHeld);
        bool fireDebuff = ThrowSettings::debuffEnabled && Edge(ThrowSettings::debuffBind, debuffHeld);
        bool firePearl = ThrowSettings::pearlEnabled && Edge(ThrowSettings::pearlBind, pearlHeld);

        if (firePot || fireSoup || fireDebuff || firePearl) {
            g_busy = true;
            if (firePot) ExecutePot(env);
            if (firePearl) ExecutePearl(env);
            if (fireSoup) ExecuteSoup(env);
            if (fireDebuff) ExecuteDebuff(env);
            g_busy = false;
        }
        Sleep(10);
    }
    if (attached && jvm) jvm->DetachCurrentThread();
}

bool Throw_IsBusy() { return g_busy.load(); }

void Throw_Start() {
    if (g_running) return;
    g_running = true;
    g_throwThread = std::thread(ThrowThreadProc);
}

void Throw_Stop() {
    g_running = false;
    g_busy = false;
    if (g_throwThread.joinable()) g_throwThread.join();
}

void ThrowModule::Run(JNIEnv* env) {
    Sleep(50);
}
