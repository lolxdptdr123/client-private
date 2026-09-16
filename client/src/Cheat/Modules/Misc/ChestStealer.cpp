#include "pch.h"
#include "ChestStealer.h"

#include "Overlay.h"
#include "Weapons.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/GuiScreen.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Cheat/Hack.h"
#include "../../../Cheat/Modules/Settings.h"
#include "../../../Helper/Utils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>
#include <string>
#include <vector>

static std::mt19937 g_rng{ std::random_device{}() };
static int s_lastWindow = -1;
static ULONGLONG s_nextAction = 0;
static bool s_closing = false;
static bool s_shiftHeld = false;
static std::vector<int> s_skip;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static bool IsLunar() {
    return g_GameLauncher == LAUNCHER_LUNAR
        && (g_GameVersion == LUNAR_1_7_10 || g_GameVersion == LUNAR_1_8_9);
}

static void ReleaseShift() {
    if (!s_shiftHeld) return;
    keybd_event(VK_SHIFT, (BYTE)MapVirtualKey(VK_SHIFT, 0), KEYEVENTF_KEYUP, 0);
    s_shiftHeld = false;
}

static void HoldShift() {
    if (s_shiftHeld) return;
    keybd_event(VK_SHIFT, (BYTE)MapVirtualKey(VK_SHIFT, 0), 0, 0);
    s_shiftHeld = true;
}

static void ResetSession() {
    ReleaseShift();
    s_lastWindow = -1;
    s_nextAction = 0;
    s_closing = false;
    s_skip.clear();
}

static int RandDelay(int mn, int mx) {
    if (mn < 0) mn = 0;
    if (mx < mn) mx = mn;
    if (mn == mx) return mn;
    std::uniform_int_distribution<int> d(mn, mx);
    return d(g_rng);
}

static std::string JString(JNIEnv* env, jstring js) {
    if (!env || !js) return {};
    const char* c = env->GetStringUTFChars(js, nullptr);
    std::string out = c ? c : "";
    if (c) env->ReleaseStringUTFChars(js, c);
    return out;
}

static std::string ChatToString(JNIEnv* env, jobject chat) {
    if (!env || !chat) return {};
    jclass c = env->GetObjectClass(chat);
    if (!c) return {};
    const char* names[] = { "getUnformattedText", "getUnformattedTextForChat", "getFormattedText" };
    std::string out;
    for (const char* n : names) {
        jmethodID m = env->GetMethodID(c, n, "()Ljava/lang/String;");
        JniOk(env);
        if (!m) continue;
        jstring js = (jstring)env->CallObjectMethod(chat, m);
        JniOk(env);
        out = JString(env, js);
        if (js) env->DeleteLocalRef(js);
        if (!out.empty()) break;
    }
    env->DeleteLocalRef(c);
    return out;
}

static std::string InventoryTitle(JNIEnv* env, jobject inv) {
    if (!env || !inv) return {};
    jclass c = env->GetObjectClass(inv);
    if (!c) return {};
    std::string out;

    const char* strNames[] = { "getInventoryName", "getName" };
    for (const char* n : strNames) {
        jmethodID m = env->GetMethodID(c, n, "()Ljava/lang/String;");
        JniOk(env);
        if (!m) continue;
        jstring js = (jstring)env->CallObjectMethod(inv, m);
        JniOk(env);
        out = JString(env, js);
        if (js) env->DeleteLocalRef(js);
        if (!out.empty()) break;
    }

    if (out.empty()) {
        std::string chatSig = Mapper::Get("net/minecraft/util/IChatComponent", 3);
        if (chatSig.empty()) chatSig = "()Lnet/minecraft/util/IChatComponent;";
        jmethodID m = env->GetMethodID(c, "getDisplayName", chatSig.c_str());
        JniOk(env);
        if (m) {
            jobject chat = env->CallObjectMethod(inv, m);
            JniOk(env);
            out = ChatToString(env, chat);
            if (chat) env->DeleteLocalRef(chat);
        }
    }

    env->DeleteLocalRef(c);
    return out;
}

static int InventorySize(JNIEnv* env, jobject inv) {
    if (!env || !inv) return 0;
    jclass c = env->GetObjectClass(inv);
    if (!c) return 0;
    jmethodID m = env->GetMethodID(c, Mapper::Get("getSizeInventory").c_str(), "()I");
    JniOk(env);
    if (!m) m = env->GetMethodID(c, "getSizeInventory", "()I");
    JniOk(env);
    int n = 0;
    if (m) {
        n = env->CallIntMethod(inv, m);
        JniOk(env);
    }
    env->DeleteLocalRef(c);
    return n;
}

static jobject GetFieldObj(JNIEnv* env, jobject obj, const char* name, const char* sig) {
    if (!env || !obj || !name || !sig) return nullptr;
    jclass c = env->GetObjectClass(obj);
    if (!c) return nullptr;
    jfieldID f = env->GetFieldID(c, name, sig);
    JniOk(env);
    jobject r = nullptr;
    if (f) {
        r = env->GetObjectField(obj, f);
        JniOk(env);
    }
    env->DeleteLocalRef(c);
    return r;
}

static jobject GetContainerFromScreen(JNIEnv* env, jobject screen) {
    std::string contSig = Mapper::Get("net/minecraft/inventory/Container", 2);
    if (contSig.empty()) return nullptr;
    std::string name = Mapper::Get("inventorySlots");
    if (name.empty()) name = "inventorySlots";
    return GetFieldObj(env, screen, name.c_str(), contSig.c_str());
}

static jobject GetLowerInventory(JNIEnv* env, jobject screen) {
    std::string invSig = Mapper::Get("net/minecraft/inventory/IInventory", 2);
    if (invSig.empty()) invSig = "Lnet/minecraft/inventory/IInventory;";
    std::string name = Mapper::Get("lowerChestInventory");
    if (name.empty()) name = "lowerChestInventory";
    jobject r = GetFieldObj(env, screen, name.c_str(), invSig.c_str());
    if (r) return r;
    return GetFieldObj(env, screen, "lowerChestInventory", "Lnet/minecraft/inventory/IInventory;");
}

static jobject GetSlotList(JNIEnv* env, jobject container) {
    return GetFieldObj(env, container, Mapper::Get("inventorySlots").c_str(), "Ljava/util/List;");
}

static int ListSize(JNIEnv* env, jobject list) {
    if (!list) return 0;
    jclass lc = env->FindClass("java/util/List");
    if (!lc) return 0;
    jmethodID m = env->GetMethodID(lc, "size", "()I");
    env->DeleteLocalRef(lc);
    if (!m) return 0;
    int n = env->CallIntMethod(list, m);
    JniOk(env);
    return n;
}

static jobject ListGet(JNIEnv* env, jobject list, int i) {
    if (!list) return nullptr;
    jclass lc = env->FindClass("java/util/List");
    if (!lc) return nullptr;
    jmethodID m = env->GetMethodID(lc, "get", "(I)Ljava/lang/Object;");
    env->DeleteLocalRef(lc);
    if (!m) return nullptr;
    jobject r = env->CallObjectMethod(list, m, i);
    JniOk(env);
    return r;
}

static jobject SlotGetStack(JNIEnv* env, jobject slot) {
    if (!slot) return nullptr;
    jclass c = env->GetObjectClass(slot);
    if (!c) return nullptr;
    std::string stackSig = Mapper::Get("net/minecraft/item/ItemStack", 3);
    if (stackSig.empty()) stackSig = "()Lnet/minecraft/item/ItemStack;";
    std::string mn = Mapper::Get("getStack");
    if (mn.empty()) mn = "getStack";
    jmethodID m = env->GetMethodID(c, mn.c_str(), stackSig.c_str());
    JniOk(env);
    jobject r = nullptr;
    if (m) {
        r = env->CallObjectMethod(slot, m);
        JniOk(env);
    }
    if (!r) {
        std::string fsig = Mapper::Get("net/minecraft/item/ItemStack", 2);
        jfieldID f = env->GetFieldID(c, "stack", fsig.c_str());
        JniOk(env);
        if (f) {
            r = env->GetObjectField(slot, f);
            JniOk(env);
        }
    }
    env->DeleteLocalRef(c);
    return r;
}

static jobject SlotInventory(JNIEnv* env, jobject slot) {
    std::string sig = Mapper::Get("net/minecraft/inventory/IInventory", 2);
    if (sig.empty()) sig = "Lnet/minecraft/inventory/IInventory;";
    return GetFieldObj(env, slot, "inventory", sig.c_str());
}

static int GetWindowId(JNIEnv* env, jobject container) {
    if (!container) return 0;
    jclass c = env->GetObjectClass(container);
    if (!c) return 0;
    jfieldID f = env->GetFieldID(c, Mapper::Get("windowId").c_str(), "I");
    JniOk(env);
    int id = 0;
    if (f) id = env->GetIntField(container, f);
    env->DeleteLocalRef(c);
    return id;
}

static bool TitleLooksLikeChest(const std::string& title) {
    std::string s = title;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s.find("chest") != std::string::npos
        || s.find("coffre") != std::string::npos
        || s.find("ender") != std::string::npos
        || s.find("hopper") != std::string::npos
        || s.find("dispenser") != std::string::npos
        || s.find("dropper") != std::string::npos
        || s.find("barrel") != std::string::npos
        || s.find("shulker") != std::string::npos
        || s.find("large") != std::string::npos
        || s.find("minichest") != std::string::npos;
}

static bool IsUsefulItem(JNIEnv* env, ItemStack* st) {
    if (!st) return false;
    if (Weapons_IsStack(env, st, -1) || st->IsBlock(env) || st->IsEnderPearl(env) || st->IsSoup(env) || st->IsRod(env))
        return true;
    int id = st->GetItemId(env);
    switch (id) {
    case 30: case 46: case 259: case 261: case 262: case 282:
    case 297: case 320: case 322: case 325: case 326: case 327:
    case 332: case 344: case 346: case 364: case 368: case 373:
    case 396:
        return true;
    default:
        break;
    }
    if (id >= 256 && id <= 279) return true;
    if (id >= 283 && id <= 286) return true;
    if (id >= 298 && id <= 317) return true;
    if (st->Is("net/minecraft/item/ItemArmor", env)) return true;
    if (st->Is("net/minecraft/item/ItemFood", env)) return true;
    if (st->Is("net/minecraft/item/ItemBow", env)) return true;
    if (st->Is("net/minecraft/item/ItemPotion", env)) return true;
    return false;
}

static bool ShouldTake(JNIEnv* env, jobject stack) {
    if (!stack) return false;
    auto* st = (ItemStack*)stack;
    if (st->IsEmpty(env)) return false;
    if (!ChestStealerSettings::intelligent) return true;
    return IsUsefulItem(env, st);
}

struct ScaledRes { int scaledWidth; int scaledHeight; int scaleFactor; };

static int CeilD(double v) {
    int i = (int)v;
    return v > (double)i ? i + 1 : i;
}

static ScaledRes CalcScale(int dw, int dh, int guiScale) {
    ScaledRes d{};
    d.scaledWidth = dw;
    d.scaledHeight = dh;
    d.scaleFactor = 1;
    int i = guiScale == 0 ? 1000 : guiScale;
    while (d.scaleFactor < i &&
        d.scaledWidth / (d.scaleFactor + 1) >= 320 &&
        d.scaledHeight / (d.scaleFactor + 1) >= 240)
        ++d.scaleFactor;
    double w = (double)dw / (double)d.scaleFactor;
    double h = (double)dh / (double)d.scaleFactor;
    d.scaledWidth = CeilD(w);
    d.scaledHeight = CeilD(h);
    return d;
}

static void GetGuiOrigin(JNIEnv* env, jobject screen, const ScaledRes& sr, int& guiLeft, int& guiTop) {
    guiLeft = (sr.scaledWidth - 176) / 2;
    guiTop = (sr.scaledHeight - 166) / 2;
    if (!screen) return;
    Klass* scls = (Klass*)env->GetObjectClass(screen);
    if (!scls) return;
    Field* fl = scls->GetField(env, Mapper::Get("guiLeft").c_str(), "I");
    Field* ft = scls->GetField(env, Mapper::Get("guiTop").c_str(), "I");
    env->DeleteLocalRef((jclass)scls);
    if (fl && ft) {
        int gl = fl->GetIntField(env, screen);
        int gt = ft->GetIntField(env, screen);
        if (gl >= 0 && gt >= 0 && gl < sr.scaledWidth && gt < sr.scaledHeight) {
            guiLeft = gl;
            guiTop = gt;
        }
    }
}

static int FieldInt(JNIEnv* env, jobject obj, const char* name) {
    if (!obj || !name) return 0;
    jclass c = env->GetObjectClass(obj);
    if (!c) return 0;
    jfieldID f = env->GetFieldID(c, name, "I");
    JniOk(env);
    int v = 0;
    if (f) v = env->GetIntField(obj, f);
    env->DeleteLocalRef(c);
    return v;
}

static void GetSlotDisplay(JNIEnv* env, jobject container, int slot, int& x, int& y) {
    x = 8 + (slot % 9) * 18;
    y = 18 + (slot / 9) * 18;
    jobject list = GetSlotList(env, container);
    jobject so = ListGet(env, list, slot);
    if (so) {
        std::string xn = Mapper::Get("xDisplayPosition");
        std::string yn = Mapper::Get("yDisplayPosition");
        if (xn.empty()) xn = "xDisplayPosition";
        if (yn.empty()) yn = "yDisplayPosition";
        x = FieldInt(env, so, xn.c_str());
        y = FieldInt(env, so, yn.c_str());
        env->DeleteLocalRef(so);
    }
    if (list) env->DeleteLocalRef(list);
}

static void SmoothMouseMove(int targetX, int targetY) {
    POINT cur{};
    GetCursorPos(&cur);
    int dx = targetX - cur.x;
    int dy = targetY - cur.y;
    double dist = sqrt((double)dx * dx + (double)dy * dy);
    if (dist < 5.0) {
        SetCursorPos(targetX, targetY);
        return;
    }
    int steps = (int)(dist / 16.0);
    steps = (std::max)(4, (std::min)(steps, 18));
    int startX = cur.x, startY = cur.y;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float ease = t * t * (3.f - 2.f * t);
        int x = startX + (int)(dx * ease);
        int y = startY + (int)(dy * ease);
        if (i < steps) {
            std::uniform_int_distribution jitter(-1, 1);
            x += jitter(g_rng);
            y += jitter(g_rng);
        }
        SetCursorPos(x, y);
        Sleep(2);
    }
    SetCursorPos(targetX, targetY);
}

static bool SlotToScreen(JNIEnv* env, jobject screen, jobject container, int slot, int& outX, int& outY) {
    int dw = Minecraft::GetDisplayWidth(env);
    int dh = Minecraft::GetDisplayHeight(env);
    if (dw <= 0 || dh <= 0) return false;
    int guiScale = 0;
    jobject gs = Minecraft::GetGameSettings(env);
    if (gs) {
        guiScale = ((GameSettings*)gs)->GetGuiScale(env);
        env->DeleteLocalRef(gs);
    }
    ScaledRes sr = CalcScale(dw, dh, guiScale);
    int guiLeft = 0, guiTop = 0, slotX = 0, slotY = 0;
    GetGuiOrigin(env, screen, sr, guiLeft, guiTop);
    GetSlotDisplay(env, container, slot, slotX, slotY);
    int sx = guiLeft + slotX + 8;
    int sy = guiTop + slotY + 8;
    double scaleX = (double)dw / (double)sr.scaledWidth;
    double scaleY = (double)dh / (double)sr.scaledHeight;
    outX = (int)(sx * scaleX);
    outY = (int)(sy * scaleY);
    if (!Minecraft::IsFullscreen(env)) {
        HWND wnd = FindLunarWindow();
        if (wnd) {
            POINT o{ 0, 0 };
            ClientToScreen(wnd, &o);
            outX += o.x;
            outY += o.y;
        }
    }
    std::uniform_int_distribution off(-3, 3);
    outX += off(g_rng);
    outY += off(g_rng);
    return true;
}

static void ShiftClickSlot(JNIEnv* env, jobject screen, jobject container, int slot) {
    int x = 0, y = 0;
    if (!SlotToScreen(env, screen, container, slot, x, y))
        return;
    SmoothMouseMove(x, y);
    HoldShift();
    Sleep(18);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(12);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

void ChestStealer::Run(JNIEnv* env) {
    if (!IsLunar() || !enabled || !env || Overlay::isOpen) {
        ResetSession();
        Sleep(20);
        return;
    }

    jobject screen = Minecraft::GetCurrentScreen(env);
    JniOk(env);
    if (!screen || !((GuiScreen*)screen)->IsChestGui(env)) {
        if (screen) env->DeleteLocalRef(screen);
        ResetSession();
        Sleep(8);
        return;
    }

    jobject player = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!player) {
        env->DeleteLocalRef(screen);
        Sleep(8);
        return;
    }

    jobject container = GetContainerFromScreen(env, screen);
    if (!container)
        container = ((Player*)player)->GetOpenContainer(env);
    JniOk(env);
    if (!container) {
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(screen);
        Sleep(8);
        return;
    }

    int windowId = GetWindowId(env, container);
    if (windowId != s_lastWindow) {
        s_lastWindow = windowId;
        s_closing = false;
        s_skip.clear();
        s_nextAction = GetTickCount64() + (ULONGLONG)(std::max)(0, ChestStealerSettings::firstDelay);
    }

    if (ChestStealerSettings::nameCheck) {
        std::string title;
        jobject lower = GetLowerInventory(env, screen);
        if (lower) {
            title = InventoryTitle(env, lower);
            env->DeleteLocalRef(lower);
        }
        if (title.empty()) {
            jobject list = GetSlotList(env, container);
            jobject slot0 = ListGet(env, list, 0);
            if (slot0) {
                jobject inv = SlotInventory(env, slot0);
                title = InventoryTitle(env, inv);
                if (inv) env->DeleteLocalRef(inv);
                env->DeleteLocalRef(slot0);
            }
            if (list) env->DeleteLocalRef(list);
        }
        if (!TitleLooksLikeChest(title)) {
            env->DeleteLocalRef(container);
            env->DeleteLocalRef(player);
            env->DeleteLocalRef(screen);
            Sleep(15);
            return;
        }
    }

    jobject list = GetSlotList(env, container);
    int listN = ListSize(env, list);
    int chestN = 0;
    jobject lower = GetLowerInventory(env, screen);
    if (lower) {
        chestN = InventorySize(env, lower);
        env->DeleteLocalRef(lower);
    }
    if (chestN <= 0 && listN >= 36)
        chestN = listN - 36;
    if (chestN <= 0 || chestN > 90)
        chestN = (listN > 0 && listN < 36) ? listN : 27;

    std::vector<int> take;
    take.reserve(chestN);
    for (int i = 0; i < chestN; i++) {
        if (std::find(s_skip.begin(), s_skip.end(), i) != s_skip.end())
            continue;
        jobject slot = ListGet(env, list, i);
        jobject stack = SlotGetStack(env, slot);
        if (ShouldTake(env, stack))
            take.push_back(i);
        if (stack) env->DeleteLocalRef(stack);
        if (slot) env->DeleteLocalRef(slot);
    }
    if (list) env->DeleteLocalRef(list);

    ULONGLONG now = GetTickCount64();
    if (now < s_nextAction) {
        env->DeleteLocalRef(container);
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(screen);
        Sleep(1);
        return;
    }

    if (take.empty()) {
        if (ChestStealerSettings::autoClose && !s_closing) {
            s_closing = true;
            s_nextAction = now + (ULONGLONG)(std::max)(0, ChestStealerSettings::closeDelay);
            env->DeleteLocalRef(container);
            env->DeleteLocalRef(player);
            env->DeleteLocalRef(screen);
            Sleep(1);
            return;
        }
        if (ChestStealerSettings::autoClose && s_closing) {
            ((Player*)player)->CloseScreen(env);
            JniOk(env);
            ResetSession();
        }
        env->DeleteLocalRef(container);
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(screen);
        Sleep(8);
        return;
    }

    s_closing = false;
    int slot = take.front();
    if (ChestStealerSettings::randomize && take.size() > 1) {
        std::uniform_int_distribution<int> d(0, (int)take.size() - 1);
        slot = take[d(g_rng)];
    }

    jobject before = nullptr;
    {
        jobject slist = GetSlotList(env, container);
        jobject so = ListGet(env, slist, slot);
        before = SlotGetStack(env, so);
        if (so) env->DeleteLocalRef(so);
        if (slist) env->DeleteLocalRef(slist);
    }

    ShiftClickSlot(env, screen, container, slot);
    JniOk(env);

    jobject after = nullptr;
    {
        jobject slist = GetSlotList(env, container);
        jobject so = ListGet(env, slist, slot);
        after = SlotGetStack(env, so);
        if (so) env->DeleteLocalRef(so);
        if (slist) env->DeleteLocalRef(slist);
    }
    bool stillThere = after && !((ItemStack*)after)->IsEmpty(env);
    if (after) env->DeleteLocalRef(after);
    if (before) env->DeleteLocalRef(before);
    if (stillThere)
        s_skip.push_back(slot);

    s_nextAction = GetTickCount64() + (ULONGLONG)RandDelay(ChestStealerSettings::delayMin, ChestStealerSettings::delayMax);

    env->DeleteLocalRef(container);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(screen);
    Sleep(1);
}
