#include "pch.h"
#include "AntiDebuff.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"

#include <jvmti.h>

static constexpr int kNausea = 9;
static constexpr int kBlindness = 15;

static jfieldID s_potionMap = nullptr;
static jfieldID s_timeInPortal = nullptr;
static jfieldID s_prevTimeInPortal = nullptr;

static jclass s_mapCls = nullptr;
static jclass s_setCls = nullptr;
static jclass s_iterCls = nullptr;
static jclass s_entryCls = nullptr;
static jmethodID s_entrySet = nullptr;
static jmethodID s_iterator = nullptr;
static jmethodID s_hasNext = nullptr;
static jmethodID s_next = nullptr;
static jmethodID s_getKey = nullptr;
static jmethodID s_getValue = nullptr;
static jmethodID s_mapRemove = nullptr;
static jmethodID s_mapPut = nullptr;

static jobject s_savedBlind = nullptr;
static jobject s_savedNausea = nullptr;
static jobject s_savedBlindKey = nullptr;
static jobject s_savedNauseaKey = nullptr;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static int PotionId(JNIEnv* env, jobject pe) {
    if (!pe) return 0;
    jclass cls = env->GetObjectClass(pe);
    if (!cls) return 0;
    int id = 0;
    std::string gn = Mapper::Get("getPotionID");
    const char* methods[] = { gn.c_str(), "getPotionID", "func_76456_a" };
    for (const char* n : methods) {
        if (!n || !n[0]) continue;
        jmethodID m = env->GetMethodID(cls, n, "()I");
        JniOk(env);
        if (!m) continue;
        id = env->CallIntMethod(pe, m);
        JniOk(env);
        if (id > 0) break;
    }
    if (id <= 0) {
        const char* fields[] = { "potionID", "field_76460_b" };
        for (const char* n : fields) {
            jfieldID f = env->GetFieldID(cls, n, "I");
            JniOk(env);
            if (!f) continue;
            id = env->GetIntField(pe, f);
            JniOk(env);
            if (id > 0) break;
        }
    }
    env->DeleteLocalRef(cls);
    return id;
}

static void EnsureJava(JNIEnv* env) {
    if (s_mapCls) return;
    auto glob = [&](const char* n) -> jclass {
        jclass c = env->FindClass(n);
        JniOk(env);
        if (!c) return nullptr;
        jclass g = (jclass)env->NewGlobalRef(c);
        env->DeleteLocalRef(c);
        return g;
    };
    s_mapCls = glob("java/util/Map");
    s_setCls = glob("java/util/Set");
    s_iterCls = glob("java/util/Iterator");
    s_entryCls = glob("java/util/Map$Entry");
    if (s_mapCls) {
        s_entrySet = env->GetMethodID(s_mapCls, "entrySet", "()Ljava/util/Set;");
        JniOk(env);
        s_mapRemove = env->GetMethodID(s_mapCls, "remove", "(Ljava/lang/Object;)Ljava/lang/Object;");
        JniOk(env);
        s_mapPut = env->GetMethodID(s_mapCls, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
        JniOk(env);
    }
    if (s_setCls)
        s_iterator = env->GetMethodID(s_setCls, "iterator", "()Ljava/util/Iterator;");
    JniOk(env);
    if (s_iterCls) {
        s_hasNext = env->GetMethodID(s_iterCls, "hasNext", "()Z");
        JniOk(env);
        s_next = env->GetMethodID(s_iterCls, "next", "()Ljava/lang/Object;");
        JniOk(env);
    }
    if (s_entryCls) {
        s_getKey = env->GetMethodID(s_entryCls, "getKey", "()Ljava/lang/Object;");
        JniOk(env);
        s_getValue = env->GetMethodID(s_entryCls, "getValue", "()Ljava/lang/Object;");
        JniOk(env);
    }
}

static void KeepRef(JNIEnv* env, jobject& slot, jobject local) {
    if (!local) return;
    if (slot) env->DeleteGlobalRef(slot);
    slot = env->NewGlobalRef(local);
}

static jfieldID FindFloatField(JNIEnv* env, jobject player, const char* mcp) {
    std::string mapped = Mapper::Get(mcp);
    jclass walk = env->GetObjectClass(player);
    jfieldID found = nullptr;
    for (int d = 0; walk && d < 10 && !found; d++) {
        if (!mapped.empty()) {
            found = env->GetFieldID(walk, mapped.c_str(), "F");
            JniOk(env);
        }
        if (!found) {
            found = env->GetFieldID(walk, mcp, "F");
            JniOk(env);
        }
        jclass sup = env->GetSuperclass(walk);
        env->DeleteLocalRef(walk);
        walk = sup;
    }
    if (walk) env->DeleteLocalRef(walk);
    if (found) return found;

    JavaVM* vm = nullptr;
    jvmtiEnv* jvmti = nullptr;
    if (env->GetJavaVM(&vm) != 0 || !vm) return nullptr;
    vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2);
    if (!jvmti) return nullptr;

    walk = env->GetObjectClass(player);
    for (int d = 0; walk && d < 8 && !found; d++) {
        jint nf = 0;
        jfieldID* fids = nullptr;
        if (jvmti->GetClassFields(walk, &nf, &fids) == JVMTI_ERROR_NONE && fids) {
            for (jint i = 0; i < nf && !found; i++) {
                char* name = nullptr;
                char* sig = nullptr;
                if (jvmti->GetFieldName(walk, fids[i], &name, &sig, nullptr) != JVMTI_ERROR_NONE)
                    continue;
                if (sig && strcmp(sig, "F") == 0 && name && strcmp(name, mcp) == 0)
                    found = fids[i];
                if (name) jvmti->Deallocate((unsigned char*)name);
                if (sig) jvmti->Deallocate((unsigned char*)sig);
            }
            jvmti->Deallocate((unsigned char*)fids);
        }
        jclass sup = env->GetSuperclass(walk);
        env->DeleteLocalRef(walk);
        walk = sup;
    }
    if (walk) env->DeleteLocalRef(walk);
    return found;
}

static void EnsurePlayer(JNIEnv* env, jobject player) {
    if (!player) return;
    if (!s_potionMap) {
        std::string mapName = Mapper::Get("activePotionsMap");
        if (mapName.empty()) mapName = "activePotionsMap";
        jclass walk = env->GetObjectClass(player);
        for (int d = 0; walk && d < 10; d++) {
            s_potionMap = env->GetFieldID(walk, mapName.c_str(), "Ljava/util/Map;");
            JniOk(env);
            if (!s_potionMap) {
                s_potionMap = env->GetFieldID(walk, mapName.c_str(), "Ljava/util/HashMap;");
                JniOk(env);
            }
            if (s_potionMap) break;
            jclass sup = env->GetSuperclass(walk);
            env->DeleteLocalRef(walk);
            walk = sup;
        }
        if (walk) env->DeleteLocalRef(walk);
    }
    if (!s_timeInPortal)
        s_timeInPortal = FindFloatField(env, player, "timeInPortal");
    if (!s_prevTimeInPortal)
        s_prevTimeInPortal = FindFloatField(env, player, "prevTimeInPortal");
}

static jobject GetMap(JNIEnv* env, jobject player) {
    EnsurePlayer(env, player);
    if (!s_potionMap) return nullptr;
    jobject map = env->GetObjectField(player, s_potionMap);
    JniOk(env);
    return map;
}

static void HideFromMap(JNIEnv* env, jobject player) {
    EnsureJava(env);
    jobject map = GetMap(env, player);
    if (!map || !s_entrySet || !s_iterator || !s_hasNext || !s_next || !s_getKey || !s_getValue || !s_mapRemove)
        return;

    jobject set = env->CallObjectMethod(map, s_entrySet);
    JniOk(env);
    if (!set) { env->DeleteLocalRef(map); return; }
    jobject it = env->CallObjectMethod(set, s_iterator);
    JniOk(env);
    env->DeleteLocalRef(set);
    if (!it) { env->DeleteLocalRef(map); return; }

    jobject dropKey[2]{};
    int dropN = 0;

    while (env->CallBooleanMethod(it, s_hasNext)) {
        JniOk(env);
        jobject ent = env->CallObjectMethod(it, s_next);
        JniOk(env);
        if (!ent) continue;
        jobject val = env->CallObjectMethod(ent, s_getValue);
        JniOk(env);
        int id = PotionId(env, val);
        bool hide = (id == kBlindness && AntiDebuffSettings::blindness)
            || (id == kNausea && AntiDebuffSettings::nausea);
        if (hide && dropN < 2) {
            jobject key = env->CallObjectMethod(ent, s_getKey);
            JniOk(env);
            if (id == kBlindness) {
                KeepRef(env, s_savedBlind, val);
                KeepRef(env, s_savedBlindKey, key);
            } else {
                KeepRef(env, s_savedNausea, val);
                KeepRef(env, s_savedNauseaKey, key);
            }
            dropKey[dropN++] = key ? env->NewGlobalRef(key) : nullptr;
            if (key) env->DeleteLocalRef(key);
        }
        if (val) env->DeleteLocalRef(val);
        env->DeleteLocalRef(ent);
    }
    env->DeleteLocalRef(it);

    for (int i = 0; i < dropN; i++) {
        if (!dropKey[i]) continue;
        jobject old = env->CallObjectMethod(map, s_mapRemove, dropKey[i]);
        JniOk(env);
        if (old) env->DeleteLocalRef(old);
        env->DeleteGlobalRef(dropKey[i]);
    }
    env->DeleteLocalRef(map);
}

static void RestoreMap(JNIEnv* env, jobject player) {
    EnsureJava(env);
    jobject map = GetMap(env, player);
    if (!map || !s_mapPut) return;
    auto put = [&](jobject& key, jobject& val) {
        if (!key || !val) return;
        jobject prev = env->CallObjectMethod(map, s_mapPut, key, val);
        JniOk(env);
        if (prev) env->DeleteLocalRef(prev);
        env->DeleteGlobalRef(key);
        env->DeleteGlobalRef(val);
        key = nullptr;
        val = nullptr;
    };
    put(s_savedBlindKey, s_savedBlind);
    put(s_savedNauseaKey, s_savedNausea);
    env->DeleteLocalRef(map);
}

static void ZeroPortal(JNIEnv* env, jobject player) {
    if (!AntiDebuffSettings::nausea) return;
    EnsurePlayer(env, player);
    if (s_timeInPortal) {
        env->SetFloatField(player, s_timeInPortal, 0.f);
        JniOk(env);
    }
    if (s_prevTimeInPortal) {
        env->SetFloatField(player, s_prevTimeInPortal, 0.f);
        JniOk(env);
    }
}

static void Tick(JNIEnv* env, bool on) {
    if (!env) return;
    jobject player = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!player) return;
    if (on) {
        HideFromMap(env, player);
        ZeroPortal(env, player);
    } else {
        RestoreMap(env, player);
    }
    env->DeleteLocalRef(player);
}

void AntiDebuff::Run(JNIEnv* env) {
    Tick(env, enabled);
}

void AntiDebuff::OnRender(JNIEnv* env) {
    Tick(env, enabled);
}
