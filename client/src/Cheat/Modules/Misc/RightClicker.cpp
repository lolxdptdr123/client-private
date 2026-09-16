#include "pch.h"
#include "RightClicker.h"

#include "../Combat/clicker.h"
#include "../Combat/AutoRefill.h"
#include "../Combat/Throw.h"
#include "../Misc/Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/GuiScreen.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <thread>

static std::thread       g_thread;
static std::atomic<bool> g_running{ false };

static int RandomInt(int mn, int mx) {
    if (mx <= mn) return mn;
    return mn + rand() % ((mx + 1) - mn);
}

static double BoxMuller(double mean, double stddev) {
    double u1 = 1.0 - rand() / (double)(RAND_MAX + 1);
    double u2 = 1.0 - rand() / (double)(RAND_MAX + 1);
    if (u1 < 1e-12) u1 = 1e-12;
    return mean + stddev * sqrt(-2.0 * log(u1)) * sin(2.0 * 3.1415926535897931 * u2);
}

static void DoRightClick(int holdMs) {
    INPUT down = {}; down.type = INPUT_MOUSE; down.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    INPUT up = {}; up.type = INPUT_MOUSE; up.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    SendInput(1, &down, sizeof(INPUT));
    if (holdMs > 0)
        Sleep(holdMs);
    SendInput(1, &up, sizeof(INPUT));
}

static void ClickBlatant() {
    const int cps = (std::max)(5, (std::min)(25, RightClicker::cps));
    const int interval = (std::max)(40, 1000 / cps);
    DoRightClick(RandomInt(8, 16));
    int wait = interval;
    if (RightClicker::exhaust) {
        if (RandomInt(0, 100) >= 95) wait += RandomInt(90, 180);
        if (RandomInt(0, 100) >= 99) wait += RandomInt(120, 240);
    }
    Sleep((std::max)(1, wait - 12));
}

static void ClickNormal() {
    const float average = (float)(std::max)(5, (std::min)(25, RightClicker::cps));
    const float meanTime = 1000.0f / average;
    int releaseDelay = (int)BoxMuller(meanTime, meanTime / 4.0f);
    if (RightClicker::exhaust) {
        if (RandomInt(0, 100) >= 95)
            releaseDelay = RandomInt(90, 180);
        if (RandomInt(0, 100) >= 99)
            releaseDelay = RandomInt(120, 240);
    }
    const int holdDelay = RandomInt(10, 20);
    DoRightClick(holdDelay);
    releaseDelay -= holdDelay;
    if (releaseDelay > 0)
        Sleep(releaseDelay);
}

static void ClickLoop() {
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

    while (g_running) {
        if (!env || !RightClicker::enabled || Overlay::isOpen || !IsGameWindowFocused()
            || AutoRefill_IsBusy() || Throw_IsBusy()) {
            Sleep(10);
            continue;
        }

        jobject screenObj = Minecraft::GetCurrentScreen(env);
        const bool inWorld = (screenObj == nullptr);
        if (screenObj) env->DeleteLocalRef(screenObj);
        if (!inWorld) {
            Sleep(10);
            continue;
        }

        if (!g_physicalRightDown.load()) {
            Sleep(15);
            continue;
        }

        if (RightClicker::blatant)
            ClickBlatant();
        else
            ClickNormal();
    }

    if (attached && jvm)
        jvm->DetachCurrentThread();
}

void RightClicker::Start() {
    if (g_running) return;
    g_running = true;
    g_thread = std::thread(ClickLoop);
}

void RightClicker::Stop() {
    g_running = false;
    if (g_thread.joinable())
        g_thread.join();
}
