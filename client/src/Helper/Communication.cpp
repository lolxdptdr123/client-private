#include "pch.h"
#include "Communication.h"
#include "../Cheat/Modules/Settings.h"

// Settings statiques de secours si le shared memory n'est pas dispo
static Settings s_fallbackSettings;

Settings* Communication::GetSettings()
{
    if (!m_Loaded)
    {
        m_Mapping = OpenFileMappingW(
            FILE_MAP_ALL_ACCESS,
            FALSE,
            L"win32");   // ← wide string correct

        if (m_Mapping)
            m_Buffer = MapViewOfFile(m_Mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);

        m_Loaded = true;
    }

    // Si le shared memory est absent (pas de loader), on retourne un Settings
    // par défaut pour éviter le crash sur pointeur null
    if (!m_Buffer)
        return &s_fallbackSettings;

    return (Settings*)m_Buffer;
}

void Communication::Unload() {
    if (m_Buffer)
        UnmapViewOfFile(m_Buffer);
    if (m_Mapping)
        CloseHandle(m_Mapping);

    // Reset pour permettre une reinjection propre
    m_Buffer = nullptr;
    m_Mapping = nullptr;
    m_Loaded = false;
}

bool   Communication::m_Loaded = false;
HANDLE Communication::m_Mapping = nullptr;
LPVOID Communication::m_Buffer = nullptr;