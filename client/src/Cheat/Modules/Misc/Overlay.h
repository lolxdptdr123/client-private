#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Overlay
// Indique si le menu ImGui est ouvert.
// Quand isOpen == true, le clicker et le scroll sont mis en pause.
// Mettre isOpen = true depuis ton code d'affichage du menu ImGui.
// ─────────────────────────────────────────────────────────────────────────────

namespace Overlay
{
    inline bool isOpen = false;
    inline HWND gameHwnd = nullptr;
}