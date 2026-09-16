// ============================================================
//  clicker.cpp
//  CPS constant : intervalle fixe (slider 5-25)
// ============================================================

#include "pch.h"
#include "clicker.h"
#include "AutoRefill.h"
#include "Throw.h"
#include <timeapi.h>
#include "JNIMemory.h"
#include "../Misc/Overlay.h"
#include "../Misc/TickLocker.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/GuiScreen.h"
#include "../../../Game/Classes/MovingObjectPosition.h"
#include "SwordCheck.h"
#include "../../../Helper/Utils.h"
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>
#include <algorithm>

static std::thread       g_clickThread;
static std::thread       g_hookThread;
static std::atomic<bool> g_running{ false };

// ── Require Click ─────────────────────────────────────────────────────────────
static HHOOK             g_mouseHook = nullptr;
std::atomic<bool> g_physicalDown{ false };
std::atomic<bool> g_physicalRightDown{ false };

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        MSLLHOOKSTRUCT* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        bool isInjected = (ms->flags & LLMHF_INJECTED) != 0;
        if (!isInjected) {
            if (wParam == WM_LBUTTONDOWN) g_physicalDown = true;
            if (wParam == WM_LBUTTONUP)   g_physicalDown = false;
            if (wParam == WM_RBUTTONDOWN) g_physicalRightDown = true;
            if (wParam == WM_RBUTTONUP)   g_physicalRightDown = false;
        }
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

static void HookThreadProc() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);
    if (!g_mouseHook) return;
    MSG msg;
    while (g_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
    }
    UnhookWindowsHookEx(g_mouseHook);
    g_mouseHook = nullptr;
}

static bool IsLunarFocused() {
    return IsGameWindowFocused();
}

// ── DoClick ───────────────────────────────────────────────────────────────────
static void DoClick() {
    INPUT down = {}; down.type = INPUT_MOUSE; down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    INPUT up = {}; up.type = INPUT_MOUSE; up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &down, sizeof(INPUT));
    auto t = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - t).count() < 8000) {
        _mm_pause();
    }
    SendInput(1, &up, sizeof(INPUT));
}

static int RandomInt(int mn, int mx) {
    if (mx <= mn) return mn;
    return mn + rand() % ((mx + 1) - mn);
}

static double BoxMuller(double mean, double stddev) {
    double u1 = 1.0 - rand() / (double)(RAND_MAX + 1);
    double u2 = 1.0 - rand() / (double)(RAND_MAX + 1);
    if (u1 < 1e-12) u1 = 1e-12;
    double randStd = sqrt(-2.0 * log(u1)) * sin(2.0 * 3.1415926535897931 * u2);
    return mean + stddev * randStd;
}

static void DoClickButterfly(int holdMs) {
    INPUT down = {}; down.type = INPUT_MOUSE; down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    INPUT up = {}; up.type = INPUT_MOUSE; up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &down, sizeof(INPUT));
    if (holdMs > 0)
        Sleep(holdMs);
    SendInput(1, &up, sizeof(INPUT));
}

static void ClickBlatant() {
    int targetCps = std::clamp(Clicker::cps, 5, 25);
    double delayUs = 1000000.0 / targetCps;
    if (delayUs < 11000.0) delayUs = 11000.0;

    auto start = std::chrono::high_resolution_clock::now();
    DoClick();

    auto after = std::chrono::high_resolution_clock::now();
    long long elapsed = std::chrono::duration_cast<std::chrono::microseconds>(after - start).count();
    long long remaining = (long long)delayUs - elapsed;

    if (remaining > 2000)
        std::this_thread::sleep_for(std::chrono::microseconds(remaining - 2000));

    auto deadline = start + std::chrono::microseconds((long long)delayUs);
    while (std::chrono::high_resolution_clock::now() < deadline) {
        if (!g_running) break;
        _mm_pause();
    }
}

static void ClickWhipPattern() {
    const float average = (float)std::clamp(Clicker::cps, 5, 25);
    const float meanTime = 1000.0f / average;
    const float meanTimeDev = meanTime / 4.0f;

    int releaseDelay = static_cast<int>(BoxMuller(meanTime, meanTimeDev));

    if (Clicker::exhaust) {
        if (RandomInt(0, 100) >= 95)
            releaseDelay = static_cast<int>(90.0f * ((float)rand() / (float)RAND_MAX) + 90.0f);
        if (RandomInt(0, 100) >= 99)
            releaseDelay = static_cast<int>(120.0f * ((float)rand() / (float)RAND_MAX) + 120.0f);
    }

    const int holdDelay = RandomInt(10, 20);
    DoClickButterfly(holdDelay);

    releaseDelay -= holdDelay;
    releaseDelay -= static_cast<int>(average / 10.0f);
    if (releaseDelay > 0)
        Sleep(releaseDelay);
}

// ── ClickLoop ─────────────────────────────────────────────────────────────────
static void ClickLoop()
{
    JavaVM* jvm = (g_Instance && g_Instance->GetJVM()) ? g_Instance->GetJVM() : nullptr;

    bool attached = false;
    JNIEnv* env = nullptr;

    if (jvm) {
        jint res = jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (res == JNI_EDETACHED) {
            if (jvm->AttachCurrentThreadAsDaemon((void**)&env, nullptr) == JNI_OK)
                attached = true;
            else
                env = nullptr;
        }
    }

    while (g_running)
    {
        if (!env) { Sleep(10); continue; }

        if (!Clicker::enabled) { Sleep(10); continue; }
        if (Overlay::isOpen) { Sleep(10); continue; }
        if (AutoRefill_IsBusy()) { Sleep(10); continue; }
        if (Throw_IsBusy()) { Sleep(10); continue; }
        if (!IsLunarFocused()) { Sleep(10); continue; }

        // ── Filtre écrans autorisés ───────────────────────────────────────────
        {
            jobject screenObj = Minecraft::GetCurrentScreen(env);
            auto* screen = reinterpret_cast<GuiScreen*>(screenObj);
            bool allowed = false;
            if (screen == nullptr) {
                allowed = true;
            }
            else if (screen->IsInventory(env)) {
                allowed = true;
            }
            else if (screen->IsContainerGui(env)) {
                allowed = true;
            }
            if (!allowed) { Sleep(10); continue; }
        }

        if (TickLocker_IsBlockingClicks()) { Sleep(1); continue; }

        if (Clicker::requireClick && !g_physicalDown.load()) { Sleep(15); continue; }

        if (!g_running) break;

        if (Clicker::weaponsOnly && !SC_IsHoldingSword(env)) { Sleep(5); continue; }

        if (Clicker::mode == 1 || Clicker::mode == 2)
            ClickWhipPattern();
        else
            ClickBlatant();
    }

    if (attached && jvm)
        jvm->DetachCurrentThread();
}

void Clicker::Start() {
    if (g_running) return;
    srand((unsigned)GetTickCount());
    timeBeginPeriod(1);
    g_running = true;
    g_hookThread = std::thread(HookThreadProc);
    g_clickThread = std::thread(ClickLoop);
}

void Clicker::Stop() {
    g_running = false;
    if (g_hookThread.joinable())  g_hookThread.join();
    if (g_clickThread.joinable()) g_clickThread.join();
    timeEndPeriod(1);
}

void LeftClicker::OnImGuiRender(JNIEnv* env) {}