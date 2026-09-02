#pragma once
#include <Windows.h>

bool __stdcall wglSwapBuffersHook(HDC hdc);

// Créé dans EntryPoint avant tout.
// Signalé par EntryPoint après Detach() — le GL thread attend ce signal
// avant MH_Uninitialize() + FreeLibrary().
extern HANDLE g_detachDoneEvent;

// Créé dans EntryPoint avant tout.
// Signalé par le GL thread après ImGui::DestroyContext() + dernier swap.
// EntryPoint attend ce signal avant DestroyModules() pour garantir qu'aucune
// frame GL ne parcourra plus GetRegisteredModules().
extern HANDLE g_glDestructDoneEvent;

// NOTE : g_glFrameMutex supprimé.
// Il causait un deadlock : GL bloqué sur WaitForSingleObject(detachDone)
// pendant que DestroyModules() bloquait sur lock_guard(g_glFrameMutex).
// La synchronisation est maintenant assurée uniquement via les deux events,
// avec ImGui détruit AVANT SetEvent(glDestructDone).