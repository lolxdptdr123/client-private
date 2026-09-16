#include "pch.h"
#include "Hack.h"
#include "../Cheat/Modules/Combat/JNIMemory.h"

#include "Modules/Module.h"
#include "Modules/Settings.h"
#include "Modules/Combat/clicker.h"

#include "../Helper/Communication.h"

#include "../Game/Mapper.h"
#include "../Helper/Utils.h"
#include "Modules/Settings.h"

#include "../../vendors/minhook/MinHook.h"
#include "../../vendors/imgui/imgui.h"
#include "../../vendors/imgui/imgui_impl_opengl2.h"

Hack::Hack()
	: m_InitializationState(WORKING), m_Jvm(nullptr), m_CleanJVMTI(true), m_Env(nullptr), m_WasAttached(false)
{
	m_Memory = std::make_unique<Memory>();
}

static void EarlyLog(const char* msg)
{
	FILE* f = nullptr;
	fopen_s(&f, "C:\\Users\\bipbo\\Documents\\lolxd_diag.txt", "a");
	if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}

void Hack::Attach()
{
	{ FILE* f = nullptr; fopen_s(&f, "C:\\Users\\bipbo\\Documents\\lolxd_diag.txt", "w"); if (f) fclose(f); }
	EarlyLog("Attach start");

	jsize vmCount = 0;
	if (JNI_GetCreatedJavaVMs(&m_Jvm, 1, &vmCount) != JNI_OK || vmCount == 0) {
		EarlyLog("ERR: no JVM found");
		m_InitializationState = ERR;
		return;
	}
	EarlyLog("JVM found");

	JNIMemory::Init(m_Jvm);

	jint res = m_Jvm->GetEnv(reinterpret_cast<void**>(&m_Env), JNI_VERSION_1_6);
	EarlyLog(res == JNI_OK ? "GetEnv OK" : (res == JNI_EDETACHED ? "GetEnv EDETACHED, attaching..." : "GetEnv ERR"));
	if (res == JNI_EDETACHED) {
		res = m_Jvm->AttachCurrentThread(reinterpret_cast<void**>(&m_Env), nullptr);
		if (res != JNI_OK) {
			EarlyLog("ERR: AttachCurrentThread failed");
			m_InitializationState = ERR;
		} else {
			EarlyLog("AttachCurrentThread OK");
			m_WasAttached = true;
		}
	}
	EarlyLog("Attach done");
}

void Hack::Detach()
{
	Modules::DestroyModules();
	if (m_Jvm && m_WasAttached)
		m_Jvm->DetachCurrentThread();
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
	const auto isJvm16 = g_GameVersion == LUNAR_1_7_10;
	const auto isCustomJvm8 = false;
	const auto _globally_initialized_offset     = isJvm16 ? 0x16 : 0x7;
	const auto always_capabilities_startOffset  = isJvm16 ? 0x0  : 0x50;
	const auto always_capabilities_size         = isJvm16 ? 0x68 : 0x58;
	const auto ext_functions_offset             = (isJvm16 || isCustomJvm8) ? 0x1A : 0x16;
	const auto ext_functions_startOffset        = isJvm16 ? 0x0  : 0x8;
	constexpr auto ext_functions_size           = 0x10;
	constexpr auto _head_environment_startOffset = 0x8;
	constexpr auto _head_environment_size        = 0x10;
	const auto JvmtiEventControllerPrivate__initialized_offset = isJvm16 ? 0x10 : 0x8;
	constexpr auto JvmtiEventControllerPrivate__initialized_size = 0x10;

	const auto _initialized        = m_Memory->GetAddress(m_Memory->FindPattern(jvm, "80 3D ? ? ? 00 00 0F 85 ? ? ? ? 83 3D ? ? ? ? ? 48 89 58 F8") + 0x2, 0x1);
	const auto _globally_initialized = m_Memory->GetAddress(m_Memory->FindPattern(jvm, "75 16 E8 ? ? ? ? E8 ? ? ? ?") - (_globally_initialized_offset - 0x2), 0x1);
	const auto always_capabilities  = m_Memory->GetAddress(m_Memory->FindPattern(jvm, "22 05 ? ? ? ? 41 88 00") + 0x2);
	const auto ext_functions        = m_Memory->GetAddress(m_Memory->FindPattern(jvm, "0F 8C ? ? ? ? 8B 00") - (ext_functions_offset - 0x3));

	if (IsBadReadPtr(_initialized) || IsBadReadPtr(_globally_initialized) || IsBadReadPtr(always_capabilities) || IsBadReadPtr(ext_functions))
		return ERR;

	*_initialized = 0;
	*_globally_initialized = 0;
	ZeroMemory(always_capabilities - always_capabilities_startOffset, always_capabilities_size);
	ZeroMemory(ext_functions - ext_functions_startOffset, ext_functions_size);
	ZeroMemory(_globally_initialized - JvmtiEventControllerPrivate__initialized_offset, JvmtiEventControllerPrivate__initialized_size);
	return SUCCESS;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers : résolution de classe via classloader des threads (comme Whip)
// ─────────────────────────────────────────────────────────────────────────────
static jclass FindClassOnThreads(JNIEnv* env, const char* name)
{
	if (!env || !name || !name[0]) return nullptr;
	if (env->ExceptionCheck()) env->ExceptionClear();

	jclass thr = env->FindClass("java/lang/Thread");
	jclass cl  = env->FindClass("java/lang/ClassLoader");
	jclass map = env->FindClass("java/util/Map");
	jclass set = env->FindClass("java/util/Set");
	if (!thr || !cl || !map || !set) {
		if (env->ExceptionCheck()) env->ExceptionClear();
		return nullptr;
	}
	jmethodID getCtx     = env->GetMethodID(thr, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
	jmethodID loadCls    = env->GetMethodID(cl,  "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	jmethodID allTraces  = env->GetStaticMethodID(thr, "getAllStackTraces", "()Ljava/util/Map;");
	jmethodID keySet     = env->GetMethodID(map, "keySet", "()Ljava/util/Set;");
	jmethodID toArray    = env->GetMethodID(set, "toArray", "()[Ljava/lang/Object;");
	if (!getCtx || !loadCls || !allTraces || !keySet || !toArray) {
		if (env->ExceptionCheck()) env->ExceptionClear();
		return nullptr;
	}

	std::string dotted = name;
	for (char& ch : dotted) if (ch == '/') ch = '.';
	jstring jname = env->NewStringUTF(dotted.c_str());

	jclass found = nullptr;
	jobject mapObj  = env->CallStaticObjectMethod(thr, allTraces);
	jobject keysObj = mapObj ? env->CallObjectMethod(mapObj, keySet) : nullptr;
	auto    arr     = keysObj ? (jobjectArray)env->CallObjectMethod(keysObj, toArray) : nullptr;
	if (arr) {
		jint n = env->GetArrayLength(arr);
		for (jint i = 0; i < n && !found; i++) {
			jobject thObj = env->GetObjectArrayElement(arr, i);
			if (!thObj) continue;
			jobject loader = env->CallObjectMethod(thObj, getCtx);
			env->DeleteLocalRef(thObj);
			if (!loader) continue;
			jclass c = (jclass)env->CallObjectMethod(loader, loadCls, jname);
			if (env->ExceptionCheck()) { env->ExceptionClear(); c = nullptr; }
			env->DeleteLocalRef(loader);
			if (c) found = c;
		}
		env->DeleteLocalRef(arr);
	}
	if (keysObj) env->DeleteLocalRef(keysObj);
	if (mapObj)  env->DeleteLocalRef(mapObj);
	env->DeleteLocalRef(jname);
	return found;
}

// Cherche par signature JVMTI exacte (L<slash>;)
static jclass FindClassOnJvmti(JNIEnv* env, jvmtiEnv* jvmti, const char* name)
{
	if (!jvmti || !name || !name[0]) return nullptr;
	jint count = 0; jclass* classes = nullptr;
	if (jvmti->GetLoadedClasses(&count, &classes) != JVMTI_ERROR_NONE || !classes)
		return nullptr;
	std::string want = name;
	for (char& ch : want) if (ch == '.') ch = '/';
	jclass result = nullptr;
	for (jint i = 0; i < count; i++) {
		char* sig = nullptr;
		if (jvmti->GetClassSignature(classes[i], &sig, nullptr) != JVMTI_ERROR_NONE || !sig) continue;
		std::string got = sig; jvmti->Deallocate((unsigned char*)sig);
		if (got.size() >= 3 && got.front() == 'L' && got.back() == ';')
			got = got.substr(1, got.size() - 2);
		if (got == want) {
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

// Cherche une classe qui a un champ static avec ce nom (et optionnellement cette signature)
static jclass FindClassByStaticField(JNIEnv* env, jvmtiEnv* jvmti, const char* fieldName, const char* fieldSig)
{
	if (!jvmti || !fieldName || !fieldName[0]) return nullptr;
	jint count = 0; jclass* classes = nullptr;
	if (jvmti->GetLoadedClasses(&count, &classes) != JVMTI_ERROR_NONE || !classes)
		return nullptr;
	jclass result = nullptr;
	for (jint i = 0; i < count && !result; i++) {
		jint n = 0; jfieldID* fields = nullptr;
		if (jvmti->GetClassFields(classes[i], &n, &fields) != JVMTI_ERROR_NONE || !fields) continue;
		for (jint f = 0; f < n; f++) {
			char* fname = nullptr; char* sig = nullptr; jint mods = 0;
			if (jvmti->GetFieldName(classes[i], fields[f], &fname, &sig, nullptr) != JVMTI_ERROR_NONE) {
				if (fname) jvmti->Deallocate((unsigned char*)fname);
				if (sig)   jvmti->Deallocate((unsigned char*)sig);
				continue;
			}
			jvmti->GetFieldModifiers(classes[i], fields[f], &mods);
			const bool isStatic = (mods & 0x0008) != 0;
			const bool nameOk   = fname && strcmp(fname, fieldName) == 0;
			const bool sigOk    = !fieldSig || (sig && strcmp(sig, fieldSig) == 0);
			if (isStatic && nameOk && sigOk)
				result = env ? (jclass)env->NewLocalRef(classes[i]) : classes[i];
			if (fname) jvmti->Deallocate((unsigned char*)fname);
			if (sig)   jvmti->Deallocate((unsigned char*)sig);
			if (result) break;
		}
		jvmti->Deallocate((unsigned char*)fields);
	}
	jvmti->Deallocate((unsigned char*)classes);
	return result;
}

// Converti une LocalRef en GlobalRef (libère la local)
static jclass PinGlobal(JNIEnv* env, jclass local)
{
	if (!local) return nullptr;
	if (!env) return local;
	jclass g = (jclass)env->NewGlobalRef(local);
	env->DeleteLocalRef(local);
	return g ? g : local;
}

// ─────────────────────────────────────────────────────────────────────────────
// InitializeGame : SCAN JVMTI SANS NewGlobalRef de masse
// ─────────────────────────────────────────────────────────────────────────────
static jclass sPinnedMc  = nullptr;
static jclass sPinnedAri = nullptr;

static void DiagDump(const char* reason, jvmtiEnv* jvmti = nullptr)
{
	FILE* f = nullptr;
	fopen_s(&f, "C:\\lolxd_diag.txt", "a");
	if (!f) return;
	fprintf(f, "[DIAG] %s\n", reason);
	if (jvmti) {
		jint cnt = 0; jclass* cls2 = nullptr;
		if (jvmti->GetLoadedClasses(&cnt, &cls2) == JVMTI_ERROR_NONE && cls2) {
			fprintf(f, "  total classes: %d\n", cnt);
			int printed = 0;
			for (jint i = 0; i < cnt && printed < 500; i++) {
				char* sig2 = nullptr;
				if (jvmti->GetClassSignature(cls2[i], &sig2, nullptr) == JVMTI_ERROR_NONE && sig2) {
					// On cherche les classes qui ressemblent a du minecraft (obf ou pas)
					std::string s = sig2;
					if (s.find("minecraft") != std::string::npos ||
						s.find("Minecraft") != std::string::npos ||
						s.find("cheatbreaker") != std::string::npos ||
						s.find("CheatBreaker") != std::string::npos ||
						(s.size() > 3 && s[0] == 'L' && s[1] == 'I' && s[2] == 'l') ||
						(s.size() > 3 && s[0] == 'L' && s[1] == 'I' && s[2] == 'I') ||
						(s.size() > 3 && s[0] == 'L' && s[1] == 'I' && s[2] == 'l') ||
						(s.size() > 3 && s[0] == 'L' && s[1] == 'l' && s[2] == 'l')) {
						fprintf(f, "  %s\n", sig2);
						printed++;
					}
					jvmti->Deallocate((unsigned char*)sig2);
				}
			}
		}
		if (cls2) jvmti->Deallocate((unsigned char*)cls2);
	}
	fclose(f);
}

void Hack::InitializeGame()
{
	EarlyLog("InitializeGame start");

	// ── JVMTI cleanup flag ───────────────────────────────────────────────
	{
		auto jvmBase = (uintptr_t)GetModuleHandleW(L"jvm.dll");
		char* pat = jvmBase ? m_Memory->FindPattern(jvmBase, "80 3D ? ? ? 00 00 0F 85 ? ? ? ? 83 3D ? ? ? ? ? 48 89 58 F8") : nullptr;
		if (!pat) {
			m_CleanJVMTI = false;
		} else {
			auto* flag = m_Memory->GetAddress(pat + 0x2, 0x1);
			m_CleanJVMTI = !IsBadReadPtr(flag) && (*flag == 0);
		}
	}

	EarlyLog("after CleanJVMTI check");

	// ── Obtenir jvmtiEnv ─────────────────────────────────────────────────
	jvmtiEnv* jvmti = nullptr;
	if (m_Jvm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2) != JNI_OK)
		m_Jvm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_1);
	if (!jvmti) {
		EarlyLog("ERR: jvmti null");
		m_InitializationState = ERR;
		return;
	}
	EarlyLog("jvmti ok");

	// ── Scan des classes avec attente que Minecraft soit chargé ──────────
	struct ScanResult {
		bool hasLunarMc  = false;
		bool hasLunar17  = false;
		bool hasLunar18  = false;
		bool hasCb18     = false;
		bool hasCb17     = false;
		jclass mcRef     = nullptr;
		jclass ariRef    = nullptr;
	} sr;

	const std::string mc18  = "IlIlIIlIIllllIlllIIlllIlI";
	const std::string mc17  = "IllIlllIlIlllIlIIIlIllIlI";
	const std::string ari18 = "IlIlllllIIllIlIlllIIlllll";
	const std::string ari17 = "IllllIlIlIIllIIlIIlllllIl";

	auto doScan = [&]() {
		sr = {};
		jint count = 0; jclass* classes = nullptr;
		if (jvmti->GetLoadedClasses(&count, &classes) != JVMTI_ERROR_NONE || !classes) return;
		for (jint i = 0; i < count; i++) {
			char* sig = nullptr;
			if (jvmti->GetClassSignature(classes[i], &sig, nullptr) != JVMTI_ERROR_NONE || !sig) continue;
			std::string s = sig; jvmti->Deallocate((unsigned char*)sig);
			if (s.size() < 3 || s[0] != 'L' || s.back() != ';') continue;
			s = s.substr(1, s.size() - 2);
			if (s == "net/minecraft/client/Minecraft")                   sr.hasLunarMc = true;
			if (s == "net/minecraft/client/entity/EntityClientPlayerMP") sr.hasLunar17 = true;
			if (s == "net/minecraft/client/entity/EntityPlayerSP")       sr.hasLunar18 = true;
			if (s == mc18 && !sr.mcRef) { sr.hasCb18 = true; sr.mcRef = classes[i]; }
			if (s == mc17 && !sr.mcRef) { sr.hasCb17 = true; sr.mcRef = classes[i]; }
			if ((s == ari18 || s == ari17) && !sr.ariRef) sr.ariRef = classes[i];
		}
		jvmti->Deallocate((unsigned char*)classes);
	};

	// Attendre que Minecraft soit chargé (max 60s)
	// CB fait du lazy-loading : les classes ne sont pas dans GetLoadedClasses
	// tant que le classloader ne les a pas encore initialisées.
	// On force le chargement via FindClassOnThreads.
	EarlyLog("waiting for Minecraft class via classloader...");
	uint64_t waitStart = GetTickCount64();

	// D'abord scan JVMTI
	doScan();

	// Si pas trouvé en JVMTI, essayer de forcer via classloader
	if (!sr.hasLunarMc && !sr.hasLunar17 && !sr.hasLunar18 && !sr.hasCb18 && !sr.hasCb17) {
		// Attendre que le classloader CB soit prêt (com/cheatbreaker présent)
		bool cbReady = false;
		while (!cbReady && GetTickCount64() - waitStart < 60000) {
			if (Communication::GetSettings()->m_Destruct) { m_InitializationState = ERR; return; }
			jint cnt = 0; jclass* cls = nullptr;
			if (jvmti->GetLoadedClasses(&cnt, &cls) == JVMTI_ERROR_NONE && cls) {
				for (jint i = 0; i < cnt && !cbReady; i++) {
					char* sig = nullptr;
					if (jvmti->GetClassSignature(cls[i], &sig, nullptr) == JVMTI_ERROR_NONE && sig) {
						std::string s = sig; jvmti->Deallocate((unsigned char*)sig);
						if (s.find("cheatbreaker") != std::string::npos || s.find("net/minecraft") != std::string::npos)
							cbReady = true;
					}
				}
				jvmti->Deallocate((unsigned char*)cls);
			}
			if (!cbReady) Sleep(500);
		}
		EarlyLog(cbReady ? "CB/MC classloader ready, forcing load..." : "timeout waiting for classloader");

		if (m_Env) {
			// Forcer le chargement via classloader des threads
			jclass mc = FindClassOnThreads(m_Env, mc18.c_str());
			if (!mc) mc = FindClassOnThreads(m_Env, mc17.c_str());
			if (!mc) mc = FindClassOnThreads(m_Env, "net/minecraft/client/Minecraft");
			// Chercher Minecraft par heuristique sur tous les champs
			if (!mc) {
				EarlyLog("trying heuristic field scan for Minecraft...");
				jint cnt3 = 0; jclass* cls3 = nullptr;
				if (jvmti->GetLoadedClasses(&cnt3, &cls3) == JVMTI_ERROR_NONE && cls3) {
					int bestScore = 0;
					jclass bestClass = nullptr;
					std::string bestName;
					for (jint i = 0; i < cnt3; i++) {
						char* sig3 = nullptr;
						if (jvmti->GetClassSignature(cls3[i], &sig3, nullptr) != JVMTI_ERROR_NONE || !sig3) continue;
						std::string s3 = sig3; jvmti->Deallocate((unsigned char*)sig3);
						if (s3.size() < 10 || s3[0] != 'L' || s3.back() != ';') continue;
						s3 = s3.substr(1, s3.size() - 2);
						if (s3.find('/') != std::string::npos) continue; // skip avec package

						std::string selfSig = "L" + s3 + ";";
						jint nf = 0; jfieldID* fids = nullptr;
						if (jvmti->GetClassFields(cls3[i], &nf, &fids) != JVMTI_ERROR_NONE || !fids) continue;

						int selfRef = 0, staticInt = 0, instObj = 0, totalFields = nf;
						for (jint f = 0; f < nf; f++) {
							char* fn = nullptr; char* fs = nullptr; jint fm = 0;
							if (jvmti->GetFieldName(cls3[i], fids[f], &fn, &fs, nullptr) != JVMTI_ERROR_NONE) {
								if (fn) jvmti->Deallocate((unsigned char*)fn);
								if (fs) jvmti->Deallocate((unsigned char*)fs);
								continue;
							}
							jvmti->GetFieldModifiers(cls3[i], fids[f], &fm);
							bool isSt = (fm & 0x0008) != 0;
							if (isSt && fs && selfSig == fs) selfRef++;
							if (isSt && fs && strcmp(fs, "I") == 0) staticInt++;
							if (!isSt && fs && fs[0] == 'L') instObj++;
							if (fn) jvmti->Deallocate((unsigned char*)fn);
							if (fs) jvmti->Deallocate((unsigned char*)fs);
						}
						jvmti->Deallocate((unsigned char*)fids);

						// Seuils abaissés : selfRef>=1, staticInt>=1, instObj>=3, fields>=10
						// Exclure enums (ont peu de méthodes inst et peu de champs inst)
						if (selfRef >= 1 && staticInt >= 1 && instObj >= 3 && totalFields >= 10) {
							int score = selfRef * 20 + staticInt * 5 + instObj * 3 + totalFields;
							if (score > bestScore) {
								bestScore = score;
								bestClass = cls3[i];
								bestName = s3;
								char logbuf[192]; sprintf_s(logbuf, "MC candidate: %s score=%d fields=%d selfRef=%d staticInt=%d instObj=%d",
									s3.c_str(), score, totalFields, selfRef, staticInt, instObj);
								EarlyLog(logbuf);
							}
						}
					}
					if (bestClass) {
						mc = m_Env ? (jclass)m_Env->NewLocalRef(bestClass) : bestClass;
						char logbuf2[128]; sprintf_s(logbuf2, "best MC: %s score=%d", bestName.c_str(), bestScore);
						EarlyLog(logbuf2);
					}
					jvmti->Deallocate((unsigned char*)cls3);
				}
			}
			if (mc) {
				EarlyLog("found Minecraft");
				sPinnedMc = PinGlobal(m_Env, mc);
				g_GameLauncher = LAUNCHER_CHEATBREAKER;
				g_GameVersion = LUNAR_1_8_9;
				// Détecter 1.7 ou 1.8
				jclass p7 = FindClassOnThreads(m_Env, "net/minecraft/client/entity/EntityClientPlayerMP");
				jclass p8 = FindClassOnThreads(m_Env, "net/minecraft/client/entity/EntityPlayerSP");
				if (p7) { sr.hasLunar17 = true; m_Env->DeleteLocalRef(p7); }
				if (p8) { sr.hasLunar18 = true; m_Env->DeleteLocalRef(p8); }
				doScan();
			} else {
				EarlyLog("Minecraft not found via classloader");
			}
		}
	}

	{
		char buf[256];
		sprintf_s(buf, "after wait: LunarMc=%d L17=%d L18=%d CB18=%d CB17=%d sPinnedMc=%p",
			(int)sr.hasLunarMc, (int)sr.hasLunar17, (int)sr.hasLunar18,
			(int)sr.hasCb18, (int)sr.hasCb17, (void*)sPinnedMc);
		EarlyLog(buf);
	}

	// ── Remplir le cache signature (sans GlobalRef) ───────────────────────
	// On relit la liste pour mettre slash+dot dans m_CachedKlass
	{
		jint count = 0; jclass* classes = nullptr;
		if (jvmti->GetLoadedClasses(&count, &classes) == JVMTI_ERROR_NONE && classes) {
			for (jint i = 0; i < count; i++) {
				char* sig = nullptr;
				if (jvmti->GetClassSignature(classes[i], &sig, nullptr) != JVMTI_ERROR_NONE || !sig) continue;
				std::string sl = sig; jvmti->Deallocate((unsigned char*)sig);
				if (sl.size() < 3 || sl[0] != 'L' || sl.back() != ';') continue;
				sl = sl.substr(1, sl.size() - 2);
				auto* k = (Klass*)classes[i];
				m_CachedKlass[sl] = k;
				std::string dt = sl;
				for (char& c : dt) if (c == '/') c = '.';
				m_CachedKlass[dt] = k;
			}
			jvmti->Deallocate((unsigned char*)classes);
		}
	}

	{
		char buf[512];
		sprintf_s(buf, "scan done: LunarMc=%d L17=%d L18=%d CB18=%d CB17=%d mcRef=%p ariRef=%p cacheSize=%zu",
			(int)sr.hasLunarMc, (int)sr.hasLunar17, (int)sr.hasLunar18,
			(int)sr.hasCb18, (int)sr.hasCb17, (void*)sr.mcRef, (void*)sr.ariRef,
			m_CachedKlass.size());
		EarlyLog(buf);

		// Dump des 200 premiers noms de classes pour diagnostique
		FILE* f2 = nullptr;
		fopen_s(&f2, "C:\\Users\\bipbo\\Documents\\lolxd_classes.txt", "w");
		if (f2) {
			jint cnt2 = 0; jclass* cls2 = nullptr;
			if (jvmti->GetLoadedClasses(&cnt2, &cls2) == JVMTI_ERROR_NONE && cls2) {
				fprintf(f2, "Total classes: %d\n\n", cnt2);
				for (jint i = 0; i < cnt2; i++) {
					char* sig2 = nullptr;
					if (jvmti->GetClassSignature(cls2[i], &sig2, nullptr) == JVMTI_ERROR_NONE && sig2) {
						fprintf(f2, "%s\n", sig2);
						jvmti->Deallocate((unsigned char*)sig2);
					}
				}
				jvmti->Deallocate((unsigned char*)cls2);
			}
			fclose(f2);
		}
	}

	// ── Détecter version / launcher ──────────────────────────────────────
	bool isCB = (sr.hasCb18 || sr.hasCb17);
	if (!isCB && !sr.hasLunarMc && !sr.hasLunar17 && !sr.hasLunar18) {
		// rien de connu → CB probable
		isCB = true;
	}
	if (sr.hasLunar17 || sr.hasLunar18)
		isCB = false;   // Lunar explicitement identifié

	g_GameLauncher = isCB ? LAUNCHER_CHEATBREAKER : LAUNCHER_LUNAR;

	if (isCB) {
		g_GameVersion = sr.hasCb17 ? LUNAR_1_7_10 : LUNAR_1_8_9;

		// Pin Minecraft CB
		if (sr.mcRef) {
			sPinnedMc = PinGlobal(m_Env, m_Env ? (jclass)m_Env->NewLocalRef(sr.mcRef) : sr.mcRef);
		} else {
			// tenter via static field theMinecraft
			const char* fMc18 = "IlIIllllllIlIllIlIIlIIlll";
			const char* fMc17 = "IlIIIlIIlIllIlIIIIlllllll";
			jclass mc = FindClassByStaticField(m_Env, jvmti, fMc18, nullptr);
			if (!mc) mc = FindClassByStaticField(m_Env, jvmti, fMc17, nullptr);
			if (!mc) {
				// dernier recours : classloader threads
				mc = FindClassOnThreads(m_Env, "IlIlIIlIIllllIlllIIlllIlI");
				if (!mc) mc = FindClassOnThreads(m_Env, "IllIlllIlIlllIlIIIlIllIlI");
			}
			if (mc) sPinnedMc = PinGlobal(m_Env, mc);
		}

		// Pin ActiveRenderInfo CB
		if (sr.ariRef) {
			sPinnedAri = PinGlobal(m_Env, m_Env ? (jclass)m_Env->NewLocalRef(sr.ariRef) : sr.ariRef);
		} else {
			const char* fPr18 = "IlllIlIlIIIllIllIIlIlllll";
			const char* fPr17 = "IlIIlIlIlIIllIllllIllIIll";
			jclass ari = FindClassByStaticField(m_Env, jvmti, fPr18, "Ljava/nio/FloatBuffer;");
			if (!ari) ari = FindClassByStaticField(m_Env, jvmti, fPr17, "Ljava/nio/FloatBuffer;");
			if (ari) sPinnedAri = PinGlobal(m_Env, ari);
		}
	} else {
		g_GameVersion = sr.hasLunar17 ? LUNAR_1_7_10 : LUNAR_1_8_9;
	}

	Mapper::Initialize(g_GameVersion);

	auto stripSig = [](const char* sig) -> std::string {
		if (!sig || sig[0] != 'L') return {};
		std::string s = sig;
		if (!s.empty() && s.back() == ';') s.pop_back();
		if (!s.empty() && s.front() == 'L') s.erase(s.begin());
		return s;
	};
	auto classStats = [&](jclass jc, int& nFields, int& nDoubles, int& nFloats, int& nLists, int& nInts) {
		nFields = nDoubles = nFloats = nLists = nInts = 0;
		if (!jc) return;
		jint nf = 0; jfieldID* fids = nullptr;
		if (jvmti->GetClassFields(jc, &nf, &fids) != JVMTI_ERROR_NONE || !fids) return;
		nFields = nf;
		for (jint f = 0; f < nf; f++) {
			char* fn = nullptr; char* fs = nullptr;
			if (jvmti->GetFieldName(jc, fids[f], &fn, &fs, nullptr) == JVMTI_ERROR_NONE && fs) {
				if (strcmp(fs, "D") == 0) nDoubles++;
				else if (strcmp(fs, "F") == 0) nFloats++;
				else if (strcmp(fs, "I") == 0) nInts++;
				else if (fs[0] == 'L' && strstr(fs, "java/util/List")) nLists++;
				else if (fs[0] == 'L' && strstr(fs, "java/util/concurrent")) {}
			}
			if (fn) jvmti->Deallocate((unsigned char*)fn);
			if (fs) jvmti->Deallocate((unsigned char*)fs);
		}
		jvmti->Deallocate((unsigned char*)fids);
	};

	if (sPinnedMc && jvmti && g_GameLauncher == LAUNCHER_CHEATBREAKER) {
		char* mcSig = nullptr;
		if (jvmti->GetClassSignature((jclass)sPinnedMc, &mcSig, nullptr) == JVMTI_ERROR_NONE && mcSig) {
			std::string mcName = stripSig(mcSig);
			jvmti->Deallocate((unsigned char*)mcSig);
			if (!mcName.empty()) {
				Mapper::Set("net/minecraft/client/Minecraft", mcName.c_str());
				m_CachedKlass[mcName] = (Klass*)sPinnedMc;
				m_CachedKlass["net/minecraft/client/Minecraft"] = (Klass*)sPinnedMc;
				m_CachedKlass["net.minecraft.client.Minecraft"] = (Klass*)sPinnedMc;
				char b[128]; sprintf_s(b, "live MC class: %s", mcName.c_str());
				EarlyLog(b);
			}

			jclass* liveArr = nullptr;
			jint liveCnt = 0;
			std::unordered_map<std::string, jclass> liveMap;
			if (jvmti->GetLoadedClasses(&liveCnt, &liveArr) == JVMTI_ERROR_NONE && liveArr) {
				for (jint i = 0; i < liveCnt; i++) {
					char* s = nullptr;
					if (jvmti->GetClassSignature(liveArr[i], &s, nullptr) != JVMTI_ERROR_NONE || !s) continue;
					std::string n = s;
					jvmti->Deallocate((unsigned char*)s);
					if (n.size() >= 3 && n.front() == 'L' && n.back() == ';')
						n = n.substr(1, n.size() - 2);
					liveMap[n] = liveArr[i];
				}
			}
			auto findLive = [&](const char* name) -> jclass {
				if (!name || !name[0]) return nullptr;
				auto it = liveMap.find(name);
				if (it == liveMap.end())
					return FindClassOnThreads(m_Env, name);
				return m_Env ? (jclass)m_Env->NewLocalRef(it->second) : it->second;
			};

			jint nf = 0; jfieldID* fids = nullptr;
			if (jvmti->GetClassFields((jclass)sPinnedMc, &nf, &fids) == JVMTI_ERROR_NONE && fids) {
				auto classLooksLikeInv = [&](jclass ic) -> bool {
					if (!ic) return false;
					int nArr = 0, nInt = 0;
					std::string elem;
					jint jn = 0; jfieldID* jids = nullptr;
					if (jvmti->GetClassFields(ic, &jn, &jids) != JVMTI_ERROR_NONE || !jids) return false;
					for (jint k = 0; k < jn; k++) {
						char* js = nullptr; jint jm = 0;
						if (jvmti->GetFieldName(ic, jids[k], nullptr, &js, nullptr) == JVMTI_ERROR_NONE && js) {
							jvmti->GetFieldModifiers(ic, jids[k], &jm);
							if ((jm & 0x0008) == 0) {
								if (strcmp(js, "I") == 0) nInt++;
								else if (js[0] == '[' && js[1] == 'L') {
									std::string e = stripSig(js + 1);
									if (elem.empty()) elem = e;
									if (e == elem) nArr++;
								}
							}
						}
						if (js) jvmti->Deallocate((unsigned char*)js);
					}
					jvmti->Deallocate((unsigned char*)jids);
					return nArr >= 2 && nInt >= 1 && !elem.empty() && elem.find('/') == std::string::npos && elem.size() >= 12;
				};

				auto typeHasInventory = [&](jclass start) -> bool {
					if (!start) return false;
					jclass walk = start;
					int depth = 0;
					bool found = false;
					while (walk && depth < 8 && !found) {
						jint inf = 0; jfieldID* ifids = nullptr;
						if (jvmti->GetClassFields(walk, &inf, &ifids) == JVMTI_ERROR_NONE && ifids) {
							for (jint g = 0; g < inf && !found; g++) {
								char* ifs = nullptr; jint ifm = 0;
								if (jvmti->GetFieldName(walk, ifids[g], nullptr, &ifs, nullptr) != JVMTI_ERROR_NONE) {
									if (ifs) jvmti->Deallocate((unsigned char*)ifs);
									continue;
								}
								jvmti->GetFieldModifiers(walk, ifids[g], &ifm);
								if ((ifm & 0x0008) == 0 && ifs && ifs[0] == 'L' && !strstr(ifs, "java/") && !strstr(ifs, "com/")) {
									std::string icn = stripSig(ifs);
									jclass ic = findLive( icn.c_str());
									if (!ic) ic = FindClassOnThreads(m_Env, icn.c_str());
									if (ic) {
										if (classLooksLikeInv(ic)) found = true;
										if (m_Env) m_Env->DeleteLocalRef(ic);
									}
								}
								if (ifs) jvmti->Deallocate((unsigned char*)ifs);
							}
							jvmti->Deallocate((unsigned char*)ifids);
						}
						jclass sup = m_Env ? m_Env->GetSuperclass(walk) : nullptr;
						if (walk != start && m_Env) m_Env->DeleteLocalRef(walk);
						walk = sup;
						depth++;
					}
					if (walk && walk != start && m_Env) m_Env->DeleteLocalRef(walk);
					return found;
				};

				auto classHasMcRef = [&](jclass jc) -> bool {
					if (!jc || mcName.empty()) return false;
					std::string want = "L" + mcName + ";";
					jint n = 0; jfieldID* ids = nullptr;
					if (jvmti->GetClassFields(jc, &n, &ids) != JVMTI_ERROR_NONE || !ids) return false;
					bool hit = false;
					for (jint i = 0; i < n && !hit; i++) {
						char* fsn = nullptr; jint fmm = 0;
						if (jvmti->GetFieldName(jc, ids[i], nullptr, &fsn, nullptr) == JVMTI_ERROR_NONE && fsn) {
							jvmti->GetFieldModifiers(jc, ids[i], &fmm);
							if ((fmm & 0x0008) == 0 && want == fsn) hit = true;
						}
						if (fsn) jvmti->Deallocate((unsigned char*)fsn);
					}
					jvmti->Deallocate((unsigned char*)ids);
					return hit;
				};

				auto classHasMcRefWalk = [&](jclass start) -> bool {
					if (!start) return false;
					jclass w = start;
					int d = 0;
					bool hit = false;
					while (w && d < 8 && !hit) {
						if (classHasMcRef(w)) hit = true;
						jclass s = m_Env ? m_Env->GetSuperclass(w) : nullptr;
						if (w != start && m_Env) m_Env->DeleteLocalRef(w);
						w = s;
						d++;
					}
					if (w && w != start && m_Env) m_Env->DeleteLocalRef(w);
					return hit;
				};

				int bestPlayer = -1, bestWorld = -1, bestTimer = -1, bestEnt = -1;
				std::string playerField, playerClass, playerWalkClass, playerFieldType, worldField, worldClass, timerField, timerClass, mcField;
				jclass entityCls = nullptr;
				for (jint f = 0; f < nf; f++) {
					char* fn = nullptr; char* fs = nullptr; jint fm = 0;
					if (jvmti->GetFieldName((jclass)sPinnedMc, fids[f], &fn, &fs, nullptr) != JVMTI_ERROR_NONE) {
						if (fn) jvmti->Deallocate((unsigned char*)fn);
						if (fs) jvmti->Deallocate((unsigned char*)fs);
						continue;
					}
					jvmti->GetFieldModifiers((jclass)sPinnedMc, fids[f], &fm);
					bool isSt = (fm & 0x0008) != 0;
					if (isSt && fs && mcName.size() && std::string(fs) == ("L" + mcName + ";")) {
						if (fn) { Mapper::Set("theMinecraft", fn); mcField = fn; }
					}
				if (!isSt && fs && fs[0] == 'L' && !strstr(fs, "java/") && !strstr(fs, "com/") && !strstr(fs, "org/") && !strstr(fs, "io/") && !strstr(fs, "$$Lambda")) {
					std::string clsName = stripSig(fs);
					jclass fc = findLive(clsName.c_str());
					if (!fc) fc = FindClassOnThreads(m_Env, clsName.c_str());
					if (fc) {
							int nF, nD, nFl, nL, nI;
							classStats(fc, nF, nD, nFl, nL, nI);
							// Entity : beaucoup de doubles/floats déclarés (pos, motion, yaw)
							if (nD >= 6 && nFl >= 4 && nF >= 40 && nF > bestEnt) {
								bestEnt = nF;
								if (entityCls && m_Env) m_Env->DeleteLocalRef(entityCls);
								entityCls = m_Env ? (jclass)m_Env->NewLocalRef(fc) : fc;
							}
							if (nL >= 2 && nD < 3 && nF > bestWorld) {
								bestWorld = nF;
								if (fn) worldField = fn;
								worldClass = clsName;
							}
						int timerScore = (nFl >= 3 && nF >= 4 && nF <= 16 && nD <= 3 && nL == 0)
							? (nFl * 10 + nD * 3 - nF) : -1;
						if (timerScore > bestTimer) {
							bestTimer = timerScore;
							if (fn) timerField = fn;
							timerClass = clsName;
						}
							if (m_Env) m_Env->DeleteLocalRef(fc);
						}
					}
					if (fn) jvmti->Deallocate((unsigned char*)fn);
					if (fs) jvmti->Deallocate((unsigned char*)fs);
				}
				jvmti->Deallocate((unsigned char*)fids);

				if (entityCls) {
					int bestSpScore = -1;
					for (const auto& kv : liveMap) {
						if (kv.first.find('/') != std::string::npos) continue;
						jclass c = findLive(kv.first.c_str());
						if (!c) continue;
						bool isEnt = m_Env && m_Env->IsAssignableFrom(c, entityCls) && !m_Env->IsSameObject(c, entityCls);
						if (m_Env && m_Env->ExceptionCheck()) m_Env->ExceptionClear();
						if (isEnt) {
							bool inv = typeHasInventory(c);
							bool mc = classHasMcRefWalk(c);
							if (inv || mc) {
								int sc = (mc ? 10000 : 0) + (inv ? 5000 : 0);
								if (sc > bestSpScore) {
									bestSpScore = sc;
									playerWalkClass = kv.first;
								}
							}
						}
						if (m_Env) m_Env->DeleteLocalRef(c);
					}

					jclass spCls = playerWalkClass.empty() ? nullptr : findLive(playerWalkClass.c_str());
					jint nf2 = 0; jfieldID* fids2 = nullptr;
					int bestDepth = -1;
					if (spCls && jvmti->GetClassFields((jclass)sPinnedMc, &nf2, &fids2) == JVMTI_ERROR_NONE && fids2) {
						for (jint f = 0; f < nf2; f++) {
							char* fn = nullptr; char* fs = nullptr; jint fm = 0;
							if (jvmti->GetFieldName((jclass)sPinnedMc, fids2[f], &fn, &fs, nullptr) != JVMTI_ERROR_NONE) {
								if (fn) jvmti->Deallocate((unsigned char*)fn);
								if (fs) jvmti->Deallocate((unsigned char*)fs);
								continue;
							}
							jvmti->GetFieldModifiers((jclass)sPinnedMc, fids2[f], &fm);
							if ((fm & 0x0008) == 0 && fs && fs[0] == 'L') {
								std::string clsName = stripSig(fs);
								jclass fc = findLive(clsName.c_str());
								if (fc) {
									bool canStore = m_Env->IsAssignableFrom(spCls, fc);
									if (m_Env->ExceptionCheck()) { m_Env->ExceptionClear(); canStore = false; }
									if (canStore) {
										int depth = 0;
										jclass t = (jclass)m_Env->NewLocalRef(fc);
										while (t && depth < 12 && !m_Env->IsSameObject(t, entityCls)) {
											jclass s = m_Env->GetSuperclass(t);
											m_Env->DeleteLocalRef(t);
											t = s;
											depth++;
										}
										if (t) m_Env->DeleteLocalRef(t);
										if (depth > bestDepth) {
											bestDepth = depth;
											if (fn) playerField = fn;
											playerFieldType = clsName;
											playerClass = clsName;
										}
									}
									m_Env->DeleteLocalRef(fc);
								}
							}
							if (fn) jvmti->Deallocate((unsigned char*)fn);
							if (fs) jvmti->Deallocate((unsigned char*)fs);
						}
						jvmti->Deallocate((unsigned char*)fids2);
					}
					if (spCls && m_Env) m_Env->DeleteLocalRef(spCls);
					{
						char* es = nullptr;
						if (jvmti->GetClassSignature(entityCls, &es, nullptr) == JVMTI_ERROR_NONE && es) {
							std::string en = stripSig(es);
							jvmti->Deallocate((unsigned char*)es);
							if (!en.empty()) Mapper::Set("net/minecraft/entity/Entity", en.c_str());
						}
					}
					if (m_Env) m_Env->DeleteLocalRef(entityCls);
					char pb[192];
					sprintf_s(pb, "player walk=%s field=%s type=%s depth=%d",
						playerWalkClass.c_str(), playerField.c_str(), playerFieldType.c_str(), bestDepth);
					EarlyLog(pb);
				}

				if (!playerField.empty()) {
					Mapper::Set("thePlayer", playerField.c_str());
					Mapper::Set("net/minecraft/client/entity/EntityPlayerSP", playerClass.c_str());
					Mapper::Set("net/minecraft/client/entity/EntityClientPlayerMP", playerClass.c_str());
				}
				if (!worldField.empty()) {
					Mapper::Set("theWorld", worldField.c_str());
					Mapper::Set("net/minecraft/client/multiplayer/WorldClient", worldClass.c_str());
				}
				if (!timerField.empty()) {
					Mapper::Set("timer", timerField.c_str());
					Mapper::Set("net/minecraft/util/Timer", timerClass.c_str());
				}
				char b[256];
				sprintf_s(b, "live map theMinecraft=%s thePlayer=%s (%s) theWorld=%s (%s) timer=%s",
					mcField.c_str(), playerField.c_str(), playerClass.c_str(),
					worldField.c_str(), worldClass.c_str(), timerField.c_str());
				EarlyLog(b);

				auto pinAlias = [&](const char* mcp, const std::string& obf) {
					if (!mcp || obf.empty()) return;
					jclass c = findLive( obf.c_str());
					if (!c) c = FindClassOnThreads(m_Env, obf.c_str());
					if (!c) return;
					jclass g = PinGlobal(m_Env, c);
					if (!g) return;
					auto* k = (Klass*)g;
					m_CachedKlass[obf] = k;
					m_CachedKlass[mcp] = k;
					std::string dots = mcp;
					for (char& ch : dots) if (ch == '/') ch = '.';
					m_CachedKlass[dots] = k;
				};

				if (!playerClass.empty()) pinAlias("net/minecraft/client/entity/EntityPlayerSP", playerClass);
				if (!playerClass.empty()) pinAlias("net/minecraft/client/entity/EntityClientPlayerMP", playerClass);
				if (!playerWalkClass.empty()) {
					pinAlias("net/minecraft/client/entity/EntityPlayerSP", playerWalkClass);
					m_CachedKlass[playerWalkClass] = m_CachedKlass["net/minecraft/client/entity/EntityPlayerSP"];
				}
				if (!worldClass.empty()) pinAlias("net/minecraft/client/multiplayer/WorldClient", worldClass);
				if (!timerClass.empty()) pinAlias("net/minecraft/util/Timer", timerClass);

				const std::string& walkName = !playerWalkClass.empty() ? playerWalkClass : playerClass;
				if (!walkName.empty()) {
					jclass pcls = findLive(walkName.c_str());
					if (!pcls) pcls = FindClassOnThreads(m_Env, walkName.c_str());
					FILE* pf = nullptr;
					fopen_s(&pf, "C:\\Users\\bipbo\\Documents\\lolxd_player_fields.txt", "w");
					if (pf) fprintf(pf, "Player class: %s (field type %s)\n", walkName.c_str(), playerFieldType.c_str());

					std::string invField, invClass, itemStackClass, mainInvField, armorInvField, currentItemField;
					std::string getStackInSlotName, getHeldItemName;
					jclass walk = pcls;
					int depth = 0;
					while (walk && depth < 8) {
						char* csig = nullptr;
						if (jvmti->GetClassSignature(walk, &csig, nullptr) == JVMTI_ERROR_NONE && csig && pf)
							fprintf(pf, "\n-- %s --\n", csig);
						if (csig) jvmti->Deallocate((unsigned char*)csig);

						jint pnf = 0; jfieldID* pfids = nullptr;
						if (jvmti->GetClassFields(walk, &pnf, &pfids) == JVMTI_ERROR_NONE && pfids) {
							if (pf) fprintf(pf, "Fields (%d):\n", pnf);
							for (jint f = 0; f < pnf; f++) {
								char* fn = nullptr; char* fs = nullptr; jint fm = 0;
								if (jvmti->GetFieldName(walk, pfids[f], &fn, &fs, nullptr) != JVMTI_ERROR_NONE) {
									if (fn) jvmti->Deallocate((unsigned char*)fn);
									if (fs) jvmti->Deallocate((unsigned char*)fs);
									continue;
								}
								jvmti->GetFieldModifiers(walk, pfids[f], &fm);
								bool isSt = (fm & 0x0008) != 0;
								if (pf) fprintf(pf, "  [%s] %s : %s\n", isSt ? "static" : "inst  ", fn ? fn : "?", fs ? fs : "?");
								if (!isSt && fs && fs[0] == 'L' && !strstr(fs, "java/") && invField.empty()) {
									std::string icn = stripSig(fs);
									jclass ic = findLive( icn.c_str());
									if (!ic) ic = FindClassOnThreads(m_Env, icn.c_str());
									if (ic) {
										if (classLooksLikeInv(ic)) {
											std::string elem;
											jint inf = 0; jfieldID* ifids = nullptr;
											if (jvmti->GetClassFields(ic, &inf, &ifids) == JVMTI_ERROR_NONE && ifids) {
												for (jint g = 0; g < inf; g++) {
													char* ifs = nullptr;
													if (jvmti->GetFieldName(ic, ifids[g], nullptr, &ifs, nullptr) == JVMTI_ERROR_NONE && ifs
														&& ifs[0] == '[' && ifs[1] == 'L') {
														std::string e = stripSig(ifs + 1);
														if (e.find('/') == std::string::npos && e.size() >= 12) { elem = e; }
													}
													if (ifs) jvmti->Deallocate((unsigned char*)ifs);
												}
												jvmti->Deallocate((unsigned char*)ifids);
											}
											if (!elem.empty()) {
											if (fn) invField = fn;
											invClass = icn;
											itemStackClass = elem;
											{
												char* wsig = nullptr;
												if (jvmti->GetClassSignature(walk, &wsig, nullptr) == JVMTI_ERROR_NONE && wsig) {
													std::string wn = stripSig(wsig);
													jvmti->Deallocate((unsigned char*)wsig);
													if (!wn.empty())
														Mapper::Set("net/minecraft/entity/player/EntityPlayer", wn.c_str());
												}
											}
											jint inf2 = 0; jfieldID* ifids2 = nullptr;
											if (jvmti->GetClassFields(ic, &inf2, &ifids2) == JVMTI_ERROR_NONE && ifids2) {
												for (jint g = 0; g < inf2; g++) {
													char* ifn = nullptr; char* ifs = nullptr; jint ifm = 0;
													if (jvmti->GetFieldName(ic, ifids2[g], &ifn, &ifs, nullptr) == JVMTI_ERROR_NONE) {
														jvmti->GetFieldModifiers(ic, ifids2[g], &ifm);
														if ((ifm & 0x0008) == 0 && ifs) {
															if (ifs[0] == '[' && ifs[1] == 'L') {
																if (mainInvField.empty() && ifn) mainInvField = ifn;
																else if (armorInvField.empty() && ifn && mainInvField != ifn) armorInvField = ifn;
															}
														}
													}
													if (ifn) jvmti->Deallocate((unsigned char*)ifn);
													if (ifs) jvmti->Deallocate((unsigned char*)ifs);
												}
												jvmti->Deallocate((unsigned char*)ifids2);
											}
											jint inf3 = 0; jfieldID* ifids3 = nullptr;
											if (jvmti->GetClassFields(ic, &inf3, &ifids3) == JVMTI_ERROR_NONE && ifids3) {
												for (jint g = 0; g < inf3; g++) {
													char* ifn = nullptr; char* ifs = nullptr; jint ifm = 0;
													if (jvmti->GetFieldName(ic, ifids3[g], &ifn, &ifs, nullptr) == JVMTI_ERROR_NONE) {
														jvmti->GetFieldModifiers(ic, ifids3[g], &ifm);
														if ((ifm & 0x0008) == 0 && ifs && strcmp(ifs, "I") == 0 && currentItemField.empty() && ifn)
															currentItemField = ifn;
													}
													if (ifn) jvmti->Deallocate((unsigned char*)ifn);
													if (ifs) jvmti->Deallocate((unsigned char*)ifs);
												}
												jvmti->Deallocate((unsigned char*)ifids3);
											}
											}
										}
										if (m_Env) m_Env->DeleteLocalRef(ic);
									}
								}
								if (fn) jvmti->Deallocate((unsigned char*)fn);
								if (fs) jvmti->Deallocate((unsigned char*)fs);
							}
							jvmti->Deallocate((unsigned char*)pfids);
						}

						jint nm = 0; jmethodID* mids = nullptr;
						if (jvmti->GetClassMethods(walk, &nm, &mids) == JVMTI_ERROR_NONE && mids) {
							if (pf) fprintf(pf, "Methods (%d):\n", nm);
							for (jint m = 0; m < nm; m++) {
								char* mn = nullptr; char* ms = nullptr; jint mm = 0;
								if (jvmti->GetMethodName(mids[m], &mn, &ms, nullptr) == JVMTI_ERROR_NONE) {
									jvmti->GetMethodModifiers(mids[m], &mm);
									if (pf) fprintf(pf, "  [%s] %s%s\n", (mm & 0x0008) ? "static" : "inst  ", mn ? mn : "?", ms ? ms : "?");
									if ((mm & 0x0008) == 0 && mn && ms && !itemStackClass.empty()) {
										std::string want = "()L" + itemStackClass + ";";
										if (want == ms && strcmp(mn, "toString") != 0 && strcmp(mn, "clone") != 0
											&& itemStackClass.find('/') == std::string::npos) {
											if (strcmp(mn, "d_") == 0)
												getHeldItemName = mn;
											else if (getHeldItemName.empty())
												getHeldItemName = mn;
										}
									}
								}
								if (mn) jvmti->Deallocate((unsigned char*)mn);
								if (ms) jvmti->Deallocate((unsigned char*)ms);
							}
							jvmti->Deallocate((unsigned char*)mids);
						}

						jclass sup = m_Env ? m_Env->GetSuperclass(walk) : nullptr;
						if (walk != pcls && m_Env) m_Env->DeleteLocalRef(walk);
						walk = sup;
						depth++;
					}

					if (!invField.empty()) Mapper::Set("inventory", invField.c_str());
					if (!invClass.empty()) {
						Mapper::Set("net/minecraft/entity/player/InventoryPlayer", invClass.c_str());
						pinAlias("net/minecraft/entity/player/InventoryPlayer", invClass);
					}
					if (!itemStackClass.empty()) {
						Mapper::Set("net/minecraft/item/ItemStack", itemStackClass.c_str());
						pinAlias("net/minecraft/item/ItemStack", itemStackClass);
					}
					if (!currentItemField.empty()) Mapper::Set("currentItem", currentItemField.c_str());
					if (!mainInvField.empty()) Mapper::Set("mainInventory", mainInvField.c_str());
					if (!armorInvField.empty()) Mapper::Set("armorInventory", armorInvField.c_str());
					if (!getHeldItemName.empty()) Mapper::Set("getHeldItem", getHeldItemName.c_str());

					if (!invClass.empty() && !itemStackClass.empty()) {
						jclass ic = findLive( invClass.c_str());
						if (ic) {
							jint nm = 0; jmethodID* mids = nullptr;
							std::string want = "(I)L" + itemStackClass + ";";
							if (jvmti->GetClassMethods(ic, &nm, &mids) == JVMTI_ERROR_NONE && mids) {
								for (jint m = 0; m < nm; m++) {
									char* mn = nullptr; char* ms = nullptr; jint mm = 0;
									if (jvmti->GetMethodName(mids[m], &mn, &ms, nullptr) == JVMTI_ERROR_NONE) {
										jvmti->GetMethodModifiers(mids[m], &mm);
										if ((mm & 0x0008) == 0 && mn && ms && want == ms) {
											getStackInSlotName = mn;
											if (mn) jvmti->Deallocate((unsigned char*)mn);
											if (ms) jvmti->Deallocate((unsigned char*)ms);
											break;
										}
									}
									if (mn) jvmti->Deallocate((unsigned char*)mn);
									if (ms) jvmti->Deallocate((unsigned char*)ms);
								}
								jvmti->Deallocate((unsigned char*)mids);
							}
							if (m_Env) m_Env->DeleteLocalRef(ic);
						}
					}
					if (!getStackInSlotName.empty()) Mapper::Set("getStackInSlot", getStackInSlotName.c_str());

					std::string itemClass, itemField, getItemName, getIdName, swordClass, axeClass;
					if (!itemStackClass.empty()) {
						jclass sc = findLive( itemStackClass.c_str());
						if (!sc) sc = FindClassOnThreads(m_Env, itemStackClass.c_str());
						if (sc) {
							if (pf) {
								char* ssig = nullptr;
								jvmti->GetClassSignature(sc, &ssig, nullptr);
								fprintf(pf, "\n== ItemStack %s ==\n", ssig ? ssig : "?");
								if (ssig) jvmti->Deallocate((unsigned char*)ssig);
							}
							jint snf = 0; jfieldID* sfids = nullptr;
							if (jvmti->GetClassFields(sc, &snf, &sfids) == JVMTI_ERROR_NONE && sfids) {
								for (jint f = 0; f < snf; f++) {
									char* fn = nullptr; char* fs = nullptr; jint fm = 0;
									if (jvmti->GetFieldName(sc, sfids[f], &fn, &fs, nullptr) == JVMTI_ERROR_NONE) {
										jvmti->GetFieldModifiers(sc, sfids[f], &fm);
										if (pf) fprintf(pf, "  [%s] %s : %s\n", (fm & 0x0008) ? "static" : "inst  ", fn ? fn : "?", fs ? fs : "?");
										if ((fm & 0x0008) == 0 && fs && fs[0] == 'L' && !strstr(fs, "java/") && itemField.empty()) {
											std::string cn = stripSig(fs);
											jclass tc = findLive( cn.c_str());
											if (!tc) tc = FindClassOnThreads(m_Env, cn.c_str());
											if (tc) {
												jint tnm = 0; jmethodID* tmids = nullptr;
												std::string idSig = "(L" + cn + ";)I";
												if (jvmti->GetClassMethods(tc, &tnm, &tmids) == JVMTI_ERROR_NONE && tmids) {
													for (jint m = 0; m < tnm; m++) {
														char* mn = nullptr; char* ms = nullptr; jint mm = 0;
														if (jvmti->GetMethodName(tmids[m], &mn, &ms, nullptr) == JVMTI_ERROR_NONE) {
															jvmti->GetMethodModifiers(tmids[m], &mm);
															if ((mm & 0x0008) != 0 && mn && ms && idSig == ms) {
																itemClass = cn;
																if (fn) itemField = fn;
																getIdName = mn;
															}
														}
														if (mn) jvmti->Deallocate((unsigned char*)mn);
														if (ms) jvmti->Deallocate((unsigned char*)ms);
													}
													jvmti->Deallocate((unsigned char*)tmids);
												}
												if (m_Env) m_Env->DeleteLocalRef(tc);
											}
										}
									}
									if (fn) jvmti->Deallocate((unsigned char*)fn);
									if (fs) jvmti->Deallocate((unsigned char*)fs);
								}
								jvmti->Deallocate((unsigned char*)sfids);
							}
							if (!itemClass.empty() && !itemField.empty()) {
								std::string stackSizeField, damageField;
								bool seenItem = false;
								jint snf2 = 0; jfieldID* sfids2 = nullptr;
								if (jvmti->GetClassFields(sc, &snf2, &sfids2) == JVMTI_ERROR_NONE && sfids2) {
									for (jint f = 0; f < snf2; f++) {
										char* fn = nullptr; char* fs = nullptr; jint fm = 0;
										if (jvmti->GetFieldName(sc, sfids2[f], &fn, &fs, nullptr) == JVMTI_ERROR_NONE) {
											jvmti->GetFieldModifiers(sc, sfids2[f], &fm);
											if ((fm & 0x0008) == 0 && fs) {
												if (fs[0] == 'L' && stripSig(fs) == itemClass) seenItem = true;
												if (strcmp(fs, "I") == 0 && fn) {
													if (stackSizeField.empty()) stackSizeField = fn;
													else if (seenItem && damageField.empty()) damageField = fn;
												}
											}
										}
										if (fn) jvmti->Deallocate((unsigned char*)fn);
										if (fs) jvmti->Deallocate((unsigned char*)fs);
									}
									jvmti->Deallocate((unsigned char*)sfids2);
								}
								if (!stackSizeField.empty()) Mapper::Set("stackSize", stackSizeField.c_str());
								if (!damageField.empty()) {
									Mapper::Set("itemDamage", damageField.c_str());
									Mapper::Set("metadata", damageField.c_str());
								}
							}
							if (!itemClass.empty()) {
								jint tnm = 0; jmethodID* tmids = nullptr;
								std::string wantGet = "()L" + itemClass + ";";
								if (jvmti->GetClassMethods(sc, &tnm, &tmids) == JVMTI_ERROR_NONE && tmids) {
									for (jint m = 0; m < tnm; m++) {
										char* mn = nullptr; char* ms = nullptr; jint mm = 0;
										if (jvmti->GetMethodName(tmids[m], &mn, &ms, nullptr) == JVMTI_ERROR_NONE) {
											jvmti->GetMethodModifiers(tmids[m], &mm);
											if ((mm & 0x0008) == 0 && mn && ms && wantGet == ms && getItemName.empty())
												getItemName = mn;
										}
										if (mn) jvmti->Deallocate((unsigned char*)mn);
										if (ms) jvmti->Deallocate((unsigned char*)ms);
									}
									jvmti->Deallocate((unsigned char*)tmids);
								}
							}
							if (m_Env) m_Env->DeleteLocalRef(sc);
						}
					}

					if (!itemClass.empty()) {
						Mapper::Set("net/minecraft/item/Item", itemClass.c_str());
						pinAlias("net/minecraft/item/Item", itemClass);
					}
					if (!itemField.empty()) Mapper::Set("item", itemField.c_str());
					if (!getItemName.empty()) Mapper::Set("getItem", getItemName.c_str());
					if (!getIdName.empty()) Mapper::Set("getIdFromItem", getIdName.c_str());

					if (!itemClass.empty()) {
						jclass itemJc = findLive( itemClass.c_str());
						if (!itemJc) itemJc = FindClassOnThreads(m_Env, itemClass.c_str());
						if (itemJc) {
							jclass itemTool = nullptr;
							int bestSword = 999, bestAxe = 999;
							jint cnt = 0; jclass* all = nullptr;
							if (jvmti->GetLoadedClasses(&cnt, &all) == JVMTI_ERROR_NONE && all) {
								for (jint i = 0; i < cnt; i++) {
									char* sig = nullptr;
									if (jvmti->GetClassSignature(all[i], &sig, nullptr) != JVMTI_ERROR_NONE || !sig) continue;
									std::string nm = sig; jvmti->Deallocate((unsigned char*)sig);
									if (nm.size() < 5 || nm.front() != 'L' || nm.back() != ';') continue;
									nm = nm.substr(1, nm.size() - 2);
									if (nm.find('/') != std::string::npos) continue;
									if (!m_Env || !m_Env->IsAssignableFrom(all[i], itemJc)) {
										if (m_Env && m_Env->ExceptionCheck()) m_Env->ExceptionClear();
										continue;
									}
									jclass sup = m_Env->GetSuperclass(all[i]);
									if (!sup) continue;
									int nF, nD, nFl, nL, nI;
									classStats(all[i], nF, nD, nFl, nL, nI);
									if (m_Env->IsSameObject(sup, itemJc)) {
										if (nFl >= 2 && nF >= 4 && nF <= 20) {
											itemTool = (jclass)m_Env->NewLocalRef(all[i]);
										}
										if (nFl == 1 && nD == 0 && nF >= 2 && nF <= 8 && nF < bestSword) {
											bestSword = nF;
											swordClass = nm;
										}
									}
									m_Env->DeleteLocalRef(sup);
								}
								if (itemTool) {
									for (jint i = 0; i < cnt; i++) {
										if (!m_Env->IsAssignableFrom(all[i], itemTool)) {
											if (m_Env->ExceptionCheck()) m_Env->ExceptionClear();
											continue;
										}
										jclass sup = m_Env->GetSuperclass(all[i]);
										if (sup && m_Env->IsSameObject(sup, itemTool)) {
											char* sig = nullptr;
											if (jvmti->GetClassSignature(all[i], &sig, nullptr) == JVMTI_ERROR_NONE && sig) {
												std::string nm = sig; jvmti->Deallocate((unsigned char*)sig);
												if (nm.size() > 3 && nm.front() == 'L' && nm.back() == ';')
													nm = nm.substr(1, nm.size() - 2);
												int nF, nD, nFl, nL, nI;
												classStats(all[i], nF, nD, nFl, nL, nI);
												if (nF < bestAxe && nF >= 1 && nF <= 12) {
													bestAxe = nF;
													axeClass = nm;
												}
											}
										}
										if (sup) m_Env->DeleteLocalRef(sup);
									}
									m_Env->DeleteLocalRef(itemTool);
								}
								jvmti->Deallocate((unsigned char*)all);
							}
							if (m_Env) m_Env->DeleteLocalRef(itemJc);
						}
					}
					if (!swordClass.empty()) {
						Mapper::Set("net/minecraft/item/ItemSword", swordClass.c_str());
						pinAlias("net/minecraft/item/ItemSword", swordClass);
					}
					if (!axeClass.empty()) {
						Mapper::Set("net/minecraft/item/ItemAxe", axeClass.c_str());
						pinAlias("net/minecraft/item/ItemAxe", axeClass);
					}

					if (!itemStackClass.empty()) {
						std::string tail = ")" + std::string("L") + itemStackClass + ";";
						jint npc = 0; jfieldID* fpc = nullptr;
						if (jvmti->GetClassFields((jclass)sPinnedMc, &npc, &fpc) == JVMTI_ERROR_NONE && fpc) {
							for (jint f = 0; f < npc; f++) {
								char* fn = nullptr; char* fs = nullptr; jint fm = 0;
								if (jvmti->GetFieldName((jclass)sPinnedMc, fpc[f], &fn, &fs, nullptr) != JVMTI_ERROR_NONE) {
									if (fn) jvmti->Deallocate((unsigned char*)fn);
									if (fs) jvmti->Deallocate((unsigned char*)fs);
									continue;
								}
								jvmti->GetFieldModifiers((jclass)sPinnedMc, fpc[f], &fm);
								if ((fm & 0x0008) == 0 && fs && fs[0] == 'L') {
									std::string cn = stripSig(fs);
									jclass pc = findLive(cn.c_str());
									if (pc) {
										jint nm = 0; jmethodID* mids = nullptr;
										if (jvmti->GetClassMethods(pc, &nm, &mids) == JVMTI_ERROR_NONE && mids) {
											for (jint m = 0; m < nm; m++) {
												char* mn = nullptr; char* ms = nullptr;
												if (jvmti->GetMethodName(mids[m], &mn, &ms, nullptr) == JVMTI_ERROR_NONE && mn && ms) {
													std::string s = ms;
													bool okSig = s.size() > 10 && s.compare(0, 6, "(IIIIL") == 0
														&& s.find(tail) != std::string::npos;
													if (okSig) {
														std::string ep = Mapper::Get("net/minecraft/entity/player/EntityPlayer");
														bool prefer = ep.empty() || s.find(ep) != std::string::npos;
														if (prefer || Mapper::Get("windowClick").empty()) {
															if (fn) Mapper::Set("playerController", fn);
															Mapper::Set("net/minecraft/client/multiplayer/PlayerControllerMP", cn.c_str());
															Mapper::Set("windowClick", mn);
															pinAlias("net/minecraft/client/multiplayer/PlayerControllerMP", cn);
														}
													}
												}
												if (mn) jvmti->Deallocate((unsigned char*)mn);
												if (ms) jvmti->Deallocate((unsigned char*)ms);
											}
											jvmti->Deallocate((unsigned char*)mids);
										}
										if (m_Env) m_Env->DeleteLocalRef(pc);
									}
								}
								if (fn) jvmti->Deallocate((unsigned char*)fn);
								if (fs) jvmti->Deallocate((unsigned char*)fs);
							}
							jvmti->Deallocate((unsigned char*)fpc);
						}
					}

					{
						FILE* invf = nullptr;
						fopen_s(&invf, "C:\\Users\\bipbo\\Documents\\lolxd_inv.txt", "w");
						if (invf) {
							auto dumpCls = [&](const char* title, const std::string& name) {
								fprintf(invf, "== %s %s ==\n", title, name.c_str());
								if (name.empty()) return;
								jclass c = findLive(name.c_str());
								if (!c) { fprintf(invf, "(not loaded)\n\n"); return; }
								jint nf = 0; jfieldID* fids = nullptr;
								if (jvmti->GetClassFields(c, &nf, &fids) == JVMTI_ERROR_NONE && fids) {
									fprintf(invf, "Fields (%d):\n", nf);
									for (jint i = 0; i < nf; i++) {
										char* fn = nullptr; char* fs = nullptr; jint fm = 0;
										if (jvmti->GetFieldName(c, fids[i], &fn, &fs, nullptr) == JVMTI_ERROR_NONE) {
											jvmti->GetFieldModifiers(c, fids[i], &fm);
											fprintf(invf, "  [%s] %s : %s\n", (fm & 0x0008) ? "static" : "inst  ", fn ? fn : "?", fs ? fs : "?");
										}
										if (fn) jvmti->Deallocate((unsigned char*)fn);
										if (fs) jvmti->Deallocate((unsigned char*)fs);
									}
									jvmti->Deallocate((unsigned char*)fids);
								}
								jint nm = 0; jmethodID* mids = nullptr;
								if (jvmti->GetClassMethods(c, &nm, &mids) == JVMTI_ERROR_NONE && mids) {
									fprintf(invf, "Methods (%d):\n", nm);
									for (jint i = 0; i < nm; i++) {
										char* mn = nullptr; char* ms = nullptr;
										if (jvmti->GetMethodName(mids[i], &mn, &ms, nullptr) == JVMTI_ERROR_NONE)
											fprintf(invf, "  %s%s\n", mn ? mn : "?", ms ? ms : "?");
										if (mn) jvmti->Deallocate((unsigned char*)mn);
										if (ms) jvmti->Deallocate((unsigned char*)ms);
									}
									jvmti->Deallocate((unsigned char*)mids);
								}
								fprintf(invf, "\n");
								if (m_Env) m_Env->DeleteLocalRef(c);
							};
							dumpCls("InventoryPlayer", invClass);
							dumpCls("PlayerControllerMP", Mapper::Get("net/minecraft/client/multiplayer/PlayerControllerMP"));
							fclose(invf);
						}
					}

					if (pf) fclose(pf);
					char ib[512];
					sprintf_s(ib, "live items inv=%s (%s) stack=%s item=%s itemField=%s getItem=%s getHeld=%s sword=%s axe=%s id=%s slot=%s pc=%s click=%s",
						invField.c_str(), invClass.c_str(), itemStackClass.c_str(), itemClass.c_str(),
						itemField.c_str(), getItemName.c_str(), getHeldItemName.c_str(),
						swordClass.c_str(), axeClass.c_str(), getIdName.c_str(), getStackInSlotName.c_str(),
						Mapper::Get("playerController").c_str(), Mapper::Get("windowClick").c_str());
					EarlyLog(ib);
				}
			}
			if (liveArr) jvmti->Deallocate((unsigned char*)liveArr);
		}
	}

	// ── Injecter les entrées Minecraft/ARI dans le cache ─────────────────
	auto alias = [&](jclass jc, std::initializer_list<const char*> keys) {
		if (!jc) return;
		auto* k = (Klass*)jc;
		for (const char* key : keys) m_CachedKlass[key] = k;
	};
	if (sPinnedMc) {
		alias(sPinnedMc, {
			"net/minecraft/client/Minecraft",
			"net.minecraft.client.Minecraft",
			"IlIlIIlIIllllIlllIIlllIlI",
			"IllIlllIlIlllIlIIIlIllIlI"
		});
	}
	if (sPinnedAri) {
		alias(sPinnedAri, {
			"net/minecraft/client/renderer/ActiveRenderInfo",
			"net.minecraft.client.renderer.ActiveRenderInfo",
			"IlIlllllIIllIlIlllIIlllll",
			"IllllIlIlIIllIIlIIlllllIl"
		});
	}

	// ── Vérification finale ───────────────────────────────────────────────
	{
		char buf2[256];
		sprintf_s(buf2, "final check: sPinnedMc=%p sPinnedAri=%p launcher=%d ver=%d",
			(void*)sPinnedMc, (void*)sPinnedAri, (int)g_GameLauncher, (int)g_GameVersion);
		EarlyLog(buf2);
	}
	const bool mcOk = (FindClass(Mapper::Get("net/minecraft/client/Minecraft")) != nullptr) || (sPinnedMc != nullptr);
	EarlyLog(mcOk ? "mcOk=true -> SUCCESS" : "mcOk=false -> ERR");
	if (!mcOk) {
		m_InitializationState = ERR;
		return;
	}

	// Dump des champs de la classe Minecraft trouvée pour identifier les mappings réels
	if (sPinnedMc && jvmti) {
		FILE* fd = nullptr;
		fopen_s(&fd, "C:\\Users\\bipbo\\Documents\\lolxd_mc_fields.txt", "w");
		if (fd) {
			char* mcSig = nullptr;
			jvmti->GetClassSignature((jclass)sPinnedMc, &mcSig, nullptr);
			fprintf(fd, "Minecraft class: %s\n\n", mcSig ? mcSig : "?");
			if (mcSig) jvmti->Deallocate((unsigned char*)mcSig);

			jint nf = 0; jfieldID* fids = nullptr;
			if (jvmti->GetClassFields((jclass)sPinnedMc, &nf, &fids) == JVMTI_ERROR_NONE && fids) {
				fprintf(fd, "Fields (%d):\n", nf);
				for (jint f = 0; f < nf; f++) {
					char* fn = nullptr; char* fs = nullptr; jint fm = 0;
					if (jvmti->GetFieldName((jclass)sPinnedMc, fids[f], &fn, &fs, nullptr) == JVMTI_ERROR_NONE) {
						jvmti->GetFieldModifiers((jclass)sPinnedMc, fids[f], &fm);
						bool isStatic = (fm & 0x0008) != 0;
						fprintf(fd, "  [%s] %s : %s\n", isStatic ? "static" : "inst  ", fn ? fn : "?", fs ? fs : "?");
					}
					if (fn) jvmti->Deallocate((unsigned char*)fn);
					if (fs) jvmti->Deallocate((unsigned char*)fs);
				}
				jvmti->Deallocate((unsigned char*)fids);
			}

			jint nm = 0; jmethodID* mids = nullptr;
			if (jvmti->GetClassMethods((jclass)sPinnedMc, &nm, &mids) == JVMTI_ERROR_NONE && mids) {
				fprintf(fd, "\nMethods (%d):\n", nm);
				for (jint m = 0; m < nm; m++) {
					char* mn = nullptr; char* ms = nullptr; jint mm = 0;
					if (jvmti->GetMethodName(mids[m], &mn, &ms, nullptr) == JVMTI_ERROR_NONE) {
						jvmti->GetMethodModifiers(mids[m], &mm);
						bool isStatic = (mm & 0x0008) != 0;
						fprintf(fd, "  [%s] %s%s\n", isStatic ? "static" : "inst  ", mn ? mn : "?", ms ? ms : "?");
					}
					if (mn) jvmti->Deallocate((unsigned char*)mn);
					if (ms) jvmti->Deallocate((unsigned char*)ms);
				}
				jvmti->Deallocate((unsigned char*)mids);
			}
			fclose(fd);
		}
		EarlyLog("mc fields dumped to lolxd_mc_fields.txt");
	}

	m_InitializationState = SUCCESS;
}

Klass* Hack::FindClass(std::string klassName) const
{
	if (klassName.empty()) return nullptr;
	std::string dots = klassName, slashes = klassName;
	std::ranges::replace(dots.begin(),    dots.end(),    '/', '.');
	std::ranges::replace(slashes.begin(), slashes.end(), '.', '/');
	if (m_CachedKlass.contains(dots))    return m_CachedKlass.at(dots);
	if (m_CachedKlass.contains(slashes)) return m_CachedKlass.at(slashes);
	if (m_CachedKlass.contains(klassName)) return m_CachedKlass.at(klassName);
	return nullptr;
}

std::unique_ptr<Hack> g_Instance;
HMODULE g_hModule = nullptr;
