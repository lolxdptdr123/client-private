#pragma once
#include <memory>

class Hack;

extern std::unique_ptr<Hack> g_Instance;
extern HMODULE g_hModule;  // handle DLL — utilisé par le GL thread pour FreeLibrary