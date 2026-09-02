// ============================================================
//  Scroll.cpp
//  Scrolle automatiquement vers les items de la whitelist,
//  puis retourne sur le slot de l'épée après chaque séquence.
// ============================================================

#include "pch.h"
#include "Scroll.h"
#include "Overlay.h"

#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Game/Method.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"

#include <thread>
#include <atomic>
#include <mutex>

// ── IDs des épées vanilla ─────────────────────────────────────────────────────
static const int SWORD_IDS[] = { 268, 272, 267, 276, 283 };
static const int SWORD_ID_COUNT = 5;

// ── État global ───────────────────────────────────────────────────────────────
static bool              s_hotbarLoaded = false;
static std::thread       g_scrollThread;
static std::atomic<bool> g_running{ false };

// Slots à scroller — remplis sur le thread de rendu, consommés par le thread input
static std::mutex        g_slotsMutex;
static int               g_pendingSlots[9] = {};
static int               g_pendingCount = 0;
static std::atomic<bool> g_doScroll{ false };

// Slot épée partagé entre Trigger (rendu) et le thread input
static std::atomic<int>  g_swordSlot{ -1 };

// Vrai pendant toute la durée de la séquence (scroll + retour épée)
// Empêche Trigger() d'écraser g_swordSlot pendant l'exécution
static std::atomic<bool> g_scrollInProgress{ false };

// ── Fonctions SEH isolées ─────────────────────────────────────────────────────
static int CallGetIdFromItem(Method* method, Klass* itemClass, jobject itemObj, JNIEnv* env) {
    __try {
        return method->CallIntMethod(env, itemClass, true, itemObj);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

static jobject SafeGetStackInSlot(InventoryPlayer* inv, int slot, JNIEnv* env) {
    __try {
        return inv->GetStackInSlot(slot, env);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

static jobject SafeGetItem(ItemStack* stack, JNIEnv* env) {
    __try {
        return stack->GetItem(env);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// ── GetItemId ─────────────────────────────────────────────────────────────────
static int GetItemId(jobject stackObj, JNIEnv* env) {
    if (!stackObj) return -1;
    if (env->IsSameObject(stackObj, nullptr)) return -1;

    auto* stack = (ItemStack*)stackObj;
    jobject itemObj = SafeGetItem(stack, env);
    if (!itemObj) return -1;
    if (env->IsSameObject(itemObj, nullptr)) return -1;

    const auto itemClass = g_Instance->FindClass(
        Mapper::Get("net/minecraft/item/Item"));
    if (!itemClass) return -1;

    std::string mappedItem = Mapper::Get("net/minecraft/item/Item");
    std::string sig = "(L" + mappedItem + ";)I";
    std::string methodName = Mapper::Get("getIdFromItem");

    const auto method = itemClass->GetMethod(env,
        methodName.c_str(), sig.c_str(), true);
    if (!method) return -1;

    return CallGetIdFromItem(method, itemClass, itemObj, env);
}

// ── LWJGL → VK ───────────────────────────────────────────────────────────────
__declspec(noinline) static int LWJGLToVK_SEH(int lwjgl) {
    __try {
        if (lwjgl < 0) {
            switch (lwjgl) {
            case -100: return VK_LBUTTON;
            case -99:  return VK_RBUTTON;
            case -98:  return VK_MBUTTON;
            case -97:  return VK_XBUTTON1;
            case -96:  return VK_XBUTTON2;
            default:   return -1;
            }
        }
        switch (lwjgl) {
        case 0:  return -1;
        case 2:  return '1'; case 3:  return '2'; case 4:  return '3';
        case 5:  return '4'; case 6:  return '5'; case 7:  return '6';
        case 8:  return '7'; case 9:  return '8'; case 10: return '9';
        case 11: return '0';
        case 16: return 'Q'; case 17: return 'W'; case 18: return 'E';
        case 19: return 'R'; case 20: return 'T'; case 21: return 'Y';
        case 22: return 'U'; case 23: return 'I'; case 24: return 'O';
        case 25: return 'P'; case 30: return 'A'; case 31: return 'S';
        case 32: return 'D'; case 33: return 'F'; case 34: return 'G';
        case 35: return 'H'; case 36: return 'J'; case 37: return 'K';
        case 38: return 'L'; case 44: return 'Z'; case 45: return 'X';
        case 46: return 'C'; case 47: return 'V'; case 48: return 'B';
        case 49: return 'N'; case 50: return 'M';
        case 1:  return VK_ESCAPE;   case 14: return VK_BACK;
        case 15: return VK_TAB;      case 28: return VK_RETURN;
        case 29: return VK_LCONTROL; case 42: return VK_LSHIFT;
        case 54: return VK_RSHIFT;   case 56: return VK_LMENU;
        case 57: return VK_SPACE;    case 58: return VK_CAPITAL;
        case 59: return VK_F1;  case 60: return VK_F2;
        case 61: return VK_F3;  case 62: return VK_F4;
        case 63: return VK_F5;  case 64: return VK_F6;
        case 65: return VK_F7;  case 66: return VK_F8;
        case 67: return VK_F9;  case 68: return VK_F10;
        case 87: return VK_F11; case 88: return VK_F12;
        case 71: return VK_NUMPAD7; case 72: return VK_NUMPAD8;
        case 73: return VK_NUMPAD9; case 75: return VK_NUMPAD4;
        case 76: return VK_NUMPAD5; case 77: return VK_NUMPAD6;
        case 79: return VK_NUMPAD1; case 80: return VK_NUMPAD2;
        case 81: return VK_NUMPAD3; case 82: return VK_NUMPAD0;
        case 83: return VK_DECIMAL;
        case 199: return VK_HOME;  case 200: return VK_UP;
        case 201: return VK_PRIOR; case 203: return VK_LEFT;
        case 205: return VK_RIGHT; case 207: return VK_END;
        case 208: return VK_DOWN;  case 209: return VK_NEXT;
        case 210: return VK_INSERT;case 211: return VK_DELETE;
        default:  return -1;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return -1;
}

static int MCKeyToVK(const std::string& val) {
    if (val.empty()) return -1;
    bool isNeg = (val[0] == '-');
    bool allDig = true;
    for (size_t i = isNeg ? 1 : 0; i < val.size(); i++)
        if (!isdigit((unsigned char)val[i])) { allDig = false; break; }
    if (!allDig) return -1;
    return LWJGLToVK_SEH(atoi(val.c_str()));
}

static void LoadHotbarKeys() {
    for (int i = 0; i < 9; i++) Scroll::hotbarKeys[i] = -1;
    char appdata[MAX_PATH] = {};
    GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    std::string path = std::string(appdata) + "\\.minecraft\\options.txt";
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "r");
    if (!f) { s_hotbarLoaded = true; return; }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        for (int i = 0; i < 9; i++) {
            std::string key = "key_key.hotbar." + std::to_string(i + 1) + ":";
            if (s.rfind(key, 0) == 0) {
                Scroll::hotbarKeys[i] = MCKeyToVK(s.substr(key.size()));
                break;
            }
        }
    }
    fclose(f);
    s_hotbarLoaded = true;
}

static void PressHotbarKey(int slot) {
    if (!s_hotbarLoaded) LoadHotbarKeys();
    if (slot < 0 || slot > 8) return;
    int vk = Scroll::hotbarKeys[slot];
    if (vk < 0) vk = (slot == 8) ? '9' : ('1' + slot);

    if (vk == VK_XBUTTON1 || vk == VK_XBUTTON2) {
        DWORD btn = (vk == VK_XBUTTON1) ? XBUTTON1 : XBUTTON2;
        INPUT in[2] = {};
        in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_XDOWN; in[0].mi.mouseData = btn;
        in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_XUP;   in[1].mi.mouseData = btn;
        SendInput(2, in, sizeof(INPUT));
        return;
    }
    if (vk == VK_LBUTTON) {
        INPUT in[2] = {};
        in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, in, sizeof(INPUT));
        return;
    }
    if (vk == VK_RBUTTON) {
        INPUT in[2] = {};
        in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        SendInput(2, in, sizeof(INPUT));
        return;
    }
    WORD sc = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = (WORD)vk; in[0].ki.wScan = sc;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = (WORD)vk; in[1].ki.wScan = sc;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

void Scroll::ScrollToSlot(int slot) {
    if (!IsGameWindowFocused()) return;
    if (slot < 0 || slot > 8) return;
    PressHotbarKey(slot);
}

// ── Thread input — ne fait que des SendInput, pas de JNI ─────────────────────
static void ScrollThreadProc() {
    while (g_running) {
        if (!g_doScroll.exchange(false)) { Sleep(5); continue; }

        // Copie locale des slots et du slot épée
        // g_scrollInProgress est déjà true (posé par Trigger avant g_doScroll)
        int slots[9];
        int count = 0;
        int swordSlot = g_swordSlot.load();
        {
            std::lock_guard<std::mutex> lock(g_slotsMutex);
            count = g_pendingCount;
            for (int i = 0; i < count; i++)
                slots[i] = g_pendingSlots[i];
            g_pendingCount = 0;
        }

        // Envoie les inputs avec délai
        for (int i = 0; i < count; i++) {
            if (!g_running) break;
            Scroll::ScrollToSlot(slots[i]);
            Sleep(Scroll::scrollDelay);
        }

        // Retour automatique sur l'épée après la séquence
        if (g_running && swordSlot >= 0) {
            Sleep(Scroll::scrollDelay);
            Scroll::ScrollToSlot(swordSlot);
        }

        // Libère le verrou — Trigger() peut de nouveau mettre à jour g_swordSlot
        g_scrollInProgress = false;
    }
}

// ── Trigger — appelé sur le thread de rendu (JNI safe) ───────────────────────
void Scroll::Trigger(JNIEnv* env) {
    if (!enabled) return;
    if (Overlay::isOpen) return;
    if (!IsGameWindowFocused()) return;
    if (!s_hotbarLoaded) LoadHotbarKeys();

    // Vérifie qu'au moins un item est activé
    bool anyEnabled = false;
    for (int w = 0; w < ITEM_COUNT; w++)
        if (g_whitelistEnabled[w]) { anyEnabled = true; break; }
    if (!anyEnabled) return;

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj || env->IsSameObject(playerObj, nullptr)) return;

    auto* player = (Player*)playerObj;
    jobject invObj = player->GetInventoryPlayer(env);
    if (!invObj || env->IsSameObject(invObj, nullptr)) return;

    auto* inv = (InventoryPlayer*)invObj;

    // Scanne la hotbar : collecte les slots whitelist + détecte l'épée
    int slots[9];
    int count = 0;
    int swordSlot = -1;

    for (int slot = 0; slot < 9; slot++) {
        jobject stackObj = SafeGetStackInSlot(inv, slot, env);
        if (!stackObj || env->IsSameObject(stackObj, nullptr)) continue;

        int id = GetItemId(stackObj, env);
        if (id < 0) continue;

        // Détection épée
        for (int s = 0; s < SWORD_ID_COUNT; s++) {
            if (id == SWORD_IDS[s]) { swordSlot = slot; break; }
        }

        // Détection whitelist
        for (int w = 0; w < ITEM_COUNT; w++) {
            if (!g_whitelistEnabled[w]) continue;
            if (id == g_itemIds[w]) {
                slots[count++] = slot;
                break;
            }
        }
    }

    if (count == 0) return;

    // Ne pas écraser g_swordSlot si une séquence est déjà en cours
    if (!g_scrollInProgress.load()) {
        g_swordSlot.store(swordSlot);
        Scroll::swordSlot = swordSlot;
    }

    // Passe les slots whitelist au thread input
    {
        std::lock_guard<std::mutex> lock(g_slotsMutex);
        g_pendingCount = count;
        for (int i = 0; i < count; i++)
            g_pendingSlots[i] = slots[i];
    }
    // Pose le verrou AVANT de signaler le thread, pour éviter la race condition
    g_scrollInProgress = true;
    g_doScroll = true;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void Scroll::Start() {
    LoadHotbarKeys();
    if (g_running) return;
    g_running = true;
    g_scrollThread = std::thread(ScrollThreadProc);
}

void Scroll::Stop() {
    g_running = false;
    g_doScroll = false;
    g_scrollInProgress = false;
    if (g_scrollThread.joinable())
        g_scrollThread.join();
}

void Scroll::ReloadKeybinds() {
    s_hotbarLoaded = false;
    LoadHotbarKeys();
}