#include "pch.h"
#include "Minecraft.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"
#include "../../Cheat/Modules/Settings.h"
#include "Player.h"
#include <jvmti.h>
#include <string>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

// â”€â”€ Cache statique des Field* resolus une seule fois â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Evite de refaire FindClass + GetField a chaque appel (1000x/sec par module)

struct MinecraftFieldCache {
    Klass* clsMC = nullptr;
    Field* fTheMinecraft = nullptr;
    Field* fThePlayer = nullptr;
    Field* fTheWorld = nullptr;
    Field* fCurrentScreen = nullptr;
    Field* fObjectMO = nullptr;
    Field* fPointedEntity = nullptr;
    Field* fGameSettings = nullptr;
    Field* fTimer = nullptr;
    Field* fFontRenderer = nullptr;
    Field* fDisplayWidth = nullptr;
    Field* fDisplayHeight = nullptr;
    Field* fRightClickDelay = nullptr;
    Field* fRenderManager = nullptr;
    bool    renderMgrTried = false;
    bool    built = false;
} static s_mc;

struct InvGuiCache {
    bool ready = false;
    jclass guiInv = nullptr;
    jclass guiScreen = nullptr;
    jmethodID ctor = nullptr;
    jmethodID display = nullptr;
    jfieldID screenFields[4]{};
    int screenFieldCount = 0;
    jobject invKey = nullptr;
} static s_invGui;
static std::mutex s_invGuiMu;

static void GuiLog(const char* fmt, ...) {
    FILE* f = nullptr;
    fopen_s(&f, "C:\\Users\\bipbo\\Documents\\lolxd_refill.txt", "a");
    if (!f) return;
    fputs("[gui] ", f);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static jobject GetMCInstance(JNIEnv* env);
static bool EnsureInvGui(JNIEnv* env);

static void BuildMCCache(JNIEnv* env) {
    if (s_mc.fTheMinecraft && s_mc.fThePlayer && s_mc.fTheWorld) return;
    s_mc.built = true;

    s_mc.clsMC = g_Instance->FindClass(Mapper::Get("net/minecraft/client/Minecraft"));
    if (!s_mc.clsMC) return;

    auto GF = [&](const char* key, const char* sigKey, int sigType = 2) -> Field* {
        std::string name = Mapper::Get(key);
        std::string sig = Mapper::Get(sigKey, sigType);
        if (name.empty() || sig.empty()) return nullptr;
        Field* f = s_mc.clsMC->GetField(env, name.c_str(), sig.c_str(), false);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
        return f;
        };
    auto GFS = [&](const char* key, const char* sigKey, int sigType = 2) -> Field* {
        std::string name = Mapper::Get(key);
        std::string sig = Mapper::Get(sigKey, sigType);
        if (name.empty() || sig.empty()) return nullptr;
        Field* f = s_mc.clsMC->GetField(env, name.c_str(), sig.c_str(), true);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
        return f;
        };
    auto GFI = [&](const char* key) -> Field* {
        std::string name = Mapper::Get(key);
        if (name.empty()) return nullptr;
        Field* f = s_mc.clsMC->GetField(env, name.c_str(), "I", false);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
        return f;
        };

    s_mc.fTheMinecraft = GFS("theMinecraft", "net/minecraft/client/Minecraft");

    auto tryPlayerSig = [&](const char* rawClass) -> Field* {
        std::string name = Mapper::Get("thePlayer");
        if (name.empty() || !rawClass) return nullptr;
        std::string sig = std::string("L") + rawClass + ";";
        Field* f = s_mc.clsMC->GetField(env, name.c_str(), sig.c_str(), false);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
        return f;
    };
    s_mc.fThePlayer = tryPlayerSig("net/minecraft/client/entity/EntityClientPlayerMP");
    if (!s_mc.fThePlayer)
        s_mc.fThePlayer = tryPlayerSig("net/minecraft/client/entity/EntityPlayerSP");
    if (!s_mc.fThePlayer)
        s_mc.fThePlayer = GF("thePlayer", "net/minecraft/client/entity/EntityClientPlayerMP");
    if (!s_mc.fThePlayer)
        s_mc.fThePlayer = GF("thePlayer", "net/minecraft/client/entity/EntityPlayerSP");
    if (!s_mc.fThePlayer)
        s_mc.fThePlayer = GF("thePlayer", "net/minecraft/entity/player/EntityPlayer");
    if (!s_mc.fThePlayer && Mapper::IsCheatBreaker())
        s_mc.fThePlayer = GF("thePlayer", "net/minecraft/entity/Entity");
    s_mc.fTheWorld = GF("theWorld", "net/minecraft/client/multiplayer/WorldClient");
    if (!s_mc.fTheWorld) {
        std::string name = Mapper::Get("theWorld");
        Field* f = s_mc.clsMC->GetField(env, name.c_str(), "Lnet/minecraft/world/World;", false);
        if (env->ExceptionCheck()) { env->ExceptionClear(); f = nullptr; }
        s_mc.fTheWorld = f;
    }
    s_mc.fCurrentScreen = GF("currentScreen", "net/minecraft/client/gui/GuiScreen");
    s_mc.fObjectMO = GF("objectMouseOver", "net/minecraft/util/MovingObjectPosition");
    s_mc.fPointedEntity = GF("pointedEntity", "net/minecraft/entity/Entity");
    s_mc.fGameSettings = GF("gameSettings", "net/minecraft/client/settings/GameSettings");
    s_mc.fTimer = GF("timer", "net/minecraft/util/Timer");
    s_mc.fFontRenderer = GF("fontRendererObj", "net/minecraft/client/gui/FontRenderer");
    if (!s_mc.fFontRenderer) {
        std::string frSig = Mapper::Get("net/minecraft/client/gui/FontRenderer", 2);
        Field* f = s_mc.clsMC->GetField(env, "fontRenderer", frSig.empty() ? "Lnet/minecraft/client/gui/FontRenderer;" : frSig.c_str(), false);
        if (env->ExceptionCheck()) { env->ExceptionClear(); f = nullptr; }
        s_mc.fFontRenderer = f;
    }
    s_mc.fDisplayWidth = GFI("displayWidth");
    s_mc.fDisplayHeight = GFI("displayHeight");
    s_mc.fRightClickDelay = GFI("rightClickDelayTimer");
    if (!s_mc.renderMgrTried) {
        s_mc.renderMgrTried = true;
        s_mc.fRenderManager = GF("renderManager", "net/minecraft/client/renderer/entity/RenderManager");
    }
}

// Helper interne â€” retourne l'instance Minecraft (champ statique)
static jobject GetMCInstance(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fTheMinecraft || !s_mc.clsMC) return nullptr;
    return s_mc.fTheMinecraft->GetObjectField(env, s_mc.clsMC, true);
}

jobject Minecraft::GetTheMinecraft(JNIEnv* env) { return GetMCInstance(env); }

jobject Minecraft::GetThePlayer(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fThePlayer) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fThePlayer->GetObjectField(env, mc, false);
}

jobject Minecraft::GetTheWorld(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fTheWorld) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fTheWorld->GetObjectField(env, mc, false);
}

jobject Minecraft::GetCurrentScreen(JNIEnv* env) {
    BuildMCCache(env);
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    if (s_invGui.ready && s_invGui.screenFieldCount > 0) {
        for (int i = 0; i < s_invGui.screenFieldCount; i++) {
            jobject s = env->GetObjectField(mc, s_invGui.screenFields[i]);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (s) {
                env->DeleteLocalRef(mc);
                return s;
            }
        }
        env->DeleteLocalRef(mc);
        return nullptr;
    }
    if (s_mc.fCurrentScreen) {
        jobject s = s_mc.fCurrentScreen->GetObjectField(env, mc, false);
        env->DeleteLocalRef(mc);
        return s;
    }
    env->DeleteLocalRef(mc);
    return nullptr;
}

jobject Minecraft::GetObjectMouseOver(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fObjectMO) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fObjectMO->GetObjectField(env, mc, false);
}

jobject Minecraft::GetPointedEntity(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fPointedEntity) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fPointedEntity->GetObjectField(env, mc, false);
}

void Minecraft::SetObjectMouseOver(JNIEnv* env, jobject mop) {
    BuildMCCache(env);
    if (!s_mc.fObjectMO) return;
    jobject mc = GetMCInstance(env);
    if (!mc) return;
    s_mc.fObjectMO->SetObjectField(env, mc, mop, false);
}

void Minecraft::SetPointedEntity(JNIEnv* env, jobject entity) {
    BuildMCCache(env);
    if (!s_mc.fPointedEntity) return;
    jobject mc = GetMCInstance(env);
    if (!mc) return;
    s_mc.fPointedEntity->SetObjectField(env, mc, entity, false);
}

jobject Minecraft::GetGameSettings(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fGameSettings) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fGameSettings->GetObjectField(env, mc, false);
}

jobject Minecraft::GetTimer(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fTimer) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fTimer->GetObjectField(env, mc, false);
}

jobject Minecraft::GetFontRenderer(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fFontRenderer) return nullptr;
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    return s_mc.fFontRenderer->GetObjectField(env, mc, false);
}

int Minecraft::GetDisplayWidth(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fDisplayWidth) return 0;
    jobject mc = GetMCInstance(env);
    if (!mc) return 0;
    return s_mc.fDisplayWidth->GetIntField(env, mc);
}

int Minecraft::GetDisplayHeight(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fDisplayHeight) return 0;
    jobject mc = GetMCInstance(env);
    if (!mc) return 0;
    return s_mc.fDisplayHeight->GetIntField(env, mc);
}

bool Minecraft::IsFullscreen(JNIEnv* env) {
    jobject mc = GetMCInstance(env);
    if (!mc) return false;
    Klass* cls = (Klass*)env->GetObjectClass(mc);
    if (!cls) return false;
    Field* f = cls->GetField(env, Mapper::Get("fullscreen").c_str(), "Z");
    env->DeleteLocalRef((jclass)cls);
    if (!f) return false;
    return f->GetBooleanField(env, mc);
}

jobject Minecraft::GetPlayerController(JNIEnv* env) {
    jobject mc = GetMCInstance(env);
    if (!mc) return nullptr;
    Klass* cls = (Klass*)env->GetObjectClass(mc);
    if (!cls) return nullptr;
    Field* f = cls->GetField(env, Mapper::Get("playerController").c_str(),
        Mapper::Get("net/minecraft/client/multiplayer/PlayerControllerMP", 2).c_str());
    if (env->ExceptionCheck()) { env->ExceptionClear(); f = nullptr; }
    jobject ctrl = nullptr;
    if (f) ctrl = f->GetObjectField(env, mc);
    if (env->ExceptionCheck()) { env->ExceptionClear(); ctrl = nullptr; }
    if (ctrl) {
        env->DeleteLocalRef((jclass)cls);
        return ctrl;
    }

    JavaVM* vm = nullptr;
    jvmtiEnv* jvmti = nullptr;
    if (env->GetJavaVM(&vm) == 0 && vm)
        vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2);
    if (jvmti) {
        jclass cur = (jclass)cls;
        for (int depth = 0; cur && depth < 6; depth++) {
            jint nf = 0; jfieldID* fids = nullptr;
            if (jvmti->GetClassFields(cur, &nf, &fids) == JVMTI_ERROR_NONE && fids) {
                for (jint i = 0; i < nf && !ctrl; i++) {
                    char* fs = nullptr; jint fm = 0;
                    if (jvmti->GetFieldName(cur, fids[i], nullptr, &fs, nullptr) != JVMTI_ERROR_NONE) continue;
                    jvmti->GetFieldModifiers(cur, fids[i], &fm);
                    bool obj = fs && fs[0] == 'L' && (fm & 0x0008) == 0;
                    if (fs) jvmti->Deallocate((unsigned char*)fs);
                    if (!obj) continue;
                    jobject cand = env->GetObjectField(mc, fids[i]);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                    if (!cand) continue;
                    jclass cc = env->GetObjectClass(cand);
                    if (!cc) { env->DeleteLocalRef(cand); continue; }
                    jint nm = 0; jmethodID* mids = nullptr;
                    bool ok = false;
                    jclass walk = cc;
                    for (int d = 0; walk && d < 6 && !ok; d++) {
                        if (jvmti->GetClassMethods(walk, &nm, &mids) == JVMTI_ERROR_NONE && mids) {
                            for (jint m = 0; m < nm; m++) {
                                char* ms = nullptr;
                                if (jvmti->GetMethodName(mids[m], nullptr, &ms, nullptr) == JVMTI_ERROR_NONE && ms) {
                                    std::string s = ms;
                                    if (s.size() > 10 && s.compare(0, 6, "(IIIIL") == 0 && s.find(")L") != std::string::npos)
                                        ok = true;
                                    jvmti->Deallocate((unsigned char*)ms);
                                }
                                if (ok) break;
                            }
                            jvmti->Deallocate((unsigned char*)mids);
                        }
                        jclass sup = env->GetSuperclass(walk);
                        if (walk != cc) env->DeleteLocalRef(walk);
                        walk = sup;
                    }
                    env->DeleteLocalRef(cc);
                    if (ok) ctrl = cand;
                    else env->DeleteLocalRef(cand);
                }
                jvmti->Deallocate((unsigned char*)fids);
            }
            jclass sup = env->GetSuperclass(cur);
            if (cur != (jclass)cls) env->DeleteLocalRef(cur);
            cur = sup;
        }
    }
    env->DeleteLocalRef((jclass)cls);
    return ctrl;
}

static jmethodID FindWindowClickMethod(JNIEnv* env, jobject controller) {
    static jmethodID cached = nullptr;
    if (cached) return cached;
    JavaVM* vm = nullptr;
    jvmtiEnv* jvmti = nullptr;
    if (env->GetJavaVM(&vm) != 0 || !vm) return nullptr;
    if (vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2) != JNI_OK || !jvmti) return nullptr;
    jclass cls = env->GetObjectClass(controller);
    if (!cls) return nullptr;
    std::string stack = Mapper::Get("net/minecraft/item/ItemStack");
    std::string player = Mapper::Get("net/minecraft/entity/player/EntityPlayer");
    jmethodID best = nullptr;
    jclass walk = cls;
    for (int d = 0; walk && d < 6; d++) {
        jint nm = 0; jmethodID* mids = nullptr;
        if (jvmti->GetClassMethods(walk, &nm, &mids) == JVMTI_ERROR_NONE && mids) {
            for (jint m = 0; m < nm; m++) {
                char* mn = nullptr; char* ms = nullptr;
                if (jvmti->GetMethodName(mids[m], &mn, &ms, nullptr) == JVMTI_ERROR_NONE && ms) {
                    std::string s = ms;
                    bool ok = s.size() > 10 && s.compare(0, 6, "(IIIIL") == 0 && s.find(")L") != std::string::npos;
                    if (ok) {
                        if (!best) best = mids[m];
                        if (!stack.empty() && s.find(stack) != std::string::npos) best = mids[m];
                        if (!player.empty() && s.find(player) != std::string::npos) {
                            best = mids[m];
                            if (mn) jvmti->Deallocate((unsigned char*)mn);
                            if (ms) jvmti->Deallocate((unsigned char*)ms);
                            jvmti->Deallocate((unsigned char*)mids);
                            if (walk != cls) env->DeleteLocalRef(walk);
                            env->DeleteLocalRef(cls);
                            cached = best;
                            return cached;
                        }
                    }
                }
                if (mn) jvmti->Deallocate((unsigned char*)mn);
                if (ms) jvmti->Deallocate((unsigned char*)ms);
            }
            jvmti->Deallocate((unsigned char*)mids);
        }
        jclass sup = env->GetSuperclass(walk);
        if (walk != cls) env->DeleteLocalRef(walk);
        walk = sup;
    }
    env->DeleteLocalRef(cls);
    cached = best;
    return cached;
}

jobject Minecraft::WindowClick(JNIEnv* env, int windowId, int slot, int mouseButton, int clickMode, jobject player) {
    jobject controller = GetPlayerController(env);
    if (!controller || !player) return nullptr;
    Klass* cls = (Klass*)env->GetObjectClass(controller);
    if (!cls) {
        env->DeleteLocalRef(controller);
        return nullptr;
    }
    jobject result = nullptr;
    bool invoked = false;
    auto trySig = [&](const char* playerKey) {
        if (invoked) return;
        std::string sig = "(IIII" + Mapper::Get(playerKey, 2) + ")"
            + Mapper::Get("net/minecraft/item/ItemStack", 2);
        Method* m = cls->GetMethod(env, Mapper::Get("windowClick").c_str(), sig.c_str());
        if (env->ExceptionCheck()) { env->ExceptionClear(); m = nullptr; }
        if (!m) return;
        invoked = true;
        result = m->CallObjectMethod(env, controller, false, windowId, slot, mouseButton, clickMode, player);
        if (env->ExceptionCheck()) { env->ExceptionClear(); result = nullptr; }
    };
    trySig("net/minecraft/entity/player/EntityPlayer");
    trySig("net/minecraft/client/entity/EntityPlayerSP");
    trySig("net/minecraft/client/entity/EntityClientPlayerMP");
    env->DeleteLocalRef((jclass)cls);
    if (!invoked) {
        jmethodID mid = FindWindowClickMethod(env, controller);
        if (mid) {
            result = env->CallObjectMethod(controller, mid, windowId, slot, mouseButton, clickMode, player);
            if (env->ExceptionCheck()) { env->ExceptionClear(); result = nullptr; }
        }
    }
    env->DeleteLocalRef(controller);
    return result;
}

static jclass FindLoaded(JNIEnv* env, jvmtiEnv* jvmti, const std::string& name) {
    if (name.empty() || !jvmti) return nullptr;
    jint n = 0; jclass* all = nullptr;
    if (jvmti->GetLoadedClasses(&n, &all) != JVMTI_ERROR_NONE || !all) return nullptr;
    jclass found = nullptr;
    std::string want = "L" + name + ";";
    for (jint i = 0; i < n; i++) {
        char* sig = nullptr;
        if (jvmti->GetClassSignature(all[i], &sig, nullptr) == JVMTI_ERROR_NONE && sig) {
            if (want == sig) found = (jclass)env->NewLocalRef(all[i]);
            jvmti->Deallocate((unsigned char*)sig);
        }
        if (found) break;
    }
    jvmti->Deallocate((unsigned char*)all);
    return found;
}

static bool EnsureInvGui(JNIEnv* env) {
    std::lock_guard<std::mutex> lock(s_invGuiMu);
    if (s_invGui.ready)
        return s_invGui.display || s_invGui.invKey || s_invGui.screenFieldCount > 0;
    // CB: GetLoadedClasses + GetObjectField on GameSettings/File crashes the JVM
    // (hs_err EnsureInvGui, java.io.File options.txt). Open via key only.
    if (Mapper::IsCheatBreaker()) {
        s_invGui.ready = true;
        GuiLog("cb skip jvmti gui scan (key only)");
        return false;
    }
    jobject mc = GetMCInstance(env);
    if (!mc) return false;
    jclass mcCls = env->GetObjectClass(mc);
    if (!mcCls) { env->DeleteLocalRef(mc); return false; }

    JavaVM* vm = nullptr;
    jvmtiEnv* jvmti = nullptr;
    if (env->GetJavaVM(&vm) != 0 || !vm || vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2) != JNI_OK || !jvmti) {
        env->DeleteLocalRef(mcCls);
        env->DeleteLocalRef(mc);
        return false;
    }

    jint nAll = 0; jclass* all = nullptr;
    if (jvmti->GetLoadedClasses(&nAll, &all) != JVMTI_ERROR_NONE || !all) {
        env->DeleteLocalRef(mcCls);
        env->DeleteLocalRef(mc);
        return false;
    }
    std::unordered_map<std::string, jclass> byName;
    for (jint i = 0; i < nAll; i++) {
        char* sig = nullptr;
        if (jvmti->GetClassSignature(all[i], &sig, nullptr) != JVMTI_ERROR_NONE || !sig) continue;
        std::string s = sig;
        jvmti->Deallocate((unsigned char*)sig);
        if (s.size() > 2 && s.front() == 'L' && s.back() == ';')
            byName[s.substr(1, s.size() - 2)] = all[i];
    }

    std::string mcName;
    {
        char* sig = nullptr;
        if (jvmti->GetClassSignature(mcCls, &sig, nullptr) == JVMTI_ERROR_NONE && sig) {
            mcName = sig;
            jvmti->Deallocate((unsigned char*)sig);
            if (mcName.size() > 2 && mcName.front() == 'L' && mcName.back() == ';')
                mcName = mcName.substr(1, mcName.size() - 2);
        }
    }

    std::unordered_map<std::string, int> mcFieldTypes;
    {
        jint nf0 = 0; jfieldID* f0 = nullptr;
        if (jvmti->GetClassFields(mcCls, &nf0, &f0) == JVMTI_ERROR_NONE && f0) {
            for (jint f = 0; f < nf0; f++) {
                char* fs = nullptr; jint fm = 0;
                if (jvmti->GetFieldName(mcCls, f0[f], nullptr, &fs, nullptr) != JVMTI_ERROR_NONE) continue;
                jvmti->GetFieldModifiers(mcCls, f0[f], &fm);
                if (fs && fs[0] == 'L' && (fm & 0x0008) == 0) {
                    std::string t = fs;
                    if (t.size() > 2 && t.back() == ';')
                        mcFieldTypes[t.substr(1, t.size() - 2)]++;
                }
                if (fs) jvmti->Deallocate((unsigned char*)fs);
            }
            jvmti->Deallocate((unsigned char*)f0);
        }
    }

    std::unordered_set<std::string> skipTypes;
    skipTypes.insert(mcName);
    auto addSkip = [&](const std::string& n) { if (!n.empty()) skipTypes.insert(n); };
    addSkip(Mapper::Get("net/minecraft/entity/Entity"));
    addSkip(Mapper::Get("net/minecraft/world/World"));
    addSkip(Mapper::Get("net/minecraft/client/multiplayer/WorldClient"));
    addSkip(Mapper::Get("net/minecraft/item/ItemStack"));
    addSkip(Mapper::Get("net/minecraft/item/Item"));
    auto addSuperOnce = [&](const std::string& n) {
        auto sit = byName.find(n);
        if (sit == byName.end()) return;
        jclass sup = env->GetSuperclass(sit->second);
        if (!sup || env->ExceptionCheck()) { env->ExceptionClear(); return; }
        char* ss = nullptr;
        if (jvmti->GetClassSignature(sup, &ss, nullptr) == JVMTI_ERROR_NONE && ss) {
            std::string sig = ss;
            jvmti->Deallocate((unsigned char*)ss);
            if (sig.size() > 2 && sig.front() == 'L' && sig.back() == ';' && sig != "Ljava/lang/Object;")
                skipTypes.insert(sig.substr(1, sig.size() - 2));
        }
        env->DeleteLocalRef(sup);
    };
    addSuperOnce(Mapper::Get("net/minecraft/client/multiplayer/WorldClient"));
    addSuperOnce(Mapper::Get("net/minecraft/entity/player/EntityPlayer"));

    struct Cand { std::string name; jmethodID display; int kids; int fields; };
    std::vector<Cand> cands;
    jint nm = 0; jmethodID* mids = nullptr;
    if (jvmti->GetClassMethods(mcCls, &nm, &mids) == JVMTI_ERROR_NONE && mids) {
        for (jint m = 0; m < nm; m++) {
            char* mn = nullptr; char* ms = nullptr;
            if (jvmti->GetMethodName(mids[m], &mn, &ms, nullptr) != JVMTI_ERROR_NONE) continue;
            std::string s = ms ? ms : "";
            if (mn) jvmti->Deallocate((unsigned char*)mn);
            if (ms) jvmti->Deallocate((unsigned char*)ms);
            if (s.size() < 5 || s.front() != '(' || s[1] != 'L' || s[s.size() - 2] != ')' || s.back() != 'V')
                continue;
            size_t sc = s.find(';');
            if (sc == std::string::npos || sc + 3 != s.size()) continue;
            std::string cn = s.substr(2, sc - 2);
            if (cn.find('/') != std::string::npos) continue;
            if (mcFieldTypes[cn] < 1) continue;
            if (skipTypes.count(cn)) continue;
            auto it = byName.find(cn);
            if (it == byName.end()) continue;
            std::string entName = Mapper::Get("net/minecraft/entity/Entity");
            auto eit = byName.find(entName);
            if (eit != byName.end() && env->IsAssignableFrom(it->second, eit->second)) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                continue;
            }
            auto w2 = byName.find(Mapper::Get("net/minecraft/world/World"));
            if (w2 != byName.end() && env->IsAssignableFrom(it->second, w2->second)) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                continue;
            }
            auto wit = byName.find(Mapper::Get("net/minecraft/client/multiplayer/WorldClient"));
            if (wit != byName.end() && env->IsAssignableFrom(it->second, wit->second)) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                continue;
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            int kids = 0;
            for (jint i = 0; i < nAll; i++) {
                if (env->IsAssignableFrom(all[i], it->second)) kids++;
                else if (env->ExceptionCheck()) env->ExceptionClear();
            }
            cands.push_back({ cn, mids[m], kids, mcFieldTypes[cn] });
        }
        jvmti->Deallocate((unsigned char*)mids);
    }

    std::string ep = Mapper::Get("net/minecraft/entity/player/EntityPlayer");
    std::string sp = Mapper::Get("net/minecraft/client/entity/EntityPlayerSP");
    std::string guiScreenName;
    int bestKids = 0;
    int bestInv = -1;
    for (auto& c : cands) {
        auto git = byName.find(c.name);
        if (git == byName.end()) continue;
        int inv = 0;
        for (jint i = 0; i < nAll; i++) {
            if (!env->IsAssignableFrom(all[i], git->second)) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                continue;
            }
            jint nme = 0; jmethodID* mm = nullptr;
            if (jvmti->GetClassMethods(all[i], &nme, &mm) != JVMTI_ERROR_NONE || !mm) continue;
            for (jint k = 0; k < nme; k++) {
                char* mn = nullptr; char* ms = nullptr;
                if (jvmti->GetMethodName(mm[k], &mn, &ms, nullptr) == JVMTI_ERROR_NONE && mn && ms
                    && strcmp(mn, "<init>") == 0) {
                    std::string sig = ms;
                    if ((!ep.empty() && sig == "(L" + ep + ";)V") || (!sp.empty() && sig == "(L" + sp + ";)V"))
                        inv++;
                }
                if (mn) jvmti->Deallocate((unsigned char*)mn);
                if (ms) jvmti->Deallocate((unsigned char*)ms);
            }
            jvmti->Deallocate((unsigned char*)mm);
        }
        if (inv > bestInv || (inv == bestInv && c.kids > bestKids)) {
            bestInv = inv;
            bestKids = c.kids;
            guiScreenName = c.name;
            s_invGui.display = c.display;
        }
    }
    if (bestInv <= 0) {
        s_invGui.display = nullptr;
        guiScreenName.clear();
    }
    GuiLog("guiScreen=%s kids=%d invKids=%d display=%p", guiScreenName.c_str(), bestKids, bestInv, (void*)s_invGui.display);

    if (!guiScreenName.empty()) {
        std::string want = "L" + guiScreenName + ";";
        jint nf = 0; jfieldID* fids = nullptr;
        if (jvmti->GetClassFields(mcCls, &nf, &fids) == JVMTI_ERROR_NONE && fids) {
            for (jint f = 0; f < nf && s_invGui.screenFieldCount < 4; f++) {
                char* fs = nullptr; jint fm = 0;
                if (jvmti->GetFieldName(mcCls, fids[f], nullptr, &fs, nullptr) != JVMTI_ERROR_NONE) continue;
                jvmti->GetFieldModifiers(mcCls, fids[f], &fm);
                if (fs && want == fs && (fm & 0x0008) == 0)
                    s_invGui.screenFields[s_invGui.screenFieldCount++] = fids[f];
                if (fs) jvmti->Deallocate((unsigned char*)fs);
            }
            jvmti->Deallocate((unsigned char*)fids);
        }
    }

    auto gsIt = byName.find(guiScreenName);
    jclass guiScreen = (gsIt != byName.end()) ? gsIt->second : nullptr;
    if (guiScreen) {
        if (s_invGui.guiScreen) env->DeleteGlobalRef(s_invGui.guiScreen);
        s_invGui.guiScreen = (jclass)env->NewGlobalRef(guiScreen);
    }
    if (guiScreen && (!ep.empty() || !sp.empty())) {
        int bestScore = -1;
        for (jint i = 0; i < nAll; i++) {
            if (!env->IsAssignableFrom(all[i], guiScreen)) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                continue;
            }
            jint mods = 0;
            jvmti->GetClassModifiers(all[i], &mods);
            if (mods & 0x0400) continue;
            jint nme = 0; jmethodID* mm = nullptr;
            jmethodID ctor = nullptr;
            if (jvmti->GetClassMethods(all[i], &nme, &mm) == JVMTI_ERROR_NONE && mm) {
                for (jint k = 0; k < nme; k++) {
                    char* mn = nullptr; char* ms = nullptr;
                    if (jvmti->GetMethodName(mm[k], &mn, &ms, nullptr) == JVMTI_ERROR_NONE && mn && ms
                        && strcmp(mn, "<init>") == 0) {
                        std::string s = ms;
                        if (!ep.empty() && s == "(L" + ep + ";)V") ctor = mm[k];
                        if (!sp.empty() && s == "(L" + sp + ";)V") ctor = mm[k];
                    }
                    if (mn) jvmti->Deallocate((unsigned char*)mn);
                    if (ms) jvmti->Deallocate((unsigned char*)ms);
                }
                jvmti->Deallocate((unsigned char*)mm);
            }
            if (!ctor) continue;
            int nFl = 0;
            jint nf2 = 0; jfieldID* f2 = nullptr;
            if (jvmti->GetClassFields(all[i], &nf2, &f2) == JVMTI_ERROR_NONE && f2) {
                for (jint f = 0; f < nf2; f++) {
                    char* fs = nullptr; jint fm = 0;
                    if (jvmti->GetFieldName(all[i], f2[f], nullptr, &fs, nullptr) == JVMTI_ERROR_NONE) {
                        jvmti->GetFieldModifiers(all[i], f2[f], &fm);
                        if ((fm & 0x0008) == 0 && fs && fs[0] == 'F') nFl++;
                    }
                    if (fs) jvmti->Deallocate((unsigned char*)fs);
                }
                jvmti->Deallocate((unsigned char*)f2);
            }
            int score = (nFl == 2) ? 1000 : (100 - nFl);
            if (score > bestScore) {
                bestScore = score;
                if (s_invGui.guiInv) env->DeleteGlobalRef(s_invGui.guiInv);
                s_invGui.guiInv = (jclass)env->NewGlobalRef(all[i]);
                s_invGui.ctor = ctor;
            }
        }
    }

    jint nfMc = 0; jfieldID* fMc = nullptr;
    jclass kbType = nullptr;
    jfieldID gsField = nullptr;
    if (jvmti->GetClassFields(mcCls, &nfMc, &fMc) == JVMTI_ERROR_NONE && fMc) {
        int bestKb = 0;
        for (jint f = 0; f < nfMc; f++) {
            char* fs = nullptr; jint fm = 0;
            if (jvmti->GetFieldName(mcCls, fMc[f], nullptr, &fs, nullptr) != JVMTI_ERROR_NONE) continue;
            jvmti->GetFieldModifiers(mcCls, fMc[f], &fm);
            if (!fs || fs[0] != 'L' || (fm & 0x0008)) {
                if (fs) jvmti->Deallocate((unsigned char*)fs);
                continue;
            }
            std::string cn = fs;
            jvmti->Deallocate((unsigned char*)fs);
            if (cn.size() < 3) continue;
            cn = cn.substr(1, cn.size() - 2);
            auto it = byName.find(cn);
            if (it == byName.end()) continue;
            jint nf2 = 0; jfieldID* f2 = nullptr;
            int same = 0;
            jclass repeated = nullptr;
            if (jvmti->GetClassFields(it->second, &nf2, &f2) == JVMTI_ERROR_NONE && f2) {
                std::unordered_map<std::string, int> cnt;
                std::unordered_map<std::string, jclass> tcls;
                for (jint g = 0; g < nf2; g++) {
                    char* gfs = nullptr; jint gfm = 0;
                    if (jvmti->GetFieldName(it->second, f2[g], nullptr, &gfs, nullptr) != JVMTI_ERROR_NONE) continue;
                    jvmti->GetFieldModifiers(it->second, f2[g], &gfm);
                    if (gfs && gfs[0] == 'L' && (gfm & 0x0008) == 0) {
                        cnt[gfs]++;
                    }
                    if (gfs) jvmti->Deallocate((unsigned char*)gfs);
                }
                for (auto& kv : cnt) {
                    if (kv.second > same) {
                        same = kv.second;
                        std::string tn = kv.first;
                        if (tn.size() > 2) {
                            tn = tn.substr(1, tn.size() - 2);
                            auto kt = byName.find(tn);
                            if (kt != byName.end()) repeated = kt->second;
                        }
                    }
                }
                jvmti->Deallocate((unsigned char*)f2);
            }
            if (same >= 5 && same > bestKb) {
                bestKb = same;
                gsField = fMc[f];
                kbType = repeated;
            }
        }
        jvmti->Deallocate((unsigned char*)fMc);
    }

    if (gsField && kbType) {
        jobject gs = env->GetObjectField(mc, gsField);
        if (gs && !env->ExceptionCheck()) {
            jclass gsCls = env->GetObjectClass(gs);
            jint nf2 = 0; jfieldID* f2 = nullptr;
            if (jvmti->GetClassFields(gsCls, &nf2, &f2) == JVMTI_ERROR_NONE && f2) {
                for (jint g = 0; g < nf2 && !s_invGui.invKey; g++) {
                    char* gfs = nullptr; jint gfm = 0;
                    if (jvmti->GetFieldName(gsCls, f2[g], nullptr, &gfs, nullptr) != JVMTI_ERROR_NONE) continue;
                    jvmti->GetFieldModifiers(gsCls, f2[g], &gfm);
                    bool isKb = gfs && gfs[0] == 'L' && (gfm & 0x0008) == 0;
                    if (gfs) jvmti->Deallocate((unsigned char*)gfs);
                    if (!isKb) continue;
                    jobject kb = env->GetObjectField(gs, f2[g]);
                    if (!kb || env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                    if (kbType && env->IsInstanceOf(kb, kbType) != JNI_TRUE) {
                        env->DeleteLocalRef(kb);
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        continue;
                    }
                    jclass kbCls = env->GetObjectClass(kb);
                    jint nf3 = 0; jfieldID* f3 = nullptr;
                    bool match = false;
                    if (jvmti->GetClassFields(kbCls, &nf3, &f3) == JVMTI_ERROR_NONE && f3) {
                        for (jint h = 0; h < nf3; h++) {
                            char* hfs = nullptr; jint hfm = 0;
                            if (jvmti->GetFieldName(kbCls, f3[h], nullptr, &hfs, nullptr) != JVMTI_ERROR_NONE) continue;
                            jvmti->GetFieldModifiers(kbCls, f3[h], &hfm);
                            if (hfs && strcmp(hfs, "Ljava/lang/String;") == 0 && (hfm & 0x0008) == 0) {
                                jobject jsObj = env->GetObjectField(kb, f3[h]);
                                if (env->ExceptionCheck()) { env->ExceptionClear(); jsObj = nullptr; }
                                jstring js = (jstring)jsObj;
                                if (js) {
                                    const char* utf = env->GetStringUTFChars(js, nullptr);
                                    if (utf && (strstr(utf, "inventory") || strstr(utf, "Inventory"))) match = true;
                                    if (utf) env->ReleaseStringUTFChars(js, utf);
                                    env->DeleteLocalRef(js);
                                }
                            }
                            if (hfs) jvmti->Deallocate((unsigned char*)hfs);
                        }
                        jvmti->Deallocate((unsigned char*)f3);
                    }
                    env->DeleteLocalRef(kbCls);
                    if (match) s_invGui.invKey = env->NewGlobalRef(kb);
                    env->DeleteLocalRef(kb);
                }
                jvmti->Deallocate((unsigned char*)f2);
            }
            env->DeleteLocalRef(gsCls);
            env->DeleteLocalRef(gs);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    if (s_invGui.screenFieldCount > 0) {
        jfieldID keep[4]{};
        int kn = 0;
        for (int i = 0; i < s_invGui.screenFieldCount; i++) {
            jobject o = env->GetObjectField(mc, s_invGui.screenFields[i]);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (!o) keep[kn++] = s_invGui.screenFields[i];
            else env->DeleteLocalRef(o);
        }
        if (kn > 0) {
            s_invGui.screenFieldCount = kn;
            for (int i = 0; i < kn; i++) s_invGui.screenFields[i] = keep[i];
        }
    }

    if (all)
        jvmti->Deallocate((unsigned char*)all);

    env->DeleteLocalRef(mcCls);
    env->DeleteLocalRef(mc);

    s_invGui.ready = true;
    GuiLog("ready display=%p guiInv=%p ctor=%p screens=%d invKey=%p ep=%s",
        (void*)s_invGui.display, (void*)s_invGui.guiInv, (void*)s_invGui.ctor,
        s_invGui.screenFieldCount, (void*)s_invGui.invKey, ep.c_str());
    return s_invGui.display || s_invGui.invKey || s_invGui.screenFieldCount > 0;
}

bool Minecraft::OpenPlayerInventory(JNIEnv* env) {
    EnsureInvGui(env);
    bool ok = false;
    if (!Mapper::IsCheatBreaker() && s_invGui.display && s_invGui.guiInv && s_invGui.ctor) {
        jobject player = GetThePlayer(env);
        jobject mc = GetMCInstance(env);
        if (player && mc) {
            jobject gui = env->NewObject(s_invGui.guiInv, s_invGui.ctor, player);
            if (env->ExceptionCheck()) { env->ExceptionClear(); gui = nullptr; }
            if (gui) {
                bool canDisplay = !s_invGui.guiScreen || env->IsInstanceOf(gui, s_invGui.guiScreen);
                if (env->ExceptionCheck()) { env->ExceptionClear(); canDisplay = false; }
                if (canDisplay) {
                    env->CallVoidMethod(mc, s_invGui.display, gui);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    else ok = true;
                }
                env->DeleteLocalRef(gui);
            }
        }
        if (player) env->DeleteLocalRef(player);
        if (mc) env->DeleteLocalRef(mc);
    }
    if (!ok)
        PressInventoryKey(env);
    jobject screen = GetCurrentScreen(env);
    if (screen) {
        env->DeleteLocalRef(screen);
        ok = true;
    }
    GuiLog("open ok=%d display=%p key=%p", ok ? 1 : 0, (void*)s_invGui.display, (void*)s_invGui.invKey);
    return ok;
}

void Minecraft::PressInventoryKey(JNIEnv* env) {
    EnsureInvGui(env);
    if (!s_invGui.invKey) {
        INPUT in[2]{};
        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wVk = 'E';
        in[1] = in[0];
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
        GuiLog("send E key fallback");
        return;
    }
    jclass kbCls = env->GetObjectClass(s_invGui.invKey);
    if (!kbCls) return;
    JavaVM* vm = nullptr;
    jvmtiEnv* jvmti = nullptr;
    if (env->GetJavaVM(&vm) == 0 && vm)
        vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2);
    jfieldID lastZ = nullptr;
    if (jvmti) {
        jint nf = 0; jfieldID* fids = nullptr;
        if (jvmti->GetClassFields(kbCls, &nf, &fids) == JVMTI_ERROR_NONE && fids) {
            for (jint i = 0; i < nf; i++) {
                char* fs = nullptr; jint fm = 0;
                if (jvmti->GetFieldName(kbCls, fids[i], nullptr, &fs, nullptr) != JVMTI_ERROR_NONE) continue;
                jvmti->GetFieldModifiers(kbCls, fids[i], &fm);
                if ((fm & 0x0008) == 0 && fs) {
                    if (fs[0] == 'I') {
                        jint v = env->GetIntField(s_invGui.invKey, fids[i]);
                        if (v == 0) env->SetIntField(s_invGui.invKey, fids[i], 1);
                    }
                    if (fs[0] == 'Z') lastZ = fids[i];
                }
                if (fs) jvmti->Deallocate((unsigned char*)fs);
            }
            jvmti->Deallocate((unsigned char*)fids);
        }
    }
    if (lastZ) env->SetBooleanField(s_invGui.invKey, lastZ, JNI_TRUE);
    env->DeleteLocalRef(kbCls);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

void Minecraft::ClosePlayerInventory(JNIEnv* env) {
    EnsureInvGui(env);
    jobject player = GetThePlayer(env);
    if (player) {
        ((Player*)player)->CloseScreen(env);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(player);
    }
    if (s_invGui.display) {
        jobject mc = GetMCInstance(env);
        if (mc) {
            env->CallVoidMethod(mc, s_invGui.display, (jobject)nullptr);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(mc);
        }
    }
    jobject still = GetCurrentScreen(env);
    if (still) {
        env->DeleteLocalRef(still);
        INPUT in[2]{};
        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wVk = 'E';
        in[1] = in[0];
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
        PressInventoryKey(env);
    }
}

bool Minecraft::IsPlayerInventoryScreen(JNIEnv* env) {
    jobject s = GetCurrentScreen(env);
    if (!s) return false;
    bool ok = true;
    if (s_invGui.guiInv) {
        jboolean inst = env->IsInstanceOf(s, s_invGui.guiInv);
        if (env->ExceptionCheck()) { env->ExceptionClear(); inst = JNI_FALSE; }
        if (inst) ok = true;
    }
    env->DeleteLocalRef(s);
    return ok;
}

void Minecraft::RightClickMouse(JNIEnv* env) {
    jobject mc = GetMCInstance(env);
    if (!mc) return;
    Klass* cls = (Klass*)env->GetObjectClass(mc);
    if (!cls) return;
    Method* m = cls->GetMethod(env, Mapper::Get("rightClickMouse").c_str(), "()V");
    env->DeleteLocalRef((jclass)cls);
    if (!m) return;
    m->CallVoidMethod(env, mc);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

int Minecraft::GetRightClickDelayTimer(JNIEnv* env) {
    BuildMCCache(env);
    if (!s_mc.fRightClickDelay) return -1;
    jobject mc = GetMCInstance(env);
    if (!mc) return -1;
    return s_mc.fRightClickDelay->GetIntField(env, mc);
}

void Minecraft::SetRightClickDelayTimer(JNIEnv* env, int ticks) {
    BuildMCCache(env);
    if (!s_mc.fRightClickDelay) return;
    jobject mc = GetMCInstance(env);
    if (!mc) return;
    s_mc.fRightClickDelay->SetIntField(env, mc, ticks);
}

jobject Minecraft::GetRenderManager(JNIEnv* env)
{
    if (!env) return nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();

    BuildMCCache(env);

    if (s_mc.fRenderManager) {
        jobject mc = GetMCInstance(env);
        if (mc) {
            jobject rm = s_mc.fRenderManager->GetObjectField(env, mc, false);
            if (env->ExceptionCheck()) { env->ExceptionClear(); rm = nullptr; }
            if (rm) return rm;
        }
    }

    const auto renderManagerClazz = g_Instance->FindClass(Mapper::Get("net/minecraft/client/renderer/entity/RenderManager"));
    if (!renderManagerClazz) return nullptr;

    std::string fname = Mapper::Get("renderManagerInstance");
    std::string fsig = Mapper::Get("net/minecraft/client/renderer/entity/RenderManager", 2);
    const auto field = renderManagerClazz->GetField(env, fname.c_str(), fsig.c_str(), true);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!field) return nullptr;
    jobject obj = field->GetObjectField(env, renderManagerClazz, true);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return obj;
}
