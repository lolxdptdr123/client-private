#pragma once

#include <jni.h>
#include <string>
#include "../../../Game/Mapper.h"
#include "../../Hack.h"

// ─────────────────────────────────────────────────────────────────────────────
// JNIMemory
// Cache les field IDs JNI nécessaires à la détection d'épée du clicker.
// Doit être initialisé via JNIMemory::Init(jvm) au démarrage du cheat.
// ─────────────────────────────────────────────────────────────────────────────

namespace JNIMemory
{
    // ── JavaVM ────────────────────────────────────────────────────────────────
    inline JavaVM* s_jvm = nullptr;
    inline bool      s_initialized = false;

    // ── Minecraft ─────────────────────────────────────────────────────────────
    inline jclass    s_classMC = nullptr; // net/minecraft/client/Minecraft
    inline jfieldID  s_fMC_instance = nullptr; // static Minecraft theMinecraft
    inline jfieldID  s_fMC_thePlayer = nullptr; // EntityClientPlayerMP thePlayer

    // ── InventoryPlayer ───────────────────────────────────────────────────────
    inline jfieldID  s_fPlayer_inventory = nullptr; // InventoryPlayer inventory
    inline jfieldID  s_fInv_currentItem = nullptr; // int currentItem
    inline jfieldID  s_fInv_mainInventory = nullptr; // ItemStack[] mainInventory

    // ── ItemStack / Item ──────────────────────────────────────────────────────
    inline jfieldID  s_fItemStack_item = nullptr; // Item item
    inline jfieldID  s_fItem_itemID = nullptr; // int itemID

    // ── Flag : fields épée cachés ─────────────────────────────────────────────
    inline bool      s_swordFieldsOk = false;

    // ─────────────────────────────────────────────────────────────────────────
    // Init : à appeler une fois depuis Hack::Attach() avec le JavaVM valide
    // ─────────────────────────────────────────────────────────────────────────
    inline void Init(JavaVM* jvm)
    {
        s_jvm = jvm;
        s_initialized = (jvm != nullptr);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // IsInitialized : vrai si Init() a été appelé avec un JVM valide
    // ─────────────────────────────────────────────────────────────────────────
    inline bool IsInitialized()
    {
        return s_initialized && s_jvm != nullptr;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // GetEnvPublic : retourne un JNIEnv* pour le thread courant
    // Attache automatiquement le thread si nécessaire
    // ─────────────────────────────────────────────────────────────────────────
    inline JNIEnv* GetEnvPublic()
    {
        if (!s_jvm) return nullptr;

        JNIEnv* env = nullptr;
        jint res = s_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);

        if (res == JNI_EDETACHED)
            res = s_jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);

        return (res == JNI_OK) ? env : nullptr;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // CacheSwordFields : récupère et stocke tous les field IDs nécessaires
    // Retourne true si le cache est complet, false sinon
    // ─────────────────────────────────────────────────────────────────────────
    inline bool CacheSwordFields()
    {
        JNIEnv* env = GetEnvPublic();
        if (!env) return false;

        auto findMc = [&](const char* key) -> jclass {
            std::string n = Mapper::Get(key);
            if (n.empty()) n = key;
            Klass* k = g_Instance ? g_Instance->FindClass(n) : nullptr;
            return k ? (jclass)k : nullptr;
        };

        jclass clsMC = findMc("net/minecraft/client/Minecraft");
        if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        if (!clsMC) return false;
        s_classMC = (jclass)env->NewGlobalRef(clsMC);

        std::string nInst = Mapper::Get("theMinecraft");
        std::string sInst = Mapper::Get("net/minecraft/client/Minecraft", 2);
        s_fMC_instance = env->GetStaticFieldID(s_classMC, nInst.c_str(), sInst.c_str());
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_fMC_instance = nullptr; }

        std::string nPl = Mapper::Get("thePlayer");
        std::string sMp = Mapper::Get("net/minecraft/client/entity/EntityClientPlayerMP", 2);
        s_fMC_thePlayer = env->GetFieldID(s_classMC, nPl.c_str(), sMp.c_str());
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_fMC_thePlayer = nullptr; }
        if (!s_fMC_thePlayer) {
            std::string sSp = Mapper::Get("net/minecraft/client/entity/EntityPlayerSP", 2);
            s_fMC_thePlayer = env->GetFieldID(s_classMC, nPl.c_str(), sSp.c_str());
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fMC_thePlayer = nullptr; }
        }

        if (!s_fMC_instance || !s_fMC_thePlayer) return false;

        jclass clsPlayer = findMc("net/minecraft/client/entity/EntityClientPlayerMP");
        if (env->ExceptionCheck()) { env->ExceptionClear(); clsPlayer = nullptr; }
        if (!clsPlayer)
            clsPlayer = findMc("net/minecraft/client/entity/EntityPlayerSP");

        if (clsPlayer) {
            std::string nInv = Mapper::Get("inventory");
            std::string sInv = Mapper::Get("net/minecraft/entity/player/InventoryPlayer", 2);
            s_fPlayer_inventory = env->GetFieldID(clsPlayer, nInv.c_str(), sInv.c_str());
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fPlayer_inventory = nullptr; }
        }
        if (!s_fPlayer_inventory) return false;

        jclass clsInv = findMc("net/minecraft/entity/player/InventoryPlayer");
        if (env->ExceptionCheck()) { env->ExceptionClear(); clsInv = nullptr; }

        if (clsInv) {
            std::string nCur = Mapper::Get("currentItem");
            s_fInv_currentItem = env->GetFieldID(clsInv, nCur.c_str(), "I");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fInv_currentItem = nullptr; }

            std::string nMain = Mapper::Get("mainInventory");
            std::string sArr = "[" + Mapper::Get("net/minecraft/item/ItemStack", 2);
            s_fInv_mainInventory = env->GetFieldID(clsInv, nMain.c_str(), sArr.c_str());
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fInv_mainInventory = nullptr; }
        }
        if (!s_fInv_currentItem || !s_fInv_mainInventory) return false;

        jclass clsStack = findMc("net/minecraft/item/ItemStack");
        if (env->ExceptionCheck()) { env->ExceptionClear(); clsStack = nullptr; }

        if (clsStack) {
            std::string nItem = Mapper::Get("theItem");
            if (nItem.empty()) nItem = Mapper::Get("item");
            std::string sItem = Mapper::Get("net/minecraft/item/Item", 2);
            s_fItemStack_item = env->GetFieldID(clsStack, nItem.c_str(), sItem.c_str());
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fItemStack_item = nullptr; }
            if (!s_fItemStack_item) {
                nItem = Mapper::Get("item");
                s_fItemStack_item = env->GetFieldID(clsStack, nItem.c_str(), sItem.c_str());
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_fItemStack_item = nullptr; }
            }
        }
        if (!s_fItemStack_item) return false;

        jclass clsItem = findMc("net/minecraft/item/Item");
        if (env->ExceptionCheck()) { env->ExceptionClear(); clsItem = nullptr; }

        if (clsItem) {
            std::string nId = Mapper::Get("itemID");
            if (nId.empty()) nId = "itemID";
            s_fItem_itemID = env->GetFieldID(clsItem, nId.c_str(), "I");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fItem_itemID = nullptr; }
        }
        if (!s_fItem_itemID) return false;

        s_swordFieldsOk = true;
        return true;
    }
}
