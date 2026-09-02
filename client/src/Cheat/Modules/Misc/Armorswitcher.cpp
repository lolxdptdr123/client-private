// ============================================================
//  ArmorSwitcher.cpp
//  Module JNI : scanne l'inventaire du joueur, détecte les
//  pièces d'armure à équiper ET les pièces déjà équipées,
//  puis déclenche Armor_Trigger() avec les infos complètes.
//
//  La détection couvre TOUTES les armures vanilla :
//    Leather : 298-301 | Chain : 302-305 | Iron  : 306-309
//    Diamond : 310-313 | Gold  : 314-317
//  piece index : 0=helmet, 1=chestplate, 2=leggings, 3=boots
//
//  Slots armure équipés dans InventoryPlayer (1.7.10) :
//    armorInventory[0] = boots      (piece 3)
//    armorInventory[1] = leggings   (piece 2)
//    armorInventory[2] = chestplate (piece 1)
//    armorInventory[3] = helmet     (piece 0)
//  → GetStackInSlot() avec index 36+k accède à armorInventory[k]
//    (ContainerPlayer mappe slots 5-8 → armorInventory[3..0])
// ============================================================

#include "pch.h"
#include "ArmorSwitcher.h"

#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Method.h"
#include "../../../Cheat/Hack.h"

// ── Scan à la demande ─────────────────────────────────────────────────────────
// Déclenché par Armor_Request_Scan() (appelé depuis Menu.cpp à la place du keybind).
// ArmorSwitcher::Run() consomme ce flag, scanne l'inventaire, puis appelle Armor_Trigger().
static std::atomic<bool> g_scanRequested{ false };

void Armor_Request_Scan() {
    g_scanRequested = true;
}

// ── SEH wrappers ──────────────────────────────────────────────────────────────
static jobject SafeGetStackInSlot(InventoryPlayer* inv, int slot, JNIEnv* env) {
    __try { return inv->GetStackInSlot(slot, env); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static jobject SafeGetItem(ItemStack* stack, JNIEnv* env) {
    __try { return stack->GetItem(env); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static int SafeCallGetId(Method* method, Klass* itemClass, jobject itemObj, JNIEnv* env) {
    __try { return method->CallIntMethod(env, itemClass, true, itemObj); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

// ── Résolution de getIdFromItem (cachée) ──────────────────────────────────────
static Klass* g_itemClass = nullptr;
static Method* g_getIdMethod = nullptr;

static bool ResolveItemClass(JNIEnv* env) {
    if (g_itemClass && g_getIdMethod) return true;

    g_itemClass = g_Instance->FindClass(Mapper::Get("net/minecraft/item/Item"));
    if (!g_itemClass) return false;

    std::string mapped = Mapper::Get("net/minecraft/item/Item");
    std::string sig = "(L" + mapped + ";)I";
    g_getIdMethod = g_itemClass->GetMethod(env,
        Mapper::Get("getIdFromItem").c_str(), sig.c_str(), true);

    return g_getIdMethod != nullptr;
}

// ── Lecture de l'ID d'un stack ────────────────────────────────────────────────
static int GetItemId(jobject stackObj, JNIEnv* env) {
    if (!stackObj || env->IsSameObject(stackObj, nullptr)) return -1;

    jobject itemObj = SafeGetItem((ItemStack*)stackObj, env);
    if (!itemObj || env->IsSameObject(itemObj, nullptr)) return -1;

    if (!ResolveItemClass(env)) return -1;
    return SafeCallGetId(g_getIdMethod, g_itemClass, itemObj, env);
}

// ── Run ───────────────────────────────────────────────────────────────────────
void ArmorSwitcher::Run(JNIEnv* env) {
    if (!Armor::enabled) { Sleep(50); return; }

    // Attend un scan demandé par le menu (keybind géré uniquement dans Menu.cpp)
    if (!g_scanRequested.exchange(false)) { Sleep(10); return; }

    // Récupère le joueur et son inventaire
    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj || env->IsSameObject(playerObj, nullptr)) return;

    jobject invObj = ((Player*)playerObj)->GetInventoryPlayer(env);
    if (!invObj || env->IsSameObject(invObj, nullptr)) return;

    auto* inv = (InventoryPlayer*)invObj;

    // ── Lecture des pièces DÉJÀ ÉQUIPÉES ─────────────────────────────────────
    //
    //  InventoryPlayer::armorInventory[] :
    //    index 0 → boots      (piece 3)
    //    index 1 → leggings   (piece 2)
    //    index 2 → chestplate (piece 1)
    //    index 3 → helmet     (piece 0)
    //
    //  GetStackInSlot() avec slot = 36 + k accède à armorInventory[k].

    int equippedIds[4] = { -1, -1, -1, -1 }; // equippedIds[piece]

    // Correspondance armorInventory index → piece index
    static const int kArmorSlotToPiece[4] = { 3, 2, 1, 0 };

    for (int k = 0; k < 4; k++) {
        int piece = kArmorSlotToPiece[k];
        jobject stackObj = SafeGetStackInSlot(inv, 36 + k, env);
        if (!stackObj || env->IsSameObject(stackObj, nullptr)) continue;
        int id = GetItemId(stackObj, env);
        if (id > 0) equippedIds[piece] = id;
    }

    // ── Scan de l'inventaire pour trouver les pièces à équiper ───────────────
    //
    //  On cherche dans l'inventaire principal (slots InventoryPlayer 9-35)
    //  et la hotbar (slots 0-8) une armure dont l'ID correspond à
    //  Armor::armorIds[piece][*] ou à IsArmorPiece() selon le mode.
    //  Priorité inventaire principal > hotbar.
    //
    //  Un slot n'est candidat que si la pièce trouvée est DIFFÉRENTE
    //  de l'armure actuellement équipée (pas la peine de swapper la même).

    int slotsToClick[4] = { -1, -1, -1, -1 };

    auto IsTargetPiece = [&](int itemId, int piece) -> bool {
        // Si l'ID correspond à l'armure déjà équipée → on ne la prend pas
        if (equippedIds[piece] >= 0 && itemId == equippedIds[piece]) return false;
        return Armor::IsArmorPiece(itemId, piece);
        };

    // Scan inventaire principal (slots 9-35)
    for (int invSlot = 9; invSlot <= 35; invSlot++) {
        jobject stackObj = SafeGetStackInSlot(inv, invSlot, env);
        if (!stackObj || env->IsSameObject(stackObj, nullptr)) continue;

        int id = GetItemId(stackObj, env);
        if (id < 0) continue;

        for (int piece = 0; piece < 4; piece++) {
            if (slotsToClick[piece] >= 0) continue;
            if (IsTargetPiece(id, piece))
                slotsToClick[piece] = invSlot;
        }
    }

    // Scan hotbar (slots 0-8) → container slots 0-8 dans ContainerSlotToGuiPos
    for (int invSlot = 0; invSlot <= 8; invSlot++) {
        jobject stackObj = SafeGetStackInSlot(inv, invSlot, env);
        if (!stackObj || env->IsSameObject(stackObj, nullptr)) continue;

        int id = GetItemId(stackObj, env);
        if (id < 0) continue;

        for (int piece = 0; piece < 4; piece++) {
            if (slotsToClick[piece] >= 0) continue;
            if (IsTargetPiece(id, piece))
                slotsToClick[piece] = invSlot;
        }
    }

    // ── Bilan ─────────────────────────────────────────────────────────────────
    int found = 0;
    for (int i = 0; i < 4; i++) if (slotsToClick[i] >= 0) found++;

    if (found == 0) {
        Armor::lastStatus = "Aucune armure a equiper trouvee";
        return;
    }

    Armor::lastStatus = std::to_string(found) + " piece(s) a equiper";
    Armor_Trigger(slotsToClick, equippedIds);
}