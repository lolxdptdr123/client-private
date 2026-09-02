#include "pch.h"
#include "Cheat/Hack.h"
#include "Cheat/Modules/Module.h"
#include "Cheat/Modules/Settings.h"
#include "Helper/Communication.h"
#include "Cheat/Modules/Misc/Overlay.h"
#include "Helper/HookFunction.h"
#include "Cheat/Hooks/wglSwapBuffers.h"
DWORD __stdcall Main(HMODULE hModule)
{

    // Creer les deux events avant tout
    g_detachDoneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_glDestructDoneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    g_Instance = std::make_unique<Hack>();
    g_Instance->Attach();

    Communication::GetSettings()->m_Destruct = false;
    Overlay::isOpen = false;

    g_Instance->InitializeGame();

    if (g_Instance->GetInitializationState() == ERR) {
        if (g_detachDoneEvent) { CloseHandle(g_detachDoneEvent);     g_detachDoneEvent = nullptr; }
        if (g_glDestructDoneEvent) { CloseHandle(g_glDestructDoneEvent); g_glDestructDoneEvent = nullptr; }
        MessageBoxA(NULL, "An error occured while initializing dope.", "", MB_OK | MB_ICONERROR);
        FreeLibraryAndExitThread(hModule, 0);
    }
    else if (g_Instance->GetInitializationState() == INCOMPLETE) {
        MessageBoxA(NULL, "The dope initializing process ended with the INCOMPLETE status, we highly recommend restarting your game to prevent any issues.", "", MB_OK | MB_ICONINFORMATION);
    }

    const auto code = g_Instance->CleanupJVMTI();
    if (code == ERR)
        MessageBoxA(NULL, "The dope cleaning process ended with the ERR status, you may not be safe in screenshare.\nIf this issue still happen please report it.", "", MB_OK | MB_ICONINFORMATION);

    Modules::InitializeModules();

    HookFunction::RunAll();

    while (!Communication::GetSettings()->m_Destruct)
        Sleep(25);

    // Le GL thread va détecter m_Destruct à sa prochaine frame,
    // signaler g_glDestructDoneEvent (WndProc restaurée), puis attendre
    // g_detachDoneEvent avant de faire ImGui::DestroyContext().
    //
    // Ici on attend que le GL thread ait au moins restauré la WndProc
    // et signalé g_glDestructDoneEvent, pour s'assurer qu'aucune nouvelle
    // frame GL ne sera rendue pendant DestroyModules().
    DWORD waitRes = WaitForSingleObject(g_glDestructDoneEvent, 5000);

    // DestroyModules() : join threads modules + stop services + delete modules
    // Le GL thread est en attente de g_detachDoneEvent à ce moment —
    // aucune frame GL ne se rendra, donc ImGui n'est pas accédé pendant les joins.
    g_Instance->Detach();

    if (g_detachDoneEvent)
        SetEvent(g_detachDoneEvent);

    // Attendre que le GL thread finisse MH_DisableHook + return TRUE
    // avant de décharger la DLL. Le GL thread fait ~14ms pour MH_DisableHook
    // puis retourne dans opengl32.dll. 300ms = marge très large.
    Sleep(300);

    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        HANDLE hThread = CreateThread(
            nullptr, 0,
            (LPTHREAD_START_ROUTINE)Main,
            hModule, 0, nullptr);

        if (hThread)
            CloseHandle(hThread);
    }

    return TRUE;
}