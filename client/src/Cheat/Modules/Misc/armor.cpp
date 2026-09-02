// ============================================================
//  armor.cpp — Switch Armor
//  Méthode : input pur (aucun JNI → aucun crash possible)
//
//  Logique de swap (armure déjà équipée) :
//    Étape 1 : clic gauche sur pièce INVENTAIRE  → la pièce est dans le curseur
//    Étape 2 : clic gauche sur slot ÉQUIPÉ       → la nouvelle s'équipe,
//                                                   l'ancienne passe dans le curseur
//    Étape 3 : clic gauche sur slot INVENTAIRE   → pose l'ancienne dans l'inventaire
//
//  Logique simple (slot armure vide) :
//    Shift+clic sur pièce INVENTAIRE → Minecraft l'équipe directement
//
//  Slots GUI armure équipée (inventory 1.7.10, repère fenêtre 176×166) :
//    Helmet     (piece 0) : centre guiX=16, guiY=16
//    Chestplate (piece 1) : centre guiX=16, guiY=34
//    Leggings   (piece 2) : centre guiX=16, guiY=52
//    Boots      (piece 3) : centre guiX=16, guiY=70
// ============================================================

#include "pch.h"
#include "armor.h"

static std::thread       g_armorThread;
static std::atomic<bool> g_running{ false };
static std::atomic<bool> g_doSwitch{ false };

// ── LWJGL → VK ───────────────────────────────────────────────────────────────
// Couvre TOUS les scancodes LWJGL : ponctuation, pave numerique, etc.

static int LWJGLToVK(int lwjgl) {
    if (lwjgl < 0) {
        switch (lwjgl) {
        case -100: return VK_LBUTTON;
        case -99:  return VK_RBUTTON;
        case -98:  return VK_MBUTTON;
        default:   return -1;
        }
    }
    switch (lwjgl) {
        // Chiffres
    case 2:  return '1'; case 3:  return '2'; case 4:  return '3';
    case 5:  return '4'; case 6:  return '5'; case 7:  return '6';
    case 8:  return '7'; case 9:  return '8'; case 10: return '9';
    case 11: return '0';
        // Lettres
    case 16: return 'Q'; case 17: return 'W'; case 18: return 'E';
    case 19: return 'R'; case 20: return 'T'; case 21: return 'Y';
    case 22: return 'U'; case 23: return 'I'; case 24: return 'O';
    case 25: return 'P'; case 30: return 'A'; case 31: return 'S';
    case 32: return 'D'; case 33: return 'F'; case 34: return 'G';
    case 35: return 'H'; case 36: return 'J'; case 37: return 'K';
    case 38: return 'L'; case 44: return 'Z'; case 45: return 'X';
    case 46: return 'C'; case 47: return 'V'; case 48: return 'B';
    case 49: return 'N'; case 50: return 'M';
        // Ponctuation / symboles
    case 12: return VK_OEM_MINUS;    // - _
    case 13: return VK_OEM_PLUS;     // = +
    case 26: return VK_OEM_4;        // [ {
    case 27: return VK_OEM_6;        // ] }
    case 39: return VK_OEM_1;        // ; :
    case 40: return VK_OEM_7;        // ' "
    case 41: return VK_OEM_3;        // ` ~ / §
    case 43: return VK_OEM_5;        // \ |
    case 51: return VK_OEM_COMMA;    // , <
    case 52: return VK_OEM_PERIOD;   // . >
    case 53: return VK_OEM_2;        // / ?
    case 86: return VK_OEM_102;      // < > | (ISO)
        // Controle
    case 1:  return VK_ESCAPE;
    case 14: return VK_BACK;
    case 15: return VK_TAB;
    case 28: return VK_RETURN;
    case 29: return VK_LCONTROL;
    case 42: return VK_LSHIFT;
    case 54: return VK_RSHIFT;
    case 56: return VK_LMENU;
    case 57: return VK_SPACE;
    case 58: return VK_CAPITAL;
    case 91: return VK_LWIN;
    case 92: return VK_RWIN;
    case 93: return VK_APPS;
    case 157: return VK_RCONTROL;
    case 184: return VK_RMENU;
        // Fonctions
    case 59: return VK_F1;  case 60: return VK_F2;
    case 61: return VK_F3;  case 62: return VK_F4;
    case 63: return VK_F5;  case 64: return VK_F6;
    case 65: return VK_F7;  case 66: return VK_F8;
    case 67: return VK_F9;  case 68: return VK_F10;
    case 87: return VK_F11; case 88: return VK_F12;
    case 100: return VK_F13; case 101: return VK_F14; case 102: return VK_F15;
        // Navigation
    case 199: return VK_HOME;   case 200: return VK_UP;
    case 201: return VK_PRIOR;  case 203: return VK_LEFT;
    case 205: return VK_RIGHT;  case 207: return VK_END;
    case 208: return VK_DOWN;   case 209: return VK_NEXT;
    case 210: return VK_INSERT; case 211: return VK_DELETE;
        // Pave numerique
    case 71: return VK_NUMPAD7; case 72: return VK_NUMPAD8; case 73: return VK_NUMPAD9;
    case 75: return VK_NUMPAD4; case 76: return VK_NUMPAD5; case 77: return VK_NUMPAD6;
    case 79: return VK_NUMPAD1; case 80: return VK_NUMPAD2; case 81: return VK_NUMPAD3;
    case 82: return VK_NUMPAD0; case 83: return VK_DECIMAL;
    case 55: return VK_MULTIPLY; case 74: return VK_SUBTRACT;
    case 78: return VK_ADD;      case 156: return VK_RETURN;
    case 181: return VK_DIVIDE;
    case 69: return VK_NUMLOCK;  case 70: return VK_SCROLL;
    case 197: return VK_PAUSE;   case 183: return VK_SNAPSHOT;
    default:  return -1;
    }
}


// ── Keybinds ──────────────────────────────────────────────────────────────────

static int  g_inventoryVK = 'E';
static void LoadKeybinds() {
    g_inventoryVK = 'E';

    char appdata[MAX_PATH] = {};
    GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    std::string path = std::string(appdata) + "\\.minecraft\\options.txt";
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();

        const std::string invKey = "key_key.inventory:";
        if (s.rfind(invKey, 0) == 0) {
            int vk = LWJGLToVK(atoi(s.substr(invKey.size()).c_str()));
            if (vk > 0) g_inventoryVK = vk;
        }
    }
    fclose(f);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static int SpeedToDelayMs(int speed) {
    if (speed < 1)  speed = 1;
    if (speed > 20) speed = 20;
    // speed 1  → 200ms  (très lent)
    // speed 10 → 20ms   (normal)
    // speed 20 → 0ms    (max)
    if (speed <= 10) return 200 - (speed - 1) * 20; // 200 → 20
    return 20 - (speed - 10) * 2;                   // 20  → 0
}

static HWND FindLunarWindow() {
    HWND h = FindWindowW(nullptr, L"Lunar Client 1.8.9");
    if (!h) h = FindWindowW(nullptr, L"Lunar Client 1.7.10");
    if (!h) h = FindWindowW(L"LWJGL", nullptr);
    return h;
}

static bool IsLunarFocused() {
    HWND lunar = FindLunarWindow();
    if (!lunar) return false;
    return GetForegroundWindow() == lunar;
}

static void SendKey(int vk) {
    if (vk <= 0) return;
    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = (WORD)vk;
    in[0].ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

static void MoveCursorTo(POINT screen) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    LONG ax = (LONG)((screen.x * 65535L) / (sw - 1));
    LONG ay = (LONG)((screen.y * 65535L) / (sh - 1));
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dx = ax;
    in.mi.dy = ay;
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &in, sizeof(INPUT));
}

static void SendLeftClick() {
    INPUT in[2] = {};
    in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, in, sizeof(INPUT));
}

static void SendShiftClick() {
    INPUT in[4] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_LSHIFT;
    in[0].ki.wScan = (WORD)MapVirtualKeyW(VK_LSHIFT, MAPVK_VK_TO_VSC);
    in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    in[2].type = INPUT_MOUSE; in[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    in[3].type = INPUT_KEYBOARD;
    in[3].ki.wVk = VK_LSHIFT;
    in[3].ki.wScan = (WORD)MapVirtualKeyW(VK_LSHIFT, MAPVK_VK_TO_VSC);
    in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
}

// ── GUI scale ─────────────────────────────────────────────────────────────────

// Lit guiScale depuis options.txt (même chemin que les keybinds).
// Retourne 0 si la clé est absente (= Auto), -1 si le fichier est illisible.
static int ReadGuiScaleFromOptions() {
    char appdata[MAX_PATH] = {};
    GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    std::string path = std::string(appdata) + "\\.minecraft\\options.txt";

    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "r");
    if (!f) return -1;

    int result = 0; // défaut : Auto
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        const std::string key = "guiScale:";
        if (s.rfind(key, 0) == 0) {
            result = atoi(s.substr(key.size()).c_str());
            break;
        }
    }
    fclose(f);
    return result;
}

// Calcule la GUI scale effective à partir de la taille du client et du
// paramètre options.txt (0 = Auto = pas de limite de scale).
static int CalcGuiScale(int cw, int ch) {
    int scaleSetting = ReadGuiScaleFromOptions();
    if (scaleSetting < 0) scaleSetting = 2; // fichier illisible → Normal par défaut
    int scale = 1;
    int maxScale = (scaleSetting == 0) ? 100 : scaleSetting;
    while (scale < maxScale
        && (scale + 1) * 320 <= cw
        && (scale + 1) * 240 <= ch)
    {
        scale++;
    }
    return scale;
}

// ── Positions GUI inventory 1.7.10 ────────────────────────────────────────────

// Centre GUI du slot ÉQUIPÉ (colonne gauche, topLeft x=8, y=8, espacement 18)
// piece 0=Helmet→y=16, 1=Chest→y=34, 2=Legs→y=52, 3=Boots→y=70
static void EquippedSlotGuiPos(int piece, int& outX, int& outY) {
    outX = 8 + 8;
    outY = 8 + piece * 18 + 8;
}

// Centre GUI d'un container slot inventaire (9-35) ou hotbar (0-8)
static bool ContainerSlotToGuiPos(int containerSlot, int& outX, int& outY) {
    if (containerSlot >= 0 && containerSlot <= 8) {
        outX = 8 + containerSlot * 18 + 8;
        outY = 142 + 8;
        return true;
    }
    if (containerSlot >= 9 && containerSlot <= 35) {
        int idx = containerSlot - 9;
        outX = 8 + (idx % 9) * 18 + 8;
        outY = 84 + (idx / 9) * 18 + 8;
        return true;
    }
    return false;
}

static bool GuiPosToScreen(HWND lunar, int guiX, int guiY, POINT& out)
{
    RECT client;
    if (!GetClientRect(lunar, &client)) return false;
    int cw = client.right - client.left;
    int ch = client.bottom - client.top;
    if (cw <= 0 || ch <= 0) return false;

    int scale = CalcGuiScale(cw, ch);
    int invOriginX = (cw / scale - 176) / 2;
    int invOriginY = (ch / scale - 166) / 2;

    POINT pt;
    pt.x = (invOriginX + guiX) * scale;
    pt.y = (invOriginY + guiY) * scale;
    ClientToScreen(lunar, &pt);
    out = pt;
    return true;
}

// ── SwitchArmorProc ───────────────────────────────────────────────────────────

static void SwitchArmorProc(const int slotsToClick[4], const int equippedIds[4]) {
    LoadKeybinds(); // recharge options.txt a chaque switch
    if (!IsLunarFocused()) { Armor::lastStatus = "Lunar non focus"; return; }

    bool hasAny = false;
    for (int i = 0; i < 4; i++)
        if (slotsToClick[i] >= 0) { hasAny = true; break; }
    if (!hasAny) {
        Armor::lastStatus = "Aucune armure trouvee dans l'inventaire";
        return;
    }

    HWND lunar = FindLunarWindow();
    if (!lunar) { Armor::lastStatus = "Fenetre Lunar introuvable"; return; }

    int delayMs = SpeedToDelayMs(Armor::speed);
    int hoverMs = 30;  // stabilisation curseur avant clic

    POINT cursorBefore;
    GetCursorPos(&cursorBefore);

    // 1. Ouvrir inventaire
    SendKey(g_inventoryVK);
    Sleep(120);

    // 2. Traiter chaque pièce
    for (int i = 0; i < 4; i++) {
        if (!g_running) break;
        if (slotsToClick[i] < 0) continue;

        // Position de la pièce dans l'inventaire
        int invGuiX, invGuiY;
        if (!ContainerSlotToGuiPos(slotsToClick[i], invGuiX, invGuiY)) continue;
        POINT invScreen;
        if (!GuiPosToScreen(lunar, invGuiX, invGuiY, invScreen)) continue;

        // Position du slot équipé correspondant
        int eqGuiX, eqGuiY;
        EquippedSlotGuiPos(i, eqGuiX, eqGuiY);
        POINT eqScreen;
        if (!GuiPosToScreen(lunar, eqGuiX, eqGuiY, eqScreen)) continue;

        if (equippedIds[i] < 0) {
            // ── Slot vide : shift+clic direct ────────────────────────────────
            MoveCursorTo(invScreen);
            Sleep(hoverMs);
            SendShiftClick();

            Armor::lastStatus = std::string("Equipe: ") + Armor::pieceNames[i];
        }
        else {
            // ── Swap : 3 clics ────────────────────────────────────────────────
            //
            // Clic 1 : sur la pièce INVENTAIRE → dans le curseur
            MoveCursorTo(invScreen);
            Sleep(hoverMs);
            SendLeftClick();

            // Clic 2 : sur le slot ÉQUIPÉ → swap (nouvelle équipée, ancienne dans curseur)
            MoveCursorTo(eqScreen);
            Sleep(hoverMs);
            SendLeftClick();

            // Clic 3 : retour INVENTAIRE → pose l'ancienne dans le slot libéré
            MoveCursorTo(invScreen);
            Sleep(hoverMs);
            SendLeftClick();

            Armor::lastStatus = std::string("Echange: ") + Armor::pieceNames[i];
        }

        Sleep(delayMs);
    }

    // 3. Fermer inventaire
    Sleep(50);
    SendKey(g_inventoryVK);

    // 4. Restaurer curseur
    Sleep(30);
    MoveCursorTo(cursorBefore);

    Armor::lastStatus = "Switch termine !";
}

// ── Données partagées ─────────────────────────────────────────────────────────

struct PendingSwap {
    int slotsToClick[4] = { -1, -1, -1, -1 };
    int equippedIds[4] = { -1, -1, -1, -1 };
};

static std::mutex  g_slotsMutex;
static PendingSwap g_pending;
static PendingSwap g_lastKnown;

// ── Thread ────────────────────────────────────────────────────────────────────

static void ArmorThreadProc() {
    while (g_running) {
        if (!g_doSwitch.exchange(false)) { Sleep(10); continue; }

        PendingSwap snap;
        {
            std::lock_guard<std::mutex> lock(g_slotsMutex);
            snap = g_pending;
        }
        SwitchArmorProc(snap.slotsToClick, snap.equippedIds);
    }
}

void Armor::Start() {
    if (g_running) return;
    g_running = true;
    LoadKeybinds();
    g_armorThread = std::thread(ArmorThreadProc);
}

void Armor::Stop() {
    g_running = false;
    g_doSwitch = false;
    if (g_armorThread.joinable()) g_armorThread.join();
}

// ── Armor_Trigger ─────────────────────────────────────────────────────────────
void Armor_Trigger(const int slotsToClick[4], const int equippedIds[4]) {
    {
        std::lock_guard<std::mutex> lock(g_slotsMutex);
        for (int i = 0; i < 4; i++) {
            g_pending.slotsToClick[i] = slotsToClick[i];
            g_pending.equippedIds[i] = equippedIds[i];
            if (slotsToClick[i] >= 0) {
                g_lastKnown.slotsToClick[i] = slotsToClick[i];
                g_lastKnown.equippedIds[i] = equippedIds[i];
            }
        }
    }
    g_doSwitch = true;
}

// ── Armor_Trigger_Manual ──────────────────────────────────────────────────────
void Armor_Trigger_Manual() {
    {
        std::lock_guard<std::mutex> lock(g_slotsMutex);
        g_pending = g_lastKnown;
    }
    g_doSwitch = true;
}