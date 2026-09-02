#include "pch.h"
#include "Hack.h"
#include "../Cheat/Modules/Combat/JNIMemory.h"

#include "Modules/Module.h"
#include "Modules/Settings.h"
#include "Modules/Combat/clicker.h"

#include "../Helper/Communication.h"

#include "../Game/Mapper.h"
#include "../Helper/Utils.h"

#include "../../vendors/minhook/MinHook.h"
#include "../../vendors/imgui/imgui.h"
#include "../../vendors/imgui/imgui_impl_opengl2.h"

Hack::Hack()
	: m_InitializationState(WORKING), m_Jvm(nullptr), m_CleanJVMTI(true)
{
	m_Memory = std::make_unique<Memory>();
}

void Hack::Attach()
{
	jsize vmCount = 0;
	if (JNI_GetCreatedJavaVMs(&m_Jvm, 1, &vmCount) != JNI_OK || vmCount == 0) {
		m_InitializationState = ERR;
		return;
	}

	JNIMemory::Init(m_Jvm);

	// ImGui est initialise sur le thread GL dans wglSwapBuffers, PAS ici.
	// On n'appelle pas ImGui::CreateContext() ni ImGui_ImplOpenGL2_Init() ici.

	jint res = g_Instance->GetJVM()->GetEnv(reinterpret_cast<void**>(&m_Env), JNI_VERSION_1_6);

	if (res == JNI_EDETACHED) {
		res = g_Instance->GetJVM()->AttachCurrentThread(reinterpret_cast<void**>(&m_Env), nullptr);
		if (res != JNI_OK) {
			m_InitializationState = ERR;
		}
		else {
			m_WasAttached = true; // on a attache ce thread, on devra le detacher
		}
	}
}

void Hack::Detach()
{

	Modules::DestroyModules();

	if (m_Jvm && m_WasAttached) {
		m_Jvm->DetachCurrentThread();
	}
	else {
	}

	Communication::Unload();

	m_Jvm = nullptr;
}

HackState Hack::CleanupJVMTI() const
{
	if (!m_CleanJVMTI)
		return INCOMPLETE;

	const auto jvm = (uintptr_t)GetModuleHandleW(L"jvm.dll");

#pragma region offsets
	const auto isJvm16 = g_GameVersion == LUNAR_1_7_10;

	const auto isCustomJvm8 = false;

	const auto _globally_initialized_offset = isJvm16 ? 0x16 : 0x7;
	const auto always_capabilities_startOffset = isJvm16 ? 0x0 : 0x50;
	const auto always_capabilities_size = isJvm16 ? 0x68 : 0x58;
	const auto ext_functions_offset = (isJvm16 || isCustomJvm8) ? 0x1A : 0x16;
	const auto ext_functions_startOffset = isJvm16 ? 0x0 : 0x8;
	constexpr auto ext_functions_size = 0x10;
	constexpr auto _head_environment_startOffset = 0x8;
	constexpr auto _head_environment_size = 0x10;
	const auto JvmtiEventControllerPrivate__initialized_offset = isJvm16 ? 0x10 : 0x8;
	constexpr auto JvmtiEventControllerPrivate__initialized_size = 0x10;
#pragma endregion

	const auto _initialized = m_Memory->GetAddress(m_Memory->FindPattern(jvm, "80 3D ? ? ? 00 00 0F 85 ? ? ? ? 83 3D ? ? ? ? ? 48 89 58 F8") + 0x2, 0x1);
	const auto _globally_initialized = m_Memory->GetAddress(m_Memory->FindPattern(jvm, "75 16 E8 ? ? ? ? E8 ? ? ? ?") - (_globally_initialized_offset - 0x2), 0x1);
	const auto always_capabilities = m_Memory->GetAddress(m_Memory->FindPattern(jvm, "22 05 ? ? ? ? 41 88 00") + 0x2);
	const auto ext_functions = m_Memory->GetAddress(m_Memory->FindPattern(jvm, "0F 8C ? ? ? ? 8B 00") - (ext_functions_offset - 0x3));

	if (IsBadReadPtr(_initialized) || IsBadReadPtr(_globally_initialized) || IsBadReadPtr(always_capabilities) || IsBadReadPtr(ext_functions))
	{
		return ERR;
	}

	*_initialized = 0;
	*_globally_initialized = 0;

	ZeroMemory(always_capabilities - always_capabilities_startOffset, always_capabilities_size);
	ZeroMemory(ext_functions - ext_functions_startOffset, ext_functions_size);
	ZeroMemory(_globally_initialized - JvmtiEventControllerPrivate__initialized_offset, JvmtiEventControllerPrivate__initialized_size);
	return SUCCESS;
}

void Hack::InitializeGame()
{
	const auto _initialized = m_Memory->GetAddress(m_Memory->FindPattern((uintptr_t)GetModuleHandleW(L"jvm.dll"), "80 3D ? ? ? 00 00 0F 85 ? ? ? ? 83 3D ? ? ? ? ? 48 89 58 F8") + 0x2, 0x1);
	m_CleanJVMTI = (*_initialized == 0);

	jvmtiEnv* jvmtiEnvironment;
	if (m_Jvm->GetEnv((void**)&jvmtiEnvironment, JVMTI_VERSION_1_1) != JNI_OK) {
		m_InitializationState = ERR;
		return;
	}

	jclass* classes; jint classCount;
	jvmtiEnvironment->GetLoadedClasses(&classCount, &classes);
	for (int i = 0; i < classCount; i++)
	{
		auto klass = (Klass*)(classes[i]);
		m_CachedKlass.emplace(std::make_pair(klass->GetName(m_Env), klass));
	}

	auto detectVersion = [&]() {
		// 1.7 a EntityPlayerSP (classe parente) ET EntityClientPlayerMP.
		// Detecter 1.8 via EntityPlayerSP seul classait Lunar 1.7 comme 1.8
		// et cassait thePlayer / sendQueue (signature EntityPlayerSP).
		const bool hasClientMP = m_CachedKlass.contains("net.minecraft.client.entity.EntityClientPlayerMP");
		const bool hasPlayerSP = m_CachedKlass.contains("net.minecraft.client.entity.EntityPlayerSP");
		if (hasClientMP)
			g_GameVersion = LUNAR_1_7_10;
		else if (hasPlayerSP)
			g_GameVersion = LUNAR_1_8_9;
		else {
			HWND h = FindLunarWindow();
			wchar_t title[256]{};
			if (h)
				GetWindowTextW(h, title, 256);
			if (wcsstr(title, L"1.7.10"))
				g_GameVersion = LUNAR_1_7_10;
			else
				g_GameVersion = LUNAR_1_8_9;
		}
		Mapper::Initialize(g_GameVersion);
	};

	detectVersion();
	if (FindClass(Mapper::Get("net/minecraft/client/Minecraft")) == NULL)
	{
		m_InitializationState = ERR;
		return;
	}

	uint64_t timer = GetTickCount64();
	while (FindClass(Mapper::Get("net/minecraft/client/renderer/ActiveRenderInfo")) == NULL && !Communication::GetSettings()->m_Destruct) {
		m_CachedKlass.clear();

		jclass* classes;
		jint classCount;
		jvmtiEnvironment->GetLoadedClasses(&classCount, &classes);
		for (int i = 0; i < classCount; i++)
		{
			auto klass = (Klass*)(classes[i]);
			m_CachedKlass.emplace(std::make_pair(klass->GetName(m_Env), klass));
		}
		detectVersion();

		if (GetTickCount64() - timer > 30000) {
			MessageBoxA(NULL, "Initialization is taking longer than usual, aborting. Please report this issue happen again.", "", MB_OK | MB_ICONERROR);
			m_InitializationState = ERR;
			return;
		}

		Sleep(1);
	}

	m_InitializationState = SUCCESS;
}

Klass* Hack::FindClass(std::string klassName) const
{
	std::ranges::replace(klassName.begin(), klassName.end(), '/', '.');
	if (m_CachedKlass.contains(klassName))
		return m_CachedKlass.at(klassName);

	return nullptr;
}

std::unique_ptr<Hack> g_Instance;
HMODULE g_hModule = nullptr;