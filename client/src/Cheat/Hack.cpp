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

	static const jint kJniVers[] = {
		0x00180000, 0x00150000, 0x00130000, 0x000A0000, JNI_VERSION_1_8, JNI_VERSION_1_6
	};
	m_Env = nullptr;
	for (jint v : kJniVers) {
		jint res = m_Jvm->GetEnv(reinterpret_cast<void**>(&m_Env), v);
		if (res == JNI_OK && m_Env)
			break;
		if (res == JNI_EDETACHED) {
			JavaVMAttachArgs args{};
			args.version = v;
			args.name = const_cast<char*>("lolxd");
			args.group = nullptr;
			res = m_Jvm->AttachCurrentThread(reinterpret_cast<void**>(&m_Env), &args);
			if (res == JNI_OK && m_Env) {
				m_WasAttached = true;
				break;
			}
			m_Env = nullptr;
		}
	}
	if (!m_Env)
		m_InitializationState = ERR;
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
	if (g_GameLauncher == LAUNCHER_CHEATBREAKER)
		return SUCCESS;
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

static jclass FindClassOnThreads(JNIEnv* env, const char* name)
{
	if (!env || !name || !name[0]) return nullptr;
	if (env->ExceptionCheck()) env->ExceptionClear();

	jclass threadClass = env->FindClass("java/lang/Thread");
	jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
	jclass mapClass = env->FindClass("java/util/Map");
	jclass setClass = env->FindClass("java/util/Set");
	if (!threadClass || !classLoaderClass || !mapClass || !setClass) {
		if (env->ExceptionCheck()) env->ExceptionClear();
		return nullptr;
	}
	jmethodID getContext = env->GetMethodID(threadClass, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
	jmethodID loadClass = env->GetMethodID(classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	jmethodID stackTraces = env->GetStaticMethodID(threadClass, "getAllStackTraces", "()Ljava/util/Map;");
	jmethodID keySet = env->GetMethodID(mapClass, "keySet", "()Ljava/util/Set;");
	jmethodID toArray = env->GetMethodID(setClass, "toArray", "()[Ljava/lang/Object;");
	if (!getContext || !loadClass || !stackTraces || !keySet || !toArray) {
		if (env->ExceptionCheck()) env->ExceptionClear();
		return nullptr;
	}

	std::string dotted = name;
	for (char& ch : dotted) if (ch == '/') ch = '.';
	jstring jname = env->NewStringUTF(dotted.c_str());

	jclass found = nullptr;
	jobject map = env->CallStaticObjectMethod(threadClass, stackTraces);
	jobject keys = map ? env->CallObjectMethod(map, keySet) : nullptr;
	auto arr = keys ? (jobjectArray)env->CallObjectMethod(keys, toArray) : nullptr;
	if (arr) {
		jint n = env->GetArrayLength(arr);
		for (jint i = 0; i < n && !found; i++) {
			jobject th = env->GetObjectArrayElement(arr, i);
			if (!th) continue;
			jobject cl = env->CallObjectMethod(th, getContext);
			env->DeleteLocalRef(th);
			if (!cl) continue;
			auto c = (jclass)env->CallObjectMethod(cl, loadClass, jname);
			if (env->ExceptionCheck()) { env->ExceptionClear(); c = nullptr; }
			env->DeleteLocalRef(cl);
			if (c) found = c;
		}
		env->DeleteLocalRef(arr);
	}
	if (keys) env->DeleteLocalRef(keys);
	if (map) env->DeleteLocalRef(map);
	env->DeleteLocalRef(jname);
	env->DeleteLocalRef(threadClass);
	env->DeleteLocalRef(classLoaderClass);
	env->DeleteLocalRef(mapClass);
	env->DeleteLocalRef(setClass);
	return found;
}

static bool SigEndsWithClass(const std::string& got, const std::string& want)
{
	if (got == want)
		return true;
	if (want.empty() || got.size() <= want.size())
		return false;
	if (got.compare(got.size() - want.size(), want.size(), want) != 0)
		return false;
	return got[got.size() - want.size() - 1] == '/';
}

static jclass FindClassOnJvmti(JNIEnv* env, jvmtiEnv* jvmti, const char* name)
{
	if (!jvmti || !name || !name[0]) return nullptr;
	jint count = 0;
	jclass* classes = nullptr;
	if (jvmti->GetLoadedClasses(&count, &classes) != JVMTI_ERROR_NONE || !classes)
		return nullptr;

	std::string want = name;
	for (char& ch : want) if (ch == '.') ch = '/';
	jclass result = nullptr;
	for (jint i = 0; i < count; i++) {
		char* sig = nullptr;
		if (jvmti->GetClassSignature(classes[i], &sig, nullptr) != JVMTI_ERROR_NONE || !sig)
			continue;
		std::string got = sig;
		jvmti->Deallocate((unsigned char*)sig);
		if (got.size() >= 3 && got.front() == 'L' && got.back() == ';')
			got = got.substr(1, got.size() - 2);
		for (char& ch : got) if (ch == '.') ch = '/';
		if (SigEndsWithClass(got, want)) {
			result = env ? (jclass)env->NewLocalRef(classes[i]) : classes[i];
			break;
		}
	}
	jvmti->Deallocate((unsigned char*)classes);
	return result;
}

static jclass ResolveNamedClass(JNIEnv* env, jvmtiEnv* jvmti, const char* name)
{
	if (!name || !name[0]) return nullptr;
	jclass c = nullptr;
	if (env) {
		if (env->ExceptionCheck()) env->ExceptionClear();
		c = env->FindClass(name);
		if (env->ExceptionCheck()) { env->ExceptionClear(); c = nullptr; }
		if (!c) c = FindClassOnThreads(env, name);
	}
	if (!c) c = FindClassOnJvmti(env, jvmti, name);
	return c;
}

static jclass FindClassByStaticField(JNIEnv* env, jvmtiEnv* jvmti, const char* fieldName, const char* fieldSig)
{
	if (!jvmti || !fieldName || !fieldName[0]) return nullptr;
	jint count = 0;
	jclass* classes = nullptr;
	if (jvmti->GetLoadedClasses(&count, &classes) != JVMTI_ERROR_NONE || !classes)
		return nullptr;
	jclass result = nullptr;
	for (jint i = 0; i < count && !result; i++) {
		jint n = 0;
		jfieldID* fields = nullptr;
		if (jvmti->GetClassFields(classes[i], &n, &fields) != JVMTI_ERROR_NONE || !fields)
			continue;
		for (jint f = 0; f < n; f++) {
			char* fname = nullptr;
			char* sig = nullptr;
			jint mods = 0;
			if (jvmti->GetFieldName(classes[i], fields[f], &fname, &sig, nullptr) != JVMTI_ERROR_NONE)
				continue;
			jvmti->GetFieldModifiers(classes[i], fields[f], &mods);
			const bool isStatic = (mods & 0x0008) != 0;
			const bool nameOk = fname && strcmp(fname, fieldName) == 0;
			const bool sigOk = !fieldSig || (sig && strcmp(sig, fieldSig) == 0);
			if (isStatic && nameOk && sigOk)
				result = env ? (jclass)env->NewLocalRef(classes[i]) : classes[i];
			if (fname) jvmti->Deallocate((unsigned char*)fname);
			if (sig) jvmti->Deallocate((unsigned char*)sig);
			if (result) break;
		}
		jvmti->Deallocate((unsigned char*)fields);
	}
	jvmti->Deallocate((unsigned char*)classes);
	return result;
}

static std::string ClassSlashName(jvmtiEnv* jvmti, jclass jc)
{
	char* sig = nullptr;
	if (!jvmti || !jc || jvmti->GetClassSignature(jc, &sig, nullptr) != JVMTI_ERROR_NONE || !sig)
		return {};
	std::string got = sig;
	jvmti->Deallocate((unsigned char*)sig);
	if (got.size() >= 3 && got.front() == 'L' && got.back() == ';')
		got = got.substr(1, got.size() - 2);
	for (char& ch : got) if (ch == '.') ch = '/';
	return got;
}

static jclass FindMinecraftHeuristic(JNIEnv* env, jvmtiEnv* jvmti)
{
	if (!jvmti) return nullptr;
	jint count = 0;
	jclass* classes = nullptr;
	if (jvmti->GetLoadedClasses(&count, &classes) != JVMTI_ERROR_NONE || !classes)
		return nullptr;
	jclass best = nullptr;
	int bestFields = -1;
	for (jint i = 0; i < count; i++) {
		std::string slash = ClassSlashName(jvmti, classes[i]);
		if (slash.empty() || slash[0] == '[' || slash.find("java/") == 0 || slash.find("jdk/") == 0
			|| slash.find("sun/") == 0 || slash.find("javax/") == 0)
			continue;
		std::string selfSig = "L" + slash + ";";
		jint n = 0;
		jfieldID* fields = nullptr;
		if (jvmti->GetClassFields(classes[i], &n, &fields) != JVMTI_ERROR_NONE || !fields)
			continue;
		int staticSelf = 0, staticInt = 0, instObj = 0;
		for (jint f = 0; f < n; f++) {
			char* fname = nullptr;
			char* sig = nullptr;
			jint mods = 0;
			if (jvmti->GetFieldName(classes[i], fields[f], &fname, &sig, nullptr) != JVMTI_ERROR_NONE) {
				if (fname) jvmti->Deallocate((unsigned char*)fname);
				if (sig) jvmti->Deallocate((unsigned char*)sig);
				continue;
			}
			jvmti->GetFieldModifiers(classes[i], fields[f], &mods);
			const bool isStatic = (mods & 0x0008) != 0;
			if (isStatic && sig && selfSig == sig)
				staticSelf++;
			if (isStatic && sig && sig[0] == 'I' && sig[1] == '\0')
				staticInt++;
			if (!isStatic && sig && sig[0] == 'L')
				instObj++;
			if (fname) jvmti->Deallocate((unsigned char*)fname);
			if (sig) jvmti->Deallocate((unsigned char*)sig);
		}
		jvmti->Deallocate((unsigned char*)fields);
		if (staticSelf == 1 && instObj >= 15 && n >= 30 && n > bestFields) {
			bestFields = n;
			best = classes[i];
		}
		(void)staticInt;
	}
	jclass result = nullptr;
	if (best)
		result = env ? (jclass)env->NewLocalRef(best) : best;
	jvmti->Deallocate((unsigned char*)classes);
	return result;
}

static jclass FindAriHeuristic(JNIEnv* env, jvmtiEnv* jvmti)
{
	if (!jvmti) return nullptr;
	jint count = 0;
	jclass* classes = nullptr;
	if (jvmti->GetLoadedClasses(&count, &classes) != JVMTI_ERROR_NONE || !classes)
		return nullptr;
	jclass result = nullptr;
	for (jint i = 0; i < count && !result; i++) {
		jint n = 0;
		jfieldID* fields = nullptr;
		if (jvmti->GetClassFields(classes[i], &n, &fields) != JVMTI_ERROR_NONE || !fields)
			continue;
		int fb = 0, ib = 0;
		for (jint f = 0; f < n; f++) {
			char* fname = nullptr;
			char* sig = nullptr;
			jint mods = 0;
			if (jvmti->GetFieldName(classes[i], fields[f], &fname, &sig, nullptr) != JVMTI_ERROR_NONE) {
				if (fname) jvmti->Deallocate((unsigned char*)fname);
				if (sig) jvmti->Deallocate((unsigned char*)sig);
				continue;
			}
			jvmti->GetFieldModifiers(classes[i], fields[f], &mods);
			if ((mods & 0x0008) && sig) {
				if (strcmp(sig, "Ljava/nio/FloatBuffer;") == 0) fb++;
				if (strcmp(sig, "Ljava/nio/IntBuffer;") == 0) ib++;
			}
			if (fname) jvmti->Deallocate((unsigned char*)fname);
			if (sig) jvmti->Deallocate((unsigned char*)sig);
		}
		jvmti->Deallocate((unsigned char*)fields);
		if (fb >= 3 && ib >= 1)
			result = env ? (jclass)env->NewLocalRef(classes[i]) : classes[i];
	}
	jvmti->Deallocate((unsigned char*)classes);
	return result;
}

static jclass PinLocal(JNIEnv* env, jclass local)
{
	if (!local) return nullptr;
	if (!env) return local;
	jclass g = (jclass)env->NewGlobalRef(local);
	if (g) {
		env->DeleteLocalRef(local);
		return g;
	}
	return local;
}

static jclass sPinnedMc = nullptr;
static jclass sPinnedAri = nullptr;

void Hack::InitializeGame()
{
	auto jvmBase = (uintptr_t)GetModuleHandleW(L"jvm.dll");
	char* initPat = jvmBase ? m_Memory->FindPattern(jvmBase, "80 3D ? ? ? 00 00 0F 85 ? ? ? ? 83 3D ? ? ? ? ? 48 89 58 F8") : nullptr;
	if (!initPat) {
		m_CleanJVMTI = false;
	} else {
		const auto _initialized = m_Memory->GetAddress(initPat + 0x2, 0x1);
		if (IsBadReadPtr(_initialized))
			m_CleanJVMTI = false;
		else
			m_CleanJVMTI = (*_initialized == 0);
	}

	jvmtiEnv* jvmtiEnvironment = nullptr;
	jint jv = m_Jvm->GetEnv((void**)&jvmtiEnvironment, JVMTI_VERSION_1_2);
	if (jv != JNI_OK)
		jv = m_Jvm->GetEnv((void**)&jvmtiEnvironment, JVMTI_VERSION_1_1);
	if (jv != JNI_OK)
		jv = m_Jvm->GetEnv((void**)&jvmtiEnvironment, 0x18000000);
	if (jv != JNI_OK || !jvmtiEnvironment) {
		m_InitializationState = ERR;
		return;
	}

	if (!m_Env) {
		static const jint kJniVers[] = {
			0x00180000, 0x00150000, 0x00130000, 0x000A0000, JNI_VERSION_1_8, JNI_VERSION_1_6
		};
		for (jint v : kJniVers) {
			jint er = m_Jvm->GetEnv((void**)&m_Env, v);
			if (er == JNI_OK && m_Env)
				break;
			if (er == JNI_EDETACHED) {
				JavaVMAttachArgs args{};
				args.version = v;
				args.name = const_cast<char*>("lolxd");
				args.group = nullptr;
				er = m_Jvm->AttachCurrentThread((void**)&m_Env, &args);
				if (er == JNI_OK && m_Env) {
					m_WasAttached = true;
					break;
				}
				m_Env = nullptr;
			}
		}
	}

	auto aliasPinned = [&]() {
		if (sPinnedMc) {
			auto* k = (Klass*)sPinnedMc;
			m_CachedKlass["net/minecraft/client/Minecraft"] = k;
			m_CachedKlass["net.minecraft.client.Minecraft"] = k;
			if (g_GameVersion == LUNAR_1_7_10)
				m_CachedKlass["IllIlllIlIlllIlIIIlIllIlI"] = k;
			else
				m_CachedKlass["IlIlIIlIIllllIlllIIlllIlI"] = k;
		}
		if (sPinnedAri) {
			auto* k = (Klass*)sPinnedAri;
			m_CachedKlass["net/minecraft/client/renderer/ActiveRenderInfo"] = k;
			m_CachedKlass["net.minecraft.client.renderer.ActiveRenderInfo"] = k;
			if (g_GameVersion == LUNAR_1_7_10)
				m_CachedKlass["IllllIlIlIIllIIlIIlllllIl"] = k;
			else
				m_CachedKlass["IlIlllllIIllIlIlllIIlllll"] = k;
		}
	};

	auto recache = [&]() {
		m_CachedKlass.clear();
		jclass* classes = nullptr;
		jint classCount = 0;
		if (jvmtiEnvironment->GetLoadedClasses(&classCount, &classes) != JVMTI_ERROR_NONE || !classes)
			return;
		if (m_Env)
			m_Env->EnsureLocalCapacity(256);

		auto put = [&](const std::string& key, Klass* klass) {
			if (key.empty() || !klass) return;
			m_CachedKlass[key] = klass;
		};

		for (int i = 0; i < classCount; i++) {
			jclass jc = classes[i];
			jclass stored = jc;
			if (m_Env)
				stored = (jclass)m_Env->NewGlobalRef(jc);
			auto klass = (Klass*)stored;
			char* sig = nullptr;
			if (jvmtiEnvironment->GetClassSignature(jc, &sig, nullptr) == JVMTI_ERROR_NONE && sig) {
				if (sig[0] == 'L') {
					std::string slash = sig + 1;
					if (!slash.empty() && slash.back() == ';')
						slash.pop_back();
					put(slash, klass);
					std::string dotted = slash;
					for (char& ch : dotted) if (ch == '/') ch = '.';
					put(dotted, klass);
				}
				jvmtiEnvironment->Deallocate((unsigned char*)sig);
			}
		}
		jvmtiEnvironment->Deallocate((unsigned char*)classes);
		aliasPinned();
	};

	auto pinCb = [&]() {
		const char* mc18 = "IlIlIIlIIllllIlllIIlllIlI";
		const char* mc17 = "IllIlllIlIlllIlIIIlIllIlI";
		const char* ari18 = "IlIlllllIIllIlIlllIIlllll";
		const char* ari17 = "IllllIlIlIIllIIlIIlllllIl";
		const char* fMc18 = "IlIIllllllIlIllIlIIlIIlll";
		const char* fMc17 = "IlIIIlIIlIllIlIIIIlllllll";
		const char* fPr18 = "IlllIlIlIIIllIllIIlIlllll";
		const char* fPr17 = "IlIIlIlIlIIllIllllIllIIll";

		jclass mc = ResolveNamedClass(m_Env, jvmtiEnvironment, mc18);
		if (mc) {
			g_GameLauncher = LAUNCHER_CHEATBREAKER;
			g_GameVersion = LUNAR_1_8_9;
			sPinnedMc = PinLocal(m_Env, mc);
		} else {
			mc = ResolveNamedClass(m_Env, jvmtiEnvironment, mc17);
			if (mc) {
				g_GameLauncher = LAUNCHER_CHEATBREAKER;
				g_GameVersion = LUNAR_1_7_10;
				sPinnedMc = PinLocal(m_Env, mc);
			}
		}
		if (!sPinnedMc) {
			mc = FindClassByStaticField(m_Env, jvmtiEnvironment, fMc18, nullptr);
			if (mc) {
				g_GameLauncher = LAUNCHER_CHEATBREAKER;
				g_GameVersion = LUNAR_1_8_9;
				sPinnedMc = PinLocal(m_Env, mc);
			} else {
				mc = FindClassByStaticField(m_Env, jvmtiEnvironment, fMc17, nullptr);
				if (mc) {
					g_GameLauncher = LAUNCHER_CHEATBREAKER;
					g_GameVersion = LUNAR_1_7_10;
					sPinnedMc = PinLocal(m_Env, mc);
				}
			}
		}

		jclass ari = ResolveNamedClass(m_Env, jvmtiEnvironment, ari18);
		if (!ari) ari = ResolveNamedClass(m_Env, jvmtiEnvironment, ari17);
		if (!ari) ari = FindClassByStaticField(m_Env, jvmtiEnvironment, fPr18, "Ljava/nio/FloatBuffer;");
		if (!ari) ari = FindClassByStaticField(m_Env, jvmtiEnvironment, fPr17, "Ljava/nio/FloatBuffer;");
		if (ari)
			sPinnedAri = PinLocal(m_Env, ari);
	};

	recache();

	auto detectVersion = [&]() {
		const bool hasMcpMc = m_CachedKlass.contains("net.minecraft.client.Minecraft")
			|| m_CachedKlass.contains("net/minecraft/client/Minecraft");
		const bool hasClientMP = m_CachedKlass.contains("net.minecraft.client.entity.EntityClientPlayerMP")
			|| m_CachedKlass.contains("net/minecraft/client/entity/EntityClientPlayerMP");
		const bool hasPlayerSP = m_CachedKlass.contains("net.minecraft.client.entity.EntityPlayerSP")
			|| m_CachedKlass.contains("net/minecraft/client/entity/EntityPlayerSP");
		const bool hasCb18Mc = m_CachedKlass.contains("IlIlIIlIIllllIlllIIlllIlI");
		const bool hasCb17Mc = m_CachedKlass.contains("IllIlllIlIlllIlIIIlIllIlI");

		bool looksCb = hasCb18Mc || hasCb17Mc || sPinnedMc != nullptr;
		if (!looksCb) {
			for (const auto& kv : m_CachedKlass) {
				if (kv.first.find("cheatbreaker") != std::string::npos
					|| kv.first.find("CheatBreaker") != std::string::npos) {
					looksCb = true;
					break;
				}
			}
		}
		if (!looksCb) {
			HWND hw = FindLunarWindow();
			wchar_t title[256]{};
			if (hw)
				GetWindowTextW(hw, title, 256);
			if (wcsstr(title, L"CheatBreaker") || wcsstr(title, L"Cheatbreaker") || wcsstr(title, L"Cheat Breaker"))
				looksCb = true;
		}
		if (!looksCb && !hasMcpMc && !hasClientMP && !hasPlayerSP)
			looksCb = true;
		if (hasClientMP || hasPlayerSP)
			looksCb = false;

		g_GameLauncher = looksCb ? LAUNCHER_CHEATBREAKER : LAUNCHER_LUNAR;

		if (g_GameLauncher == LAUNCHER_CHEATBREAKER) {
			if (hasCb18Mc)
				g_GameVersion = LUNAR_1_8_9;
			else if (hasCb17Mc)
				g_GameVersion = LUNAR_1_7_10;
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
		}
		else if (hasClientMP)
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
	if (!FindClass("net/minecraft/client/Minecraft") && !FindClass("net.minecraft.client.Minecraft")) {
		jclass mcp = ResolveNamedClass(m_Env, jvmtiEnvironment, "net/minecraft/client/Minecraft");
		if (mcp) {
			sPinnedMc = PinLocal(m_Env, mcp);
			g_GameLauncher = LAUNCHER_LUNAR;
			aliasPinned();
		} else {
			pinCb();
			aliasPinned();
		}
	}
	if (sPinnedMc && !FindClass("net/minecraft/client/entity/EntityPlayerSP") && !FindClass("net/minecraft/client/entity/EntityClientPlayerMP"))
		g_GameLauncher = LAUNCHER_CHEATBREAKER;
	Mapper::Initialize(g_GameVersion);

	if (!FindClass(Mapper::Get("net/minecraft/client/Minecraft")) && !sPinnedMc) {
		pinCb();
		aliasPinned();
		Mapper::Initialize(g_GameVersion);
	}

	if (!FindClass(Mapper::Get("net/minecraft/client/Minecraft")) && !sPinnedMc) {
		m_InitializationState = ERR;
		return;
	}

	m_InitializationState = SUCCESS;
}

Klass* Hack::FindClass(std::string klassName) const
{
	if (klassName.empty())
		return nullptr;
	std::string dots = klassName;
	std::string slashes = klassName;
	std::ranges::replace(dots.begin(), dots.end(), '/', '.');
	std::ranges::replace(slashes.begin(), slashes.end(), '.', '/');
	if (m_CachedKlass.contains(dots))
		return m_CachedKlass.at(dots);
	if (m_CachedKlass.contains(slashes))
		return m_CachedKlass.at(slashes);
	if (m_CachedKlass.contains(klassName))
		return m_CachedKlass.at(klassName);
	return nullptr;
}

std::unique_ptr<Hack> g_Instance;
HMODULE g_hModule = nullptr;