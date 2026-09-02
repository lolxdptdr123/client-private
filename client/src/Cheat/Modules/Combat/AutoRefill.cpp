#include "pch.h"
#include "AutoRefill.h"

#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Game/Method.h"
#include "../../../Cheat/Hack.h"
#include "../Misc/Overlay.h"
#include "../Visuals/Notifications.h"
#include "../../../Helper/Utils.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_set>
#include <vector>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

static std::thread       g_refillThread;
static std::atomic<bool> g_running{ false };
static std::atomic<bool> g_doRefill{ false };
static std::atomic<bool> g_busy{ false };
static std::atomic<bool> g_legitActive{ false };

static std::vector<std::vector<int>> g_patterns;
static std::mt19937 g_rng{ std::random_device{}() };

static void InitPatterns() {
    if (!g_patterns.empty()) return;
    g_patterns.emplace_back();
    for (int i = 9; i < 36; i++) g_patterns.back().push_back(i);
    g_patterns.emplace_back();
    for (int i = 35; i >= 9; i--) g_patterns.back().push_back(i);
    g_patterns.emplace_back();
    for (int col = 0; col < 9; col++) {
        if (col % 2 == 0) {
            for (int row = 0; row < 3; row++)
                g_patterns.back().push_back(9 + row * 9 + col);
        } else {
            for (int row = 2; row >= 0; row--)
                g_patterns.back().push_back(9 + row * 9 + col);
        }
    }
}

enum class HealType { None, Heal, Soup };

static HealType GetHealType(ItemStack* stack, JNIEnv* env) {
    if (!stack) return HealType::None;
    int dmg = stack->GetMetadata(env);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return HealType::None; }
    if (dmg == 16421 || dmg == 16453) return HealType::Heal;
    if (stack->IsSoup(env)) return HealType::Soup;
    return HealType::None;
}

static bool MatchesItemMode(HealType t) {
    const int mode = AutoRefillSettings::itemMode;
    if (mode == 0) return t == HealType::Heal;
    if (mode == 1) return t == HealType::Soup;
    return t != HealType::None;
}

static int SlotDistance(int a, int b) {
    if (a < 9 || b < 9) return 0;
    int r1 = (a - 9) / 9, c1 = (a - 9) % 9;
    int r2 = (b - 9) / 9, c2 = (b - 9) % 9;
    return abs(r1 - r2) + abs(c1 - c2);
}

static int FindSlot(InventoryPlayer* inv, JNIEnv* env,
    const std::unordered_set<int>& planned, int& lastSlotIndex, int& lastIndex, int& patternIndex)
{
    if (!AutoRefillSettings::randomMode) {
        for (int i = 9; i < 36; i++) {
            if (planned.count(i)) continue;
            jobject stackObj = inv->GetStackInSlot(i, env);
            if (!stackObj) continue;
            HealType t = GetHealType((ItemStack*)stackObj, env);
            env->DeleteLocalRef(stackObj);
            if (MatchesItemMode(t)) return i;
        }
        return -1;
    }

    if (lastSlotIndex == -1 && !g_patterns.empty()) {
        std::uniform_int_distribution<> pd(0, (int)g_patterns.size() - 1);
        patternIndex = pd(g_rng);
        const auto& pat = g_patterns[patternIndex];
        if (!pat.empty()) {
            std::uniform_int_distribution<> sd(0, (int)pat.size() - 1);
            int start = sd(g_rng);
            lastSlotIndex = pat[start];
            lastIndex = start;
            std::uniform_int_distribution extra(1, 100);
            if (extra(g_rng) <= 30)
                patternIndex = (patternIndex + 1) % (int)g_patterns.size();
        }
    }

    const int startPat = patternIndex;
    const int startIdx = lastIndex;
    bool first = true;
    do {
        const auto& pat = g_patterns[patternIndex];
        std::vector<std::pair<int, int>> valid;
        for (int i = 0; i < (int)pat.size(); i++) {
            int index = (lastIndex + i) % (int)pat.size();
            int slot = pat[index];
            if (planned.count(slot)) continue;
            jobject stackObj = inv->GetStackInSlot(slot, env);
            if (!stackObj) continue;
            HealType t = GetHealType((ItemStack*)stackObj, env);
            env->DeleteLocalRef(stackObj);
            if (MatchesItemMode(t))
                valid.emplace_back(index, SlotDistance(lastSlotIndex, slot));
        }
        if (!valid.empty()) {
            std::sort(valid.begin(), valid.end(),
                [](auto& a, auto& b) { return a.second < b.second; });
            int sel = valid[0].first;
            int slot = pat[sel];
            lastIndex = (sel + 1) % (int)pat.size();
            lastSlotIndex = slot;
            return slot;
        }
        patternIndex = (patternIndex + 1) % (int)g_patterns.size();
        if (first) first = false;
        else if (patternIndex == startPat) break;
    } while (true);
    patternIndex = startPat;
    lastIndex = startIdx;
    return -1;
}

static int GetWindowId(jobject container, JNIEnv* env) {
    if (!container) return 0;
    Klass* cls = (Klass*)env->GetObjectClass(container);
    if (!cls) return 0;
    Field* f = cls->GetField(env, Mapper::Get("windowId").c_str(), "I");
    env->DeleteLocalRef((jclass)cls);
    if (!f) return 0;
    return f->GetIntField(env, container);
}

static void GetSlotDisplay(jobject container, int slot, JNIEnv* env, int& x, int& y) {
    x = ((slot - 9) % 9) * 18;
    y = ((slot - 9) / 9) * 18;
    if (!container) return;
    Klass* cls = (Klass*)env->GetObjectClass(container);
    if (!cls) return;
    Field* f = cls->GetField(env, Mapper::Get("inventorySlots").c_str(), "Ljava/util/List;");
    env->DeleteLocalRef((jclass)cls);
    if (!f) return;
    jobject list = f->GetObjectField(env, container);
    if (!list) return;
    jclass listCls = env->FindClass("java/util/List");
    if (!listCls) { env->DeleteLocalRef(list); return; }
    jmethodID sizeM = env->GetMethodID(listCls, "size", "()I");
    jmethodID getM = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
    env->DeleteLocalRef(listCls);
    if (!sizeM || !getM) { env->DeleteLocalRef(list); return; }
    jint n = env->CallIntMethod(list, sizeM);
    if (slot < 0 || slot >= n) { env->DeleteLocalRef(list); return; }
    jobject slotObj = env->CallObjectMethod(list, getM, slot);
    env->DeleteLocalRef(list);
    if (!slotObj) return;
    Klass* scls = (Klass*)env->GetObjectClass(slotObj);
    if (!scls) { env->DeleteLocalRef(slotObj); return; }
    Field* fx = scls->GetField(env, Mapper::Get("xDisplayPosition").c_str(), "I");
    Field* fy = scls->GetField(env, Mapper::Get("yDisplayPosition").c_str(), "I");
    env->DeleteLocalRef((jclass)scls);
    if (fx) x = fx->GetIntField(env, slotObj);
    if (fy) y = fy->GetIntField(env, slotObj);
    env->DeleteLocalRef(slotObj);
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

static void GetGuiOrigin(JNIEnv* env, const ScaledRes& sr, int& guiLeft, int& guiTop) {
    guiLeft = (sr.scaledWidth - 176) / 2;
    guiTop = (sr.scaledHeight - 166) / 2;
    jobject screen = Minecraft::GetCurrentScreen(env);
    if (!screen) return;
    Klass* gc = g_Instance->FindClass(Mapper::Get("net/minecraft/client/gui/inventory/GuiContainer"));
    if (gc && env->IsInstanceOf(screen, (jclass)gc)) {
        Klass* scls = (Klass*)env->GetObjectClass(screen);
        if (scls) {
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
    }
    env->DeleteLocalRef(screen);
}

static double DynamicSpeed(bool active, double base) {
    if (!active) return base;
    std::uniform_real_distribution<double> d(-3.0, 5.0);
    return (std::max)(0.0, base + d(g_rng));
}

static void SmoothMouseMove(int targetX, int targetY, int speed) {
    POINT cur{};
    GetCursorPos(&cur);
    int dx = targetX - cur.x;
    int dy = targetY - cur.y;
    double dist = sqrt((double)dx * dx + (double)dy * dy);
    if (dist < 5 || speed >= 45) {
        SetCursorPos(targetX, targetY);
        return;
    }
    int steps = (int)(dist / speed);
    int minSteps = speed >= 30 ? 2 : 3;
    steps = (std::max)(minSteps, (std::min)(steps, 12));
    int startX = cur.x, startY = cur.y;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float ease = t * t * (3.f - 2.f * t);
        int x = startX + (int)(dx * ease);
        int y = startY + (int)(dy * ease);
        if (i < steps) {
            std::uniform_int_distribution jitter(-2, 2);
            x += jitter(g_rng);
            y += jitter(g_rng);
        }
        SetCursorPos(x, y);
        Sleep(2 + (i % 2));
    }
    SetCursorPos(targetX, targetY);
}

static void WaitTicks(int n) {
    if (n <= 0) return;
    Sleep(n * 50);
}

static int MsToTicks(int ms) {
    if (ms <= 0) return 0;
    return (std::max)(1, (ms + 49) / 50);
}

static void DoWindowClick(JNIEnv* env, int windowId, int slot, jobject player) {
    jobject r = Minecraft::WindowClick(env, windowId, slot, 0, 1, player);
    if (r) env->DeleteLocalRef(r);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

static void ClickLegit(JNIEnv* env, int slot, int reelSpeed, jobject container, jobject player) {
    if (reelSpeed > 0) {
        if (AutoRefillSettings::transition) {
            int adj = (int)DynamicSpeed(AutoRefillSettings::dynamicSpeed, reelSpeed);
            WaitTicks(MsToTicks(adj * 10));
        } else {
            WaitTicks(MsToTicks(reelSpeed * 10));
        }
    }
    if (!g_legitActive.load()) return;

    int guiLeft, guiTop, slotX, slotY;
    int dw = Minecraft::GetDisplayWidth(env);
    int dh = Minecraft::GetDisplayHeight(env);
    jobject gs = Minecraft::GetGameSettings(env);
    int guiScale = 0;
    if (gs) {
        guiScale = ((GameSettings*)gs)->GetGuiScale(env);
        env->DeleteLocalRef(gs);
    }
    ScaledRes sr = CalcScale(dw, dh, guiScale);
    GetGuiOrigin(env, sr, guiLeft, guiTop);
    GetSlotDisplay(container, slot, env, slotX, slotY);
    int sx = guiLeft + slotX + 8;
    int sy = guiTop + slotY + 8;
    double scaleX = dw > 0 ? (double)dw / (double)sr.scaledWidth : 1.0;
    double scaleY = dh > 0 ? (double)dh / (double)sr.scaledHeight : 1.0;
    int realX = (int)(sx * scaleX);
    int realY = (int)(sy * scaleY);

    if (!Minecraft::IsFullscreen(env)) {
        HWND wnd = FindLunarWindow();
        if (wnd) {
            POINT o{ 0, 0 };
            ClientToScreen(wnd, &o);
            realX += o.x;
            realY += o.y;
        }
    }
    if (reelSpeed >= 3) {
        std::uniform_int_distribution off(-6, 6);
        realX += off(g_rng);
        realY += off(g_rng);
    }
    if (AutoRefillSettings::transition) {
        int baseMouse = 15 + (10 - reelSpeed) * 185 / 10;
        int adj = (int)DynamicSpeed(AutoRefillSettings::dynamicSpeed, baseMouse);
        SmoothMouseMove(realX, realY, adj);
    } else {
        SetCursorPos(realX, realY);
    }
    if (reelSpeed >= 2) WaitTicks(1);
    else Sleep(20);
    if (!g_legitActive.load()) return;
    int clicks = reelSpeed == 0 ? 9 : std::clamp(3 - reelSpeed, 1, 3);
    for (int i = 0; i < clicks; i++) {
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    }
}

static bool RunRefill(JNIEnv* env) {
    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) return false;
    auto* player = (Player*)playerObj;

    jobject invObj = player->GetInventoryPlayer(env);
    if (!invObj) { env->DeleteLocalRef(playerObj); return false; }
    auto* inv = (InventoryPlayer*)invObj;

    if (inv->IsHotbarFull(env)) {
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return false;
    }

    jobject gsObj = Minecraft::GetGameSettings(env);
    if (!gsObj) {
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return false;
    }
    auto* gs = (GameSettings*)gsObj;
    jobject invBind = gs->GetKeyBindInventory(env);
    jobject fwdBind = gs->GetKeyBindForward(env);
    if (!invBind || !fwdBind) {
        if (invBind) env->DeleteLocalRef(invBind);
        if (fwdBind) env->DeleteLocalRef(fwdBind);
        env->DeleteLocalRef(gsObj);
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return false;
    }
    auto* keyInv = (KeyBinding*)invBind;
    auto* keyFwd = (KeyBinding*)fwdBind;
    bool wasForward = keyFwd->IsPressed(env);

    int lastSlot = AutoRefillSettings::randomMode ? -1 : 0;
    int lastIndex = 0;
    int patternIndex = 0;
    std::unordered_set<int> planned;
    if (FindSlot(inv, env, planned, lastSlot, lastIndex, patternIndex) == -1) {
        env->DeleteLocalRef(invBind);
        env->DeleteLocalRef(fwdBind);
        env->DeleteLocalRef(gsObj);
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return false;
    }

    NotificationSettings::PushInfo("AutoRefill", "used", "Combat");
    g_busy = true;
    keyInv->SetPressTime(1, env);

    jobject screen = nullptr;
    for (int i = 0; i < 50; i++) {
        if (keyInv->GetPressTime(env) == 0)
            keyInv->SetPressTime(1, env);
        Sleep(20);
        if (screen) env->DeleteLocalRef(screen);
        screen = Minecraft::GetCurrentScreen(env);
        if (screen) break;
    }
    if (!screen) {
        keyFwd->SetPressed(wasForward, env);
        g_busy = false;
        env->DeleteLocalRef(invBind);
        env->DeleteLocalRef(fwdBind);
        env->DeleteLocalRef(gsObj);
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return false;
    }
    env->DeleteLocalRef(screen);

    jobject container = player->GetOpenContainer(env);
    if (!container) {
        player->CloseScreen(env);
        keyFwd->SetPressed(wasForward, env);
        g_busy = false;
        env->DeleteLocalRef(invBind);
        env->DeleteLocalRef(fwdBind);
        env->DeleteLocalRef(gsObj);
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return false;
    }

    const int mode = AutoRefillSettings::mode;
    const int reelSpeed = 10 - AutoRefillSettings::speed;
    bool wasLmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

    if (mode == 1) {
        g_legitActive = true;
        WaitTicks(1);
        keybd_event(VK_SHIFT, 0, 0, 0);
    } else if (mode == 2) {
        timeBeginPeriod(1);
    }

    lastSlot = AutoRefillSettings::randomMode ? -1 : 0;
    lastIndex = 0;
    patternIndex = 0;
    planned.clear();

    if (mode == 0) {
        int iter = 0;
        while (!inv->IsHotbarFull(env) && iter++ < 100) {
            jobject s = Minecraft::GetCurrentScreen(env);
            if (!s) break;
            env->DeleteLocalRef(s);
            int slot = FindSlot(inv, env, planned, lastSlot, lastIndex, patternIndex);
            if (slot == -1) break;
            DoWindowClick(env, GetWindowId(container, env), slot, playerObj);
            lastSlot = slot;
        }
    } else {
        const int empty = inv->CountEmptyHotbarSlots(env);
        std::vector<int> toClick;
        for (int i = 0; i < empty; i++) {
            int slot = FindSlot(inv, env, planned, lastSlot, lastIndex, patternIndex);
            if (slot == -1) break;
            planned.insert(slot);
            toClick.push_back(slot);
            lastSlot = slot;
        }
        if (mode == 2) {
            if (reelSpeed <= 0) {
                int wid = GetWindowId(container, env);
                for (int slot : toClick)
                    DoWindowClick(env, wid, slot, playerObj);
            } else {
                int wid = GetWindowId(container, env);
                for (size_t i = 0; i < toClick.size(); i++) {
                    DoWindowClick(env, wid, toClick[i], playerObj);
                    if (i + 1 < toClick.size())
                        Sleep(reelSpeed * 50);
                }
            }
        } else {
            for (int slot : toClick) {
                jobject s = Minecraft::GetCurrentScreen(env);
                if (!s) break;
                env->DeleteLocalRef(s);
                ClickLegit(env, slot, reelSpeed, container, playerObj);
                if (!g_legitActive.load()) break;
            }
        }
    }

    if (mode == 1) {
        WaitTicks(1);
        if (wasLmb) mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
        g_legitActive = false;
    } else if (mode == 2) {
        timeEndPeriod(1);
        if (wasLmb) mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    }

    Sleep(50);
    jobject after = Minecraft::GetCurrentScreen(env);
    if (after) {
        player->CloseScreen(env);
        env->DeleteLocalRef(after);
    }
    keyFwd->SetPressed(wasForward, env);

    env->DeleteLocalRef(container);
    env->DeleteLocalRef(invBind);
    env->DeleteLocalRef(fwdBind);
    env->DeleteLocalRef(gsObj);
    env->DeleteLocalRef(invObj);
    env->DeleteLocalRef(playerObj);
    g_busy = false;
    return true;
}

static void RefillThreadProc() {
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
    InitPatterns();
    while (g_running) {
        if (!g_doRefill.exchange(false)) {
            Sleep(15);
            continue;
        }
        if (!env) continue;
        if (Overlay::isOpen) continue;
        if (!IsGameWindowFocused()) continue;
        RunRefill(env);
    }
    if (attached && jvm) jvm->DetachCurrentThread();
}

void AutoRefill_Trigger() { g_doRefill = true; }
bool AutoRefill_IsBusy() { return g_busy.load(); }

void AutoRefill_Start() {
    if (g_running) return;
    g_running = true;
    g_refillThread = std::thread(RefillThreadProc);
}

void AutoRefill_Stop() {
    g_running = false;
    g_doRefill = false;
    g_legitActive = false;
    g_busy = false;
    if (g_refillThread.joinable()) g_refillThread.join();
}

void AutoRefill::Run(JNIEnv* env) {
    Sleep(50);
}
