// ============================================================
//  SwordCheck.h — detection epee/hache en main via JNI
// ============================================================

#pragma once
#include "pch.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Cheat/Hack.h"

// ── Cache partagé via inline (C++17) ─────────────────────────────────────────
// CRITIQUE : ne PAS utiliser `static` ici.
// `static` dans un header = une instance PAR .cpp qui l'inclut.
// Velocity et Clicker auraient chacun leur propre g_swordCache,
// et SC_BuildCache ferait DeleteGlobalRef sur des refs appartenant à d'autres
// instances → double-free → crash 0x00...10.
// `inline` garantit UNE SEULE instance partagée entre tous les .cpp.
struct SwordCache {
    jclass clsItemSword = nullptr;
    jclass clsItemAxe = nullptr;
    bool   ok = false;
};
inline SwordCache g_swordCache;

// ── Fonctions SEH isolées ─────────────────────────────────────────────────────

static jobject SC_SafeGetHeldItem(Player* lp, JNIEnv* env) {
    __try {
        jobject r = lp->GetHeldItem(env);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
        return r;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static jobject SC_SafeGetItemField(JNIEnv* env, jobject stackObj, Field* fItem) {
    __try {
        jobject r = fItem->GetObjectField(env, stackObj);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
        return r;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static bool SC_SafeIsInstance(JNIEnv* env, jobject obj,
    jclass clsSword, jclass clsAxe) {
    __try {
        if (clsSword && env->IsInstanceOf(obj, clsSword)) return true;
        if (clsAxe && env->IsInstanceOf(obj, clsAxe))   return true;
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ── Build cache ───────────────────────────────────────────────────────────────

static bool SC_BuildCache(JNIEnv* env) {
    // Libérer les anciens refs proprement
    if (g_swordCache.clsItemSword) {
        env->DeleteGlobalRef(g_swordCache.clsItemSword);
        g_swordCache.clsItemSword = nullptr;
    }
    if (g_swordCache.clsItemAxe) {
        env->DeleteGlobalRef(g_swordCache.clsItemAxe);
        g_swordCache.clsItemAxe = nullptr;
    }
    g_swordCache.ok = false;

    Klass* kSword = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemSword"));
    if (!kSword) return false;
    g_swordCache.clsItemSword = (jclass)env->NewGlobalRef((jclass)kSword);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    Klass* kAxe = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemAxe"));
    if (kAxe) {
        g_swordCache.clsItemAxe = (jclass)env->NewGlobalRef((jclass)kAxe);
        if (env->ExceptionCheck()) { env->ExceptionClear(); g_swordCache.clsItemAxe = nullptr; }
    }

    g_swordCache.ok = true;
    return true;
}

// ── SC_IsHoldingSword ─────────────────────────────────────────────────────────
// Lunar : instanceof ItemSword / ItemAxe (fail-open si le cache n'est pas prêt).
// CheatBreaker : même règle, mais fail-closed (main vide / item inconnu = pas d'arme).

static bool SC_IsWeaponItemId(int id) {
    switch (id) {
    case 267: case 268: case 272: case 276: case 283:
    case 258: case 271: case 275: case 279: case 286:
        return true;
    default:
        return false;
    }
}

static bool SC_IsHoldingSword_Lunar(JNIEnv* env) {
    if (!env || !g_Instance) return true;

    if (!g_swordCache.ok)
        SC_BuildCache(env);
    if (!g_swordCache.ok) return true;

    jobject lpObj = Minecraft::GetThePlayer(env);
    if (!lpObj) return true;
    Player* lp = reinterpret_cast<Player*>(lpObj);

    jobject stackObj = SC_SafeGetHeldItem(lp, env);
    if (!stackObj) return false;

    jclass stackKlass = env->GetObjectClass(stackObj);
    if (!stackKlass) return false;

    std::string itemFieldName = Mapper::Get("item");
    std::string itemSig = "L" + std::string(Mapper::Get("net/minecraft/item/Item")) + ";";

    Field* fItem = ((Klass*)stackKlass)->GetField(env, itemFieldName.c_str(), itemSig.c_str());
    env->DeleteLocalRef(stackKlass);
    if (!fItem) return false;

    jobject itemObj = SC_SafeGetItemField(env, stackObj, fItem);
    if (!itemObj) return false;

    return SC_SafeIsInstance(env, itemObj,
        g_swordCache.clsItemSword,
        g_swordCache.clsItemAxe);
}

static bool SC_IsHoldingSword_CB(JNIEnv* env) {
    if (!env || !g_Instance) return false;

    if (!g_swordCache.ok)
        SC_BuildCache(env);

    jobject lpObj = Minecraft::GetThePlayer(env);
    if (!lpObj) return false;
    Player* lp = reinterpret_cast<Player*>(lpObj);

    jobject stackObj = SC_SafeGetHeldItem(lp, env);
    if (!stackObj) {
        static DWORD lastLog = 0;
        DWORD now = GetTickCount();
        if (now - lastLog > 1000) {
            lastLog = now;
            FILE* wf = nullptr;
            fopen_s(&wf, "C:\\Users\\bipbo\\Documents\\lolxd_weapon.txt", "w");
            if (wf) { fprintf(wf, "held item null player=%p cache=%d\n", (void*)lpObj, (int)g_swordCache.ok); fclose(wf); }
        }
        env->DeleteLocalRef(lpObj);
        return false;
    }

    jobject itemObj = ((ItemStack*)stackObj)->GetItem(env);
    if (!itemObj) {
        jclass stackKlass = env->GetObjectClass(stackObj);
        if (stackKlass) {
            std::string itemFieldName = Mapper::Get("item");
            std::string itemSig = Mapper::Get("net/minecraft/item/Item", 2);
            Field* fItem = ((Klass*)stackKlass)->GetField(env, itemFieldName.c_str(), itemSig.c_str());
            if (env->ExceptionCheck()) { env->ExceptionClear(); fItem = nullptr; }
            env->DeleteLocalRef(stackKlass);
            if (fItem) itemObj = SC_SafeGetItemField(env, stackObj, fItem);
        }
    }

    bool inst = false;
    if (itemObj && g_swordCache.ok)
        inst = SC_SafeIsInstance(env, itemObj, g_swordCache.clsItemSword, g_swordCache.clsItemAxe);

    int id = ((ItemStack*)stackObj)->GetItemId(env);
    bool ok = inst || SC_IsWeaponItemId(id);

    {
        static DWORD lastLog = 0;
        DWORD now = GetTickCount();
        if (now - lastLog > 1000) {
            lastLog = now;
            FILE* wf = nullptr;
            fopen_s(&wf, "C:\\Users\\bipbo\\Documents\\lolxd_weapon.txt", "w");
            if (wf) {
                fprintf(wf, "ok=%d inst=%d cache=%d id=%d item=%p stack=%p player=%p\n",
                    (int)ok, (int)inst, (int)g_swordCache.ok, id, (void*)itemObj, (void*)stackObj, (void*)lpObj);
                fclose(wf);
            }
        }
    }

    if (itemObj) env->DeleteLocalRef(itemObj);
    env->DeleteLocalRef(stackObj);
    env->DeleteLocalRef(lpObj);
    return ok;
}

#include "../Misc/Weapons.h"

static bool SC_IsHoldingSword(JNIEnv* env) {
    return Weapons_IsHolding(env);
}