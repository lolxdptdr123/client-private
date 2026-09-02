#include "pch.h"
#include "FastPlace.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Helper/Utils.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static ULONGLONG s_lastClickTime = 0;
static DWORD s_pid = 0;

static int RandInt(int mn, int mx) {
    if (mx < mn) return mn;
    return mn + rand() % (mx - mn + 1);
}

static double BoxMuller(double mean, double stddev) {
    double u1 = 1.0 - rand() / (double)(RAND_MAX + 1);
    double u2 = 1.0 - rand() / (double)(RAND_MAX + 1);
    if (u1 < 1e-12) u1 = 1e-12;
    double n = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2);
    return mean + stddev * n;
}

static bool HoldingBlock(JNIEnv* env) {
    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) return false;
    jobject stack = ((Player*)playerObj)->GetHeldItem(env);
    if (!stack) return false;
    bool ok = ((ItemStack*)stack)->IsBlock(env);
    env->DeleteLocalRef(stack);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return ok;
}

static HWND GameHwnd() {
    HWND h = GetForegroundWindow();
    if (!h) return nullptr;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (s_pid == 0) s_pid = GetCurrentProcessId();
    if (pid != s_pid) {
        h = FindLunarWindow();
        if (!h) return nullptr;
        pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid != s_pid) return nullptr;
    }
    return h;
}

void FastPlace::Run(JNIEnv* env) {
    if (!enabled || !env || Overlay::isOpen) {
        Sleep(20);
        return;
    }

    if (FastPlaceSettings::mode == 0) {
        if (!FastPlaceSettings::onlyBlock || HoldingBlock(env)) {
            int t = Minecraft::GetRightClickDelayTimer(env);
            if (t == 4)
                Minecraft::SetRightClickDelayTimer(env, FastPlaceSettings::tickDelay);
        }
        Sleep(1);
        return;
    }

    if (FastPlaceSettings::holdToClick && (GetAsyncKeyState(VK_RBUTTON) & 0x8000) == 0) {
        Sleep(5);
        return;
    }

    if (FastPlaceSettings::onlyBlock && !HoldingBlock(env)) {
        Sleep(5);
        return;
    }

    HWND hWnd = GameHwnd();
    if (!hWnd) {
        Sleep(5);
        return;
    }

    const float meanTime = 1000.f / (std::max)(1.f, FastPlaceSettings::average);
    const float meanDev = meanTime / 4.f;
    int cycleDelay = (int)BoxMuller((double)meanTime, (double)meanDev);
    if (cycleDelay < 8) cycleDelay = 8;

    if (FastPlaceSettings::exhaust) {
        if (RandInt(0, 100) >= 95)
            cycleDelay = (int)(90.f * ((float)rand() / (float)RAND_MAX) + 90.f);
        if (RandInt(0, 100) >= 99)
            cycleDelay = (int)(120.f * ((float)rand() / (float)RAND_MAX) + 120.f);
    }

    const int holdDelay = RandInt(10, 20);

    PostMessageA(hWnd, WM_RBUTTONDOWN, 0, 0);
    Sleep(holdDelay);
    PostMessageA(hWnd, WM_RBUTTONUP, 0, 0);

    const ULONGLONG now = GetTickCount64();
    if (s_lastClickTime == 0) s_lastClickTime = now;
    if ((now - s_lastClickTime) >= (ULONGLONG)(1500 + RandInt(0, 2500))) {
        if (RandInt(1, 100) <= RandInt(10, 18)) {
            Sleep(holdDelay);
            PostMessageA(hWnd, WM_RBUTTONDOWN, 0, 0);
            Sleep(holdDelay);
            PostMessageA(hWnd, WM_RBUTTONUP, 0, 0);
            cycleDelay -= 2 * holdDelay;
            s_lastClickTime = now;
        }
    }

    const int remaining = cycleDelay - holdDelay;
    if (remaining > 0) Sleep(remaining);
}
