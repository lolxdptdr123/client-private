#include "pch.h"
#include "HookFunction.h"
#include "../../vendors/minhook/MinHook.h"

static HookFunctionBase* g_hookFunctions;

void HookFunctionBase::Register()
{
    m_next = g_hookFunctions;
    g_hookFunctions = this;
}

void HookFunctionBase::RunAll()
{
    // Initialiser MinHook une seule fois ici, centralement.
    // Evite que chaque HookFunction lambda appelle MH_Initialize()
    // individuellement, ce qui corrompt l'état interne de MinHook
    // dès le second appel.
    MH_Initialize();

    for (auto func = g_hookFunctions; func; func = func->m_next)
    {
        func->Run();
    }
}
