#include "pch.h"
#include "NoHurtCam.h"

#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../../../../vendors/minhook/MinHook.h"

#include <gl/GL.h>
#pragma comment(lib, "opengl32.lib")
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#include <atomic>
#include <mutex>

static std::atomic<bool> g_noHurtCamOn{ false };
static std::atomic<bool> g_hookOk{ false };
static std::atomic<bool> g_appliedThisFrame{ false };

using GlClearFn = void (APIENTRY*)(GLbitfield);
using NglClearFn = void (JNICALL*)(JNIEnv*, jclass, jint, jlong);
static GlClearFn oGlClear = nullptr;
static GlClearFn oGlClearIcd = nullptr;
static NglClearFn oNglClear = nullptr;

static void ApplyNoHurt(JNIEnv* env) {
    if (!g_noHurtCamOn.load(std::memory_order_relaxed) || !env)
        return;
    if (g_appliedThisFrame.exchange(true, std::memory_order_relaxed))
        return;

    auto* player = (Player*)Minecraft::GetThePlayer(env);
    if (!player)
        return;

    jclass cls = env->GetObjectClass((jobject)player);
    if (!cls)
        return;

    const std::string ht = Mapper::Get("hurtTime");
    if (!ht.empty()) {
        jfieldID f = env->GetFieldID(cls, ht.c_str(), "I");
        if (f) env->SetIntField((jobject)player, f, 0);
        else env->ExceptionClear();
    }

    const std::string yaw = Mapper::Get("attackedAtYaw");
    if (!yaw.empty()) {
        jfieldID f = env->GetFieldID(cls, yaw.c_str(), "F");
        if (f) env->SetFloatField((jobject)player, f, 0.f);
        else env->ExceptionClear();
    }

    env->DeleteLocalRef(cls);
}

static JNIEnv* GlEnv() {
    if (!g_Instance || !g_Instance->GetJVM())
        return nullptr;
    JNIEnv* env = nullptr;
    JavaVM* jvm = g_Instance->GetJVM();
    if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (jvm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), nullptr) != JNI_OK)
            return nullptr;
    }
    return env;
}

static void PreFromGl() {
    if (!g_noHurtCamOn.load(std::memory_order_relaxed))
        return;
    JNIEnv* env = GlEnv();
    if (!env) return;
    if (env->PushLocalFrame(16) == JNI_OK) {
        ApplyNoHurt(env);
        env->PopLocalFrame(nullptr);
    }
}

void APIENTRY hkGlClear(GLbitfield mask) {
    if (mask & GL_DEPTH_BUFFER_BIT)
        PreFromGl();
    if (oGlClear) oGlClear(mask);
}

void APIENTRY hkGlClearIcd(GLbitfield mask) {
    if (mask & GL_DEPTH_BUFFER_BIT)
        PreFromGl();
    if (oGlClearIcd) oGlClearIcd(mask);
}

void JNICALL hkNglClear(JNIEnv* env, jclass clazz, jint mask, jlong fp) {
    if ((mask & GL_DEPTH_BUFFER_BIT) && env && env->PushLocalFrame(16) == JNI_OK) {
        ApplyNoHurt(env);
        env->PopLocalFrame(nullptr);
    }
    if (oNglClear) oNglClear(env, clazz, mask, fp);
}

static bool IsValidProc(void* p) {
    return p && (uintptr_t)p > 0x10000;
}

static bool HookAt(void* target, void* detour, void** orig) {
    if (!target || !orig) return false;
    MH_STATUS st = MH_CreateHook(target, detour, orig);
    if (st == MH_ERROR_ALREADY_CREATED)
        return true;
    if (st != MH_OK)
        return false;
    return MH_EnableHook(target) == MH_OK;
}

static void EnsureHook(JNIEnv* env) {
    if (g_hookOk.load(std::memory_order_relaxed))
        return;
    static std::mutex mu;
    std::lock_guard<std::mutex> lock(mu);

    if (env && !oGlClearIcd) {
        auto tryCls = [&](const char* clsName, const char* method, const char* sig, const char* field) {
            if (oGlClearIcd) return;
            jclass cls = env->FindClass(clsName);
            if (!cls) { env->ExceptionClear(); return; }
            jmethodID mid = env->GetStaticMethodID(cls, method, sig);
            if (!mid) { env->ExceptionClear(); env->DeleteLocalRef(cls); return; }
            jobject caps = env->CallStaticObjectMethod(cls, mid);
            env->DeleteLocalRef(cls);
            if (!caps) { env->ExceptionClear(); return; }
            jclass capsCls = env->GetObjectClass(caps);
            jfieldID fid = capsCls ? env->GetFieldID(capsCls, field, "J") : nullptr;
            if (!fid) env->ExceptionClear();
            else {
                jlong ptr = env->GetLongField(caps, fid);
                if (IsValidProc((void*)(uintptr_t)ptr))
                    HookAt((void*)(uintptr_t)ptr, (void*)&hkGlClearIcd, (void**)&oGlClearIcd);
            }
            if (capsCls) env->DeleteLocalRef(capsCls);
            env->DeleteLocalRef(caps);
        };
        tryCls("org/lwjgl/opengl/GLContext", "getCapabilities",
            "()Lorg/lwjgl/opengl/ContextCapabilities;", "glClear");
        tryCls("org/lwjgl/opengl/GL", "getCapabilities",
            "()Lorg/lwjgl/opengl/GLCapabilities;", "glClear");
    }

    if (g_hookOk.load(std::memory_order_relaxed) && oGlClearIcd)
        return;

    bool any = false;
    HMODULE gl = GetModuleHandleA("opengl32.dll");
    if (gl) {
        void* pClear = (void*)GetProcAddress(gl, "glClear");
        if (IsValidProc(pClear) && HookAt(pClear, (void*)&hkGlClear, (void**)&oGlClear))
            any = true;
        using WglGpa = PROC(WINAPI*)(LPCSTR);
        auto gpa = (WglGpa)GetProcAddress(gl, "wglGetProcAddress");
        if (gpa) {
            void* icd = (void*)gpa("glClear");
            if (IsValidProc(icd) && icd != pClear && HookAt(icd, (void*)&hkGlClearIcd, (void**)&oGlClearIcd))
                any = true;
        }
    }

    if (!oNglClear) {
        HMODULE mods[384];
        DWORD needed = 0;
        if (EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed)) {
            const int n = (int)(needed / sizeof(HMODULE));
            static const char* kNames[] = {
                "Java_org_lwjgl_opengl_GL11_nglClear",
                "Java_org_lwjgl_opengl_GL11_nglClear__IJ",
                "Java_org_lwjgl_opengl_GL11C_nglClear",
                "Java_org_lwjgl_opengl_GL11C_nglClear__IJ",
            };
            for (int i = 0; i < n && !oNglClear; i++) {
                for (const char* name : kNames) {
                    void* p = (void*)GetProcAddress(mods[i], name);
                    if (IsValidProc(p) && HookAt(p, (void*)&hkNglClear, (void**)&oNglClear))
                        break;
                }
            }
        }
    }
    if (oNglClear) any = true;

    if (any)
        g_hookOk.store(true, std::memory_order_relaxed);
}

void NoHurtCam::Run(JNIEnv* env) {
    g_noHurtCamOn.store(enabled, std::memory_order_relaxed);
    if (enabled)
        EnsureHook(env);
}

void NoHurtCam::OnRender(JNIEnv* env) {
    g_noHurtCamOn.store(enabled, std::memory_order_relaxed);
    g_appliedThisFrame.store(false, std::memory_order_relaxed);
    if (enabled)
        EnsureHook(env);
}
