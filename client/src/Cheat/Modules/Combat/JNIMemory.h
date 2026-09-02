#pragma once

#include <jni.h>

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

        // ── Minecraft ─────────────────────────────────────────────────────────
        jclass clsMC = env->FindClass("net/minecraft/client/Minecraft");
        if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        if (!clsMC) return false;
        s_classMC = (jclass)env->NewGlobalRef(clsMC);
        env->DeleteLocalRef(clsMC);

        s_fMC_instance = env->GetStaticFieldID(
            s_classMC, "theMinecraft", "Lnet/minecraft/client/Minecraft;");
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_fMC_instance = nullptr; }

        s_fMC_thePlayer = env->GetFieldID(
            s_classMC, "thePlayer",
            "Lnet/minecraft/client/entity/EntityClientPlayerMP;");
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_fMC_thePlayer = nullptr; }
        if (!s_fMC_thePlayer) {
            s_fMC_thePlayer = env->GetFieldID(
                s_classMC, "thePlayer",
                "Lnet/minecraft/client/entity/EntityPlayerSP;");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fMC_thePlayer = nullptr; }
        }

        if (!s_fMC_instance || !s_fMC_thePlayer) return false;

        // ── InventoryPlayer ───────────────────────────────────────────────────
        jclass clsPlayer = env->FindClass(
            "net/minecraft/client/entity/EntityClientPlayerMP");
        if (env->ExceptionCheck()) { env->ExceptionClear(); clsPlayer = nullptr; }
        if (!clsPlayer) {
            clsPlayer = env->FindClass("net/minecraft/client/entity/EntityPlayerSP");
            if (env->ExceptionCheck()) { env->ExceptionClear(); clsPlayer = nullptr; }
        }

        if (clsPlayer) {
            s_fPlayer_inventory = env->GetFieldID(
                clsPlayer, "inventory",
                "Lnet/minecraft/entity/player/InventoryPlayer;");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fPlayer_inventory = nullptr; }
            env->DeleteLocalRef(clsPlayer);
        }
        if (!s_fPlayer_inventory) return false;

        jclass clsInv = env->FindClass(
            "net/minecraft/entity/player/InventoryPlayer");
        if (env->ExceptionCheck()) { env->ExceptionClear(); clsInv = nullptr; }

        if (clsInv) {
            s_fInv_currentItem = env->GetFieldID(clsInv, "currentItem", "I");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fInv_currentItem = nullptr; }

            s_fInv_mainInventory = env->GetFieldID(
                clsInv, "mainInventory", "[Lnet/minecraft/item/ItemStack;");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fInv_mainInventory = nullptr; }

            env->DeleteLocalRef(clsInv);
        }
        if (!s_fInv_currentItem || !s_fInv_mainInventory) return false;

        // ── ItemStack ─────────────────────────────────────────────────────────
        jclass clsStack = env->FindClass("net/minecraft/item/ItemStack");
        if (env->ExceptionCheck()) { env->ExceptionClear(); clsStack = nullptr; }

        if (clsStack) {
            s_fItemStack_item = env->GetFieldID(
                clsStack, "theItem", "Lnet/minecraft/item/Item;");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fItemStack_item = nullptr; }
            if (!s_fItemStack_item) {
                s_fItemStack_item = env->GetFieldID(
                    clsStack, "item", "Lnet/minecraft/item/Item;");
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_fItemStack_item = nullptr; }
            }
            env->DeleteLocalRef(clsStack);
        }
        if (!s_fItemStack_item) return false;

        // ── Item ──────────────────────────────────────────────────────────────
        jclass clsItem = env->FindClass("net/minecraft/item/Item");
        if (env->ExceptionCheck()) { env->ExceptionClear(); clsItem = nullptr; }

        if (clsItem) {
            s_fItem_itemID = env->GetFieldID(clsItem, "itemID", "I");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_fItem_itemID = nullptr; }
            env->DeleteLocalRef(clsItem);
        }
        if (!s_fItem_itemID) return false;

        s_swordFieldsOk = true;
        return true;
    }
}