#include "pch.h"
#include "GuiScreen.h"

#include <string>

// Compare le nom de classe Java de l'objet avec un suffixe donné.
// Utilise GetObjectClass + getName — aucun cache, aucune dépendance externe.
static bool ClassNameEndsWith(JNIEnv* env, jobject obj, const char* suffix)
{
    if (!obj || !suffix) return false;

    jclass cls = env->GetObjectClass(obj);
    if (!cls) return false;

    jclass clsMeta = env->FindClass("java/lang/Class");
    if (!clsMeta) { env->DeleteLocalRef(cls); return false; }

    jmethodID getName = env->GetMethodID(clsMeta, "getName", "()Ljava/lang/String;");
    env->DeleteLocalRef(clsMeta);
    if (!getName) { env->DeleteLocalRef(cls); return false; }

    jstring jname = (jstring)env->CallObjectMethod(cls, getName);
    env->DeleteLocalRef(cls);
    if (!jname) return false;

    const char* name = env->GetStringUTFChars(jname, nullptr);
    bool result = false;
    if (name) {
        std::string s(name);
        std::string sfx(suffix);
        result = s.size() >= sfx.size() &&
            s.compare(s.size() - sfx.size(), sfx.size(), sfx) == 0;
        env->ReleaseStringUTFChars(jname, name);
    }
    env->DeleteLocalRef(jname);
    return result;
}

bool GuiScreen::IsInventory(JNIEnv* env) {
    if (this == nullptr) return false;
    // GuiInventory (survie) et GuiContainerCreative (créatif)
    return ClassNameEndsWith(env, (jobject)this, "GuiInventory")
        || ClassNameEndsWith(env, (jobject)this, "GuiContainerCreative");
}

bool GuiScreen::IsChat(JNIEnv* env) {
    if (this == nullptr) return false;
    return ClassNameEndsWith(env, (jobject)this, "GuiChat");
}

bool GuiScreen::IsContainerGui(JNIEnv* env) {
    if (this == nullptr) return false;
    // Coffres, double coffres, enclumes, fours, etc.
    // GuiChest, GuiCrafting, GuiEnchantment, GuiFurnace, GuiBrewingStand...
    // tous ont "Gui" dans leur nom et sont dans le package inventory
    // On exclut les écrans déjà couverts et les GUIs Lunar/options
    return ClassNameEndsWith(env, (jobject)this, "GuiChest")
        || ClassNameEndsWith(env, (jobject)this, "GuiCrafting")
        || ClassNameEndsWith(env, (jobject)this, "GuiFurnace")
        || ClassNameEndsWith(env, (jobject)this, "GuiEnchantment")
        || ClassNameEndsWith(env, (jobject)this, "GuiBrewingStand")
        || ClassNameEndsWith(env, (jobject)this, "GuiHopper")
        || ClassNameEndsWith(env, (jobject)this, "GuiDispenser")
        || ClassNameEndsWith(env, (jobject)this, "GuiBeacon")
        || ClassNameEndsWith(env, (jobject)this, "GuiEditSign")
        || ClassNameEndsWith(env, (jobject)this, "GuiMerchant");
}