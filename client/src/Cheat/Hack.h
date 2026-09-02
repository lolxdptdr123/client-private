#pragma once
#include "pch.h"
#include "../Game/Klass.h"
#include "../Helper/Memory.h"

#include <jvmti.h>
#include <unordered_map>
#include <string>

class Hack
{
public:
	Hack();
	~Hack() = default;

	auto GetJVM() const {
		return m_Jvm;
	}

	auto GetENV() const {
		return m_Env;
	}

	auto GetInitializationState() const {
		return m_InitializationState;
	}

	void Attach();
	void Detach();
	void InitializeGame();
	HackState CleanupJVMTI() const;
	Klass* FindClass(std::string klassName) const;
private:
	HackState m_InitializationState;
	bool m_CleanJVMTI;

	std::unique_ptr<Memory> m_Memory;
	std::unordered_map<std::string, Klass*> m_CachedKlass;

	JavaVM* m_Jvm;
	JNIEnv* m_Env;
	bool    m_WasAttached = false; // true si on a appele AttachCurrentThread
};

extern std::unique_ptr<Hack> g_Instance;extern HMODULE g_hModule;