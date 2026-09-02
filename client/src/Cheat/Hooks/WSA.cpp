#include "pch.h"
#include "WSA.h"

#include "../Modules/Module.h"
#include "../Modules/Settings.h"
#include "../../Helper/Communication.h"
#include "../../Helper/HookFunction.h"
#include "../../../vendors/minhook/MinHook.h"

int(__stdcall* g_origWSASend)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD,
    LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

static std::mutex        g_wsaMutex;
static std::atomic<bool> g_wsa_shutdown{ false };

void WSA_SignalShutdown() {
    g_wsa_shutdown = true;
    std::lock_guard<std::mutex> lock(g_wsaMutex);
}

int __stdcall WSASendHook(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesSent, DWORD dwFlags,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    if (!g_wsa_shutdown.load(std::memory_order_acquire))
    {
        std::lock_guard<std::mutex> lock(g_wsaMutex);
        if (!g_wsa_shutdown.load(std::memory_order_relaxed))
        {
            for (const auto& mod : Modules::GetRegisteredModules())
                mod->OnReceiveData();
        }
    }

    return g_origWSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent,
        dwFlags, lpOverlapped, lpCompletionRoutine);
}

static HookFunction hookFunc([]() {
    MH_CreateHookApi(L"Ws2_32.dll", "WSASend", WSASendHook, (void**)&g_origWSASend);
    MH_EnableHook(MH_ALL_HOOKS);
    });