#pragma once
#include "pch.h"

namespace Armor {
    inline bool enabled = false;
    inline int  bind = 0;
    inline bool listening = false;
    inline int  speed = 10;
    inline std::string lastStatus = "En attente...";

    inline const char* pieceNames[4] = { "Helmet", "Chestplate", "Leggings", "Boots" };

    // IDs Minecraft configurés par pièce : armorIds[piece][slot] (4 pièces × 8 IDs possibles)
    // piece : 0=helmet, 1=chestplate, 2=leggings, 3=boots
    // slot 0 = ID principal, slots 1-7 = IDs alternatifs (0 = ignoré)
    inline int armorIds[4][8] = {
        { 310, 0, 0, 0, 0, 0, 0, 0 }, // Helmet     (défaut: Diamond)
        { 311, 0, 0, 0, 0, 0, 0, 0 }, // Chestplate (défaut: Diamond)
        { 312, 0, 0, 0, 0, 0, 0, 0 }, // Leggings   (défaut: Diamond)
        { 313, 0, 0, 0, 0, 0, 0, 0 }, // Boots      (défaut: Diamond)
    };

    // Détecte si un item ID est une pièce d'armure d'un type donné.
    // piece : 0=helmet, 1=chestplate, 2=leggings, 3=boots
    // IDs vanilla : leather=298-301, chain=302-305, iron=306-309, diamond=310-313, gold=314-317
    inline bool IsArmorPiece(int itemId, int piece) {
        for (int base = 298; base <= 314; base += 4)
            if (itemId == base + piece) return true;
        return false;
    }

    void Start();
    void Stop();
}

// Appelé depuis ArmorSwitcher::Run() avec les container slots trouvés par JNI.
// slotsToClick[i] = container slot (0-35) de la pièce i à équiper, -1 = non trouvé.
// equippedIds[i]  = item ID actuellement dans le slot armure i, -1 = slot vide.
void Armor_Trigger(const int slotsToClick[4], const int equippedIds[4]);

// Déclenche un scan JNI de l'inventaire (appelé depuis Menu.cpp sur keybind).
// ArmorSwitcher::Run() consomme ce flag et appelle Armor_Trigger() avec les vrais slots.
void Armor_Request_Scan();

// Déclenche manuellement le switch avec les derniers slots connus (sans nouveau scan JNI).
void Armor_Trigger_Manual();