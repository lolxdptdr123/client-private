#pragma once
#include <vector>
#include <thread>
#include <jni.h>

// Flag mis à jour par les threads modules : true si le joueur est en jeu
// (pas de GuiScreen MC ouvert). Utilisé par Notifications pour hideIfInGame.
inline std::atomic<bool> g_playerInGame{ false };

class Module
{
public:
	virtual ~Module() = default;

	virtual void Run(JNIEnv* env) {}
	virtual void OnRender(JNIEnv* env) {}
	virtual void OnImGuiRender(JNIEnv* env) {}
	virtual void OnReceiveData() {}
	virtual void OnPlayerPreRenderCallback() {}
	virtual void OnPlayerPostRenderCallback() {}

	// Nécessaire pour ArrayList
	virtual const char* GetName() { return ""; }
	virtual bool        IsEnabled() { return false; }

	// Suffix affiché dans l'ArrayList après le nom (ex: "12 CPS", "4.0", "0%")
	// Retourne "" par défaut — override dans chaque module pour personnaliser.
	virtual const char* GetSuffix() { return ""; }
};

class Modules
{
private:
	static std::vector<Module*> m_Modules;
	static std::vector<std::thread> m_ModuleThread;

public:
	static void InitializeModules();
	static void DestroyModules();

	static const auto& GetRegisteredModules() {
		return m_Modules;
	}
};