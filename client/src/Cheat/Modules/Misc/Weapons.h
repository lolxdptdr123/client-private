#pragma once
#include "../Module.h"
#include <jni.h>

class ItemStack;

namespace WeaponsSettings {
    inline bool fist = false;
    inline bool swords = true;
    inline bool axes = true;
    inline int  extraIds[32] = {};
    inline bool sharpness = false;
    inline bool knockback = false;
    inline bool fireAspect = false;
    inline bool hotbar[9] = {};
}

class Weapons : public Module {
public:
    const char* GetName()   override { return "Weapons"; }
    bool        IsEnabled() override { return false; }
};

struct WeaponsCatalogEntry {
    int id;
    const char* name;
};

const WeaponsCatalogEntry* Weapons_Catalog(int& count);
const char* Weapons_NameForId(int id);
bool Weapons_HasExtra(int id);
bool Weapons_AddExtra(int id);
void Weapons_RemoveExtra(int id);

bool Weapons_IsStack(JNIEnv* env, ItemStack* stack, int hotbarSlot);
bool Weapons_IsHolding(JNIEnv* env);
