#include "pch.h"
#include "wglSwapBuffers.h"

#include "../Hack.h"
#include "../../Globals.h"

#include "../Modules/Module.h"
#include "../Modules/Settings.h"
#include "../Modules/Misc/Overlay.h"

#include "../../Helper/Communication.h"
#include "../../Helper/Utils.h"
#include "../../Helper/HookFunction.h"
#include "../../../vendors/minhook/MinHook.h"
#include "../../../vendors/imgui/imgui.h"
#include "../../../vendors/imgui/imgui_impl_opengl2.h"
#include "CoolveticaFont.h"
#include <gl/GL.h>
#pragma comment(lib, "opengl32.lib")

int(__stdcall* g_origWglSwapBuffers)(HDC);

HANDLE g_detachDoneEvent = nullptr;
HANDLE g_glDestructDoneEvent = nullptr;

// SUPPRIME : g_glFrameMutex était la source du deadlock.
// Il forçait DestroyModules() à attendre que le GL thread soit libre,
// mais le GL thread attendait DestroyModules() → deadlock.
// La synchronisation est maintenant faite uniquement via les deux events.

static bool    s_glThreadAttached = false;
static bool    contextInitialized = false;
static JNIEnv* env = nullptr;

static HWND    g_hwnd = nullptr;
static WNDPROC g_origWndProc = nullptr;

static DWORD s_frameCount = 0;

// ── VK -> ImGuiKey ─────────────────────────────────────────────────────────
static ImGuiKey VKToImGuiKey(WPARAM vk) {
    switch (vk) {
    case VK_TAB:    return ImGuiKey_Tab;
    case VK_LEFT:   return ImGuiKey_LeftArrow;
    case VK_RIGHT:  return ImGuiKey_RightArrow;
    case VK_UP:     return ImGuiKey_UpArrow;
    case VK_DOWN:   return ImGuiKey_DownArrow;
    case VK_HOME:   return ImGuiKey_Home;
    case VK_END:    return ImGuiKey_End;
    case VK_PRIOR:  return ImGuiKey_PageUp;
    case VK_NEXT:   return ImGuiKey_PageDown;
    case VK_DELETE: return ImGuiKey_Delete;
    case VK_BACK:   return ImGuiKey_Backspace;
    case VK_RETURN: return ImGuiKey_Enter;
    case VK_ESCAPE: return ImGuiKey_Escape;
    case VK_SPACE:  return ImGuiKey_Space;
    case 'A': return ImGuiKey_A; case 'B': return ImGuiKey_B;
    case 'C': return ImGuiKey_C; case 'D': return ImGuiKey_D;
    case 'E': return ImGuiKey_E; case 'F': return ImGuiKey_F;
    case 'G': return ImGuiKey_G; case 'H': return ImGuiKey_H;
    case 'I': return ImGuiKey_I; case 'J': return ImGuiKey_J;
    case 'K': return ImGuiKey_K; case 'L': return ImGuiKey_L;
    case 'M': return ImGuiKey_M; case 'N': return ImGuiKey_N;
    case 'O': return ImGuiKey_O; case 'P': return ImGuiKey_P;
    case 'Q': return ImGuiKey_Q; case 'R': return ImGuiKey_R;
    case 'S': return ImGuiKey_S; case 'T': return ImGuiKey_T;
    case 'U': return ImGuiKey_U; case 'V': return ImGuiKey_V;
    case 'W': return ImGuiKey_W; case 'X': return ImGuiKey_X;
    case 'Y': return ImGuiKey_Y; case 'Z': return ImGuiKey_Z;
    default:  return ImGuiKey_None;
    }
}

// ── WndProc hookée ──────────────────────────────────────────────────────────
static LRESULT WINAPI HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (Overlay::isOpen) {
        ImGuiIO& io = ImGui::GetIO();
        switch (msg) {
        case WM_MOUSEWHEEL:
            io.AddMouseWheelEvent(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
            return 0;
        case WM_MOUSEHWHEEL:
            io.AddMouseWheelEvent((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, 0.0f);
            return 0;
        case WM_CHAR:
            if (wParam > 0 && wParam < 0x10000)
                io.AddInputCharacterUTF16((ImWchar16)wParam);
            return 0;
        case WM_KEYDOWN: case WM_SYSKEYDOWN: {
            io.AddKeyEvent(ImGuiMod_Ctrl, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
            ImGuiKey k = VKToImGuiKey(wParam);
            if (k != ImGuiKey_None) io.AddKeyEvent(k, true);
            return 0;
        }
        case WM_KEYUP: case WM_SYSKEYUP: {
            io.AddKeyEvent(ImGuiMod_Ctrl, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
            ImGuiKey k = VKToImGuiKey(wParam);
            if (k != ImGuiKey_None) io.AddKeyEvent(k, false);
            return 0;
        }
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP:
        case WM_MOUSEMOVE:   return 0;
        }
    }
    return CallWindowProcW(g_origWndProc, hWnd, msg, wParam, lParam);
}

static void FeedMouseInputs() {
    ImGuiIO& io = ImGui::GetIO();
    if (g_hwnd) {
        POINT p; GetCursorPos(&p); ScreenToClient(g_hwnd, &p);
        io.AddMousePosEvent((float)p.x, (float)p.y);
    }
    io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);
    io.MouseDrawCursor = Overlay::isOpen;
}

// ── Hook principal ──────────────────────────────────────────────────────────
bool __stdcall wglSwapBuffersHook(HDC hdc)
{
    s_frameCount++;

    // ── Init (première frame) ──────────────────────────────────────────────
    if (!contextInitialized && !Communication::GetSettings()->m_Destruct)
    {

        jint res = g_Instance->GetJVM()->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        s_glThreadAttached = (res == JNI_EDETACHED);

        if (s_glThreadAttached)
            g_Instance->GetJVM()->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);

        ImGui::CreateContext();
        ImGui_ImplOpenGL2_Init();
        ImGui::GetIO().IniFilename = nullptr;

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        // CRITIQUE : FontDataOwnedByAtlas = false
        // Sans ça, ImGui appelle ImGui::MemFree() sur CoolveticaFont_data
        // lors du DestroyContext() — or c'est un tableau statique, pas un
        // pointeur heap → free() sur adresse statique → crash immédiat.
        ImFontConfig fontCfg;
        fontCfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF(
            (void*)CoolveticaFont_data, (int)CoolveticaFont_size,
            16.f, &fontCfg);
        ImGui_ImplOpenGL2_NewFrame();

        g_hwnd = WindowFromDC(hdc);
        if (g_hwnd) {
            Overlay::gameHwnd = g_hwnd;
            g_origWndProc = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
        }

        if (s_glThreadAttached) {
            g_Instance->GetJVM()->DetachCurrentThread();
            s_glThreadAttached = false;
        }
        env = nullptr;

        contextInitialized = true;
    }

    // ── Frame normale ──────────────────────────────────────────────────────
    if (!Communication::GetSettings()->m_Destruct)
    {
        // Si Lunar n'est pas la fenêtre active → ne rien rendre, ne rien traiter
        bool lunarFocused = IsGameWindowFocused();
        if (!lunarFocused && g_hwnd) {
            HWND fg = GetForegroundWindow();
            if (fg) {
                lunarFocused = (fg == g_hwnd)
                    || (GetAncestor(fg, GA_ROOT) == g_hwnd)
                    || (GetAncestor(g_hwnd, GA_ROOT) == fg)
                    || (GetAncestor(fg, GA_ROOT) == GetAncestor(g_hwnd, GA_ROOT));
            }
        }

        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2{ (float)viewport[2], (float)viewport[3] };

        if (g_Instance && g_Instance->GetJVM()) {
            jint jres = g_Instance->GetJVM()->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (jres == JNI_EDETACHED) {
                if (g_Instance->GetJVM()->AttachCurrentThreadAsDaemon(
                        reinterpret_cast<void**>(&env), nullptr) != JNI_OK)
                    env = nullptr;
                else
                    s_glThreadAttached = true;
            }
        }
        if (env && env->ExceptionCheck()) env->ExceptionClear();
        for (const auto& mod : Modules::GetRegisteredModules())
            mod->OnRender(env);

        FeedMouseInputs();
        ImGui_ImplOpenGL2_NewFrame();
        ImGui::NewFrame();

        if (lunarFocused) {
            for (const auto& mod : Modules::GetRegisteredModules())
                mod->OnImGuiRender(env);
        }

        ImGui::Render();

        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData && drawData->TotalVtxCount > 0)
            ImGui_ImplOpenGL2_RenderDrawData(drawData);

        return g_origWglSwapBuffers(hdc);
    }

    // ── Destruct ───────────────────────────────────────────────────────────
    if (contextInitialized)
    {
        contextInitialized = false;

        // 1. Restaurer la WndProc en premier — stopper les WM_* immédiatement
        if (g_hwnd && g_origWndProc) {
            SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
            g_origWndProc = nullptr;
        }

        // ── ORDRE CRITIQUE — explication de la séquence ───────────────────
        //
        // ANCIEN ORDRE (causait le deadlock) :
        //   GL:  SetEvent(glDestructDone)
        //   EP:  WaitForSingleObject(glDestructDone) → OK
        //   EP:  DestroyModules() → join threads → BLOQUE
        //          (threads modules ne peuvent pas finir car ils
        //           attendent la prochaine frame GL pour continuer,
        //           et le GL thread est bloqué sur WaitForSingleObject)
        //   GL:  WaitForSingleObject(detachDone) → JAMAIS SIGNALE → DEADLOCK
        //
        // NOUVEL ORDRE (correct) :
        //   GL:  ImGui::DestroyContext()  ← fait ICI, avant tout signal
        //          (safe : aucun thread module n'appelle ImGui directement,
        //           seul le GL thread appelle OnImGuiRender, et on est sorti
        //           de la frame normale → aucune concurrence possible)
        //   GL:  dernier swap propre
        //   GL:  SetEvent(glDestructDone)  ← signal EP que ImGui est mort
        //   EP:  DestroyModules() → join threads (maintenant safe)
        //   EP:  SetEvent(detachDone)
        //   GL:  WaitForSingleObject(detachDone) → OK
        //   GL:  MH_Uninitialize() + FreeLibrary

        // 2. Détruire ImGui ICI — avant d'attendre quoi que ce soit.
        //    Le GL thread est le seul à utiliser ImGui (OnImGuiRender est
        //    appelé uniquement depuis ce hook). On vient de sortir de la
        //    frame normale → aucune concurrence → safe de détruire maintenant.
        ImGui_ImplOpenGL2_Shutdown();
        ImGui::DestroyContext();

        // 3. Dernier swap OpenGL propre (contexte encore valide)
        g_origWglSwapBuffers(hdc);

        // 4. Signaler à EntryPoint que ImGui est détruit et qu'on ne
        //    parcourra plus jamais GetRegisteredModules().
        //    EntryPoint peut maintenant appeler DestroyModules() en toute sécurité.
        if (g_glDestructDoneEvent)
            SetEvent(g_glDestructDoneEvent);

        // 5. Attendre que DestroyModules() ait terminé (join threads + delete modules).
        //    Seulement après on peut uninitialiser MinHook — sinon des threads
        //    modules encore vivants pourraient appeler des hooks déjà détruits.
        DWORD wr = WaitForSingleObject(g_detachDoneEvent, 8000);

        // 6. Désactiver le hook — MH_DisableHook restaure les bytes originaux
        //    de wglSwapBuffers dans opengl32.dll. Lunar appellera directement
        //    la vraie fonction à partir de maintenant.
        //    On NE PAS appelle MH_Uninitialize ici : cette fonction libère
        //    la mémoire du trampoline MinHook, mais Lunar peut encore avoir
        //    des threads en cours d'appel de wglSwapBuffers qui passent par
        //    ce trampoline → crash si la mémoire est libérée pendant ce temps.
        //    La mémoire du trampoline sera libérée automatiquement avec la DLL
        //    quand FreeLibrary sera appelé dans 500ms.
        MH_DisableHook(MH_ALL_HOOKS);

        // 7. Cleanup events
        if (g_glDestructDoneEvent) { CloseHandle(g_glDestructDoneEvent); g_glDestructDoneEvent = nullptr; }
        if (g_detachDoneEvent) { CloseHandle(g_detachDoneEvent);     g_detachDoneEvent = nullptr; }

        // 8. Ne pas appeler FreeLibrary depuis le hook.
        //    Le crash venait de FreeLibrary qui déclenchait DllMain(DETACH)
        //    avec des destructeurs statiques C++ (g_anims, g_swordCache, etc.)
        //    s'exécutant dans un contexte instable.
        //    L'injecteur gère le déchargement de son côté via son propre
        //    FreeLibrary. On se contente de signaler qu'on a fini.
        //    Le thread de cleanup fait juste ExitThread pour ne pas bloquer.

        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            Sleep(100);
            ExitThread(0);
            return 0;
            }, nullptr, 0, nullptr);

        return TRUE;
    }

    return g_origWglSwapBuffers(hdc);
}

static HookFunction hookFunc([]()
    {
        MH_STATUS st = MH_CreateHookApi(L"opengl32.dll", "wglSwapBuffers",
            wglSwapBuffersHook, (void**)&g_origWglSwapBuffers);
        MH_EnableHook(MH_ALL_HOOKS);
    });