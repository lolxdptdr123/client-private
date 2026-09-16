#include "pch.h"
#include "Weapons.h"

#include "../Combat/SwordCheck.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"

#include <cstdio>
#include <cstring>

static const WeaponsCatalogEntry kCatalog[] = {
    {1, "Stone"}, {2, "Grass"}, {3, "Dirt"}, {4, "Cobblestone"}, {5, "Oak wood planks"},
    {6, "Oak sapling"}, {12, "Sand"}, {13, "Gravel"}, {14, "Gold ore"}, {15, "Iron ore"},
    {16, "Coal ore"}, {17, "Oak wood"}, {18, "Oak leaves"}, {19, "Sponge"}, {20, "Glass"},
    {21, "Lapis lazuli ore"}, {22, "Lapis lazuli block"}, {23, "Dispenser"}, {24, "Sandstone"},
    {25, "Note block"}, {27, "Powered rail"}, {28, "Detector rail"}, {29, "Sticky piston"},
    {30, "Cobweb"}, {32, "Dead bush"}, {33, "Piston"}, {35, "Wool"}, {37, "Dandelion"},
    {38, "Poppy"}, {39, "Brown mushroom"}, {40, "Red mushroom"}, {41, "Gold block"},
    {42, "Iron block"}, {44, "Stone slab"}, {45, "Bricks"}, {46, "TNT"}, {47, "Bookshelf"},
    {48, "Moss stone"}, {49, "Obsidian"}, {50, "Torch"}, {53, "Oak wood stairs"},
    {54, "Chest"}, {57, "Diamond block"}, {58, "Crafting table"}, {65, "Ladder"},
    {66, "Rail"}, {67, "Cobblestone stairs"}, {69, "Lever"}, {70, "Stone pressure plate"},
    {72, "Wooden pressure plate"}, {76, "Redstone torch"}, {77, "Stone button"},
    {78, "Snow"}, {79, "Ice"}, {80, "Snow block"}, {81, "Cactus"}, {82, "Clay"},
    {84, "Jukebox"}, {85, "Fence"}, {86, "Pumpkin"}, {87, "Netherrack"}, {88, "Soul sand"},
    {89, "Glowstone"}, {91, "Jack o'lantern"}, {96, "Trapdoor"}, {98, "Stone bricks"},
    {101, "Iron bars"}, {102, "Glass pane"}, {103, "Melon block"}, {106, "Vines"},
    {107, "Fence gate"}, {108, "Brick stairs"}, {109, "Stone brick stairs"},
    {110, "Mycelium"}, {111, "Lily pad"}, {112, "Nether brick"}, {113, "Nether brick fence"},
    {114, "Nether brick stairs"}, {116, "Enchantment table"}, {120, "End portal frame"},
    {121, "End stone"}, {123, "Redstone lamp"}, {128, "Sandstone stairs"},
    {129, "Emerald ore"}, {130, "Ender chest"}, {131, "Tripwire hook"},
    {133, "Emerald block"}, {134, "Spruce wood stairs"}, {135, "Birch wood stairs"},
    {136, "Jungle wood stairs"}, {138, "Beacon"}, {139, "Cobblestone wall"},
    {143, "Wooden button"}, {145, "Anvil"}, {146, "Trapped chest"},
    {147, "Weighted pressure plate (light)"}, {148, "Weighted pressure plate (heavy)"},
    {151, "Daylight sensor"}, {152, "Redstone block"}, {153, "Nether quartz ore"},
    {154, "Hopper"}, {155, "Quartz block"}, {156, "Quartz stairs"}, {157, "Activator rail"},
    {158, "Dropper"}, {159, "Stained clay"}, {160, "Stained glass pane"},
    {162, "Acacia wood"}, {163, "Acacia wood stairs"}, {164, "Dark oak wood stairs"},
    {165, "Slime block"}, {167, "Iron trapdoor"}, {168, "Prismarine"}, {169, "Sea lantern"},
    {170, "Hay bale"}, {171, "Carpet"}, {172, "Hardened clay"}, {173, "Block of coal"},
    {174, "Packed ice"}, {175, "Sunflower"}, {179, "Red sandstone"},
    {180, "Red sandstone stairs"}, {182, "Red sandstone slab"},
    {183, "Spruce fence gate"}, {184, "Birch fence gate"}, {185, "Jungle fence gate"},
    {186, "Dark oak fence gate"}, {187, "Acacia fence gate"}, {188, "Spruce fence"},
    {189, "Birch fence"}, {190, "Jungle fence"}, {191, "Dark oak fence"},
    {192, "Acacia fence"},
    {256, "Iron shovel"}, {257, "Iron pickaxe"}, {258, "Iron axe"}, {259, "Flint and steel"},
    {260, "Apple"}, {261, "Bow"}, {262, "Arrow"}, {263, "Coal"}, {264, "Diamond"},
    {265, "Iron ingot"}, {266, "Gold ingot"}, {267, "Iron sword"}, {268, "Wooden sword"},
    {269, "Wooden shovel"}, {270, "Wooden pickaxe"}, {271, "Wooden axe"}, {272, "Stone sword"},
    {273, "Stone shovel"}, {274, "Stone pickaxe"}, {275, "Stone axe"}, {276, "Diamond sword"},
    {277, "Diamond shovel"}, {278, "Diamond pickaxe"}, {279, "Diamond axe"}, {280, "Stick"},
    {281, "Bowl"}, {282, "Mushroom stew"}, {283, "Golden sword"}, {284, "Golden shovel"},
    {285, "Golden pickaxe"}, {286, "Golden axe"}, {287, "String"}, {288, "Feather"},
    {289, "Gunpowder"}, {290, "Wooden hoe"}, {291, "Stone hoe"}, {292, "Iron hoe"},
    {293, "Diamond hoe"}, {294, "Golden hoe"}, {295, "Seeds"}, {296, "Wheat"},
    {297, "Bread"}, {298, "Leather cap"}, {299, "Leather tunic"}, {300, "Leather pants"},
    {301, "Leather boots"}, {302, "Chainmail helmet"}, {303, "Chainmail chestplate"},
    {304, "Chainmail leggings"}, {305, "Chainmail boots"}, {306, "Iron helmet"},
    {307, "Iron chestplate"}, {308, "Iron leggings"}, {309, "Iron boots"},
    {310, "Diamond helmet"}, {311, "Diamond chestplate"}, {312, "Diamond leggings"},
    {313, "Diamond boots"}, {314, "Golden helmet"}, {315, "Golden chestplate"},
    {316, "Golden leggings"}, {317, "Golden boots"}, {318, "Flint"}, {319, "Raw porkchop"},
    {320, "Cooked porkchop"}, {321, "Painting"}, {322, "Golden apple"}, {323, "Sign"},
    {324, "Oak door item"}, {325, "Bucket"}, {326, "Water bucket"}, {327, "Lava bucket"},
    {328, "Minecart"}, {329, "Saddle"}, {330, "Iron door item"}, {331, "Redstone"},
    {332, "Snowball"}, {333, "Boat"}, {334, "Leather"}, {335, "Milk"}, {336, "Brick"},
    {337, "Clay"}, {338, "Sugar canes"}, {339, "Paper"}, {340, "Book"}, {341, "Slimeball"},
    {342, "Chest minecart"}, {343, "Furnace minecart"}, {344, "Egg"}, {345, "Compass"},
    {346, "Fishing rod"}, {347, "Clock"}, {348, "Glowstone dust"}, {349, "Raw fish"},
    {350, "Cooked fish"}, {351, "Ink sac"}, {352, "Bone"}, {353, "Sugar"},
    {354, "Cake item"}, {355, "Bed item"}, {356, "Redstone repeater"}, {357, "Cookie"},
    {358, "Map"}, {359, "Shears"}, {360, "Melon"}, {361, "Pumpkin seeds"},
    {362, "Melon seeds"}, {363, "Beef"}, {364, "Steak"}, {365, "Chicken"},
    {366, "Cooked chicken"}, {367, "Rotten flesh"}, {368, "Ender pearl"}, {369, "Blaze rod"},
    {370, "Ghast tear"}, {371, "Gold nugget"}, {372, "Nether wart"}, {373, "Potion"},
    {374, "Glass bottle"}, {375, "Spider eye"}, {376, "Fermented spider eye"},
    {377, "Blaze powder"}, {378, "Magma cream"}, {379, "Brewing stand item"},
    {380, "Cauldron item"}, {381, "Eye of ender"}, {382, "Glistering melon"},
    {383, "Spawn egg"}, {384, "Bottle o' enchanting"}, {385, "Fire charge"},
    {386, "Book and quill"}, {387, "Written book"}, {388, "Emerald"}, {389, "Item frame"},
    {390, "Flower pot item"}, {391, "Carrot"}, {392, "Potato"}, {393, "Baked potato"},
    {394, "Poisonous potato"}, {395, "Empty map"}, {396, "Golden carrot"},
    {397, "Skeleton skull"}, {398, "Carrot on a stick"}, {399, "Nether star"},
    {400, "Pumpkin pie"}, {401, "Firework rocket"}, {402, "Firework star"},
    {403, "Enchanted book"}, {404, "Redstone comparator"}, {405, "Nether brick"},
    {406, "Nether quartz"}, {407, "TNT minecart"}, {408, "Hopper minecart"},
    {409, "Prismarine shard"}, {410, "Prismarine crystals"}, {411, "Raw rabbit"},
    {412, "Cooked rabbit"}, {413, "Rabbit stew"}, {414, "Rabbit's foot"},
    {415, "Rabbit hide"}, {416, "Armor stand"}, {417, "Iron horse armor"},
    {418, "Gold horse armor"}, {419, "Diamond horse armor"}, {420, "Lead"},
    {421, "Name tag"}, {422, "Command block minecart"}, {423, "Raw mutton"},
    {424, "Cooked mutton"}, {425, "Banner"}, {427, "Spruce door item"},
    {428, "Birch door item"}, {429, "Jungle door item"}, {430, "Acacia door item"},
    {431, "Dark oak door item"},
};

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static bool IdIsSword(int id) {
    switch (id) {
    case 267: case 268: case 272: case 276: case 283:
        return true;
    default:
        return false;
    }
}

static bool IdIsAxe(int id) {
    switch (id) {
    case 258: case 271: case 275: case 279: case 286:
        return true;
    default:
        return false;
    }
}

const WeaponsCatalogEntry* Weapons_Catalog(int& count) {
    count = (int)(sizeof(kCatalog) / sizeof(kCatalog[0]));
    return kCatalog;
}

const char* Weapons_NameForId(int id) {
    if (id <= 0) return "";
    for (const auto& e : kCatalog) {
        if (e.id == id) return e.name;
    }
    static char buf[24];
    snprintf(buf, sizeof(buf), "Item %d", id);
    return buf;
}

bool Weapons_HasExtra(int id) {
    if (id <= 0) return false;
    for (int i = 0; i < 32; i++) {
        if (WeaponsSettings::extraIds[i] == id) return true;
    }
    return false;
}

bool Weapons_AddExtra(int id) {
    if (id <= 0 || Weapons_HasExtra(id)) return false;
    for (int i = 0; i < 32; i++) {
        if (WeaponsSettings::extraIds[i] <= 0) {
            WeaponsSettings::extraIds[i] = id;
            return true;
        }
    }
    return false;
}

void Weapons_RemoveExtra(int id) {
    int w = 0;
    for (int i = 0; i < 32; i++) {
        if (WeaponsSettings::extraIds[i] > 0 && WeaponsSettings::extraIds[i] != id)
            WeaponsSettings::extraIds[w++] = WeaponsSettings::extraIds[i];
    }
    for (; w < 32; w++)
        WeaponsSettings::extraIds[w] = 0;
}

bool Weapons_IsStack(JNIEnv* env, ItemStack* stack, int hotbarSlot) {
    if (!env) return false;

    if (hotbarSlot >= 0 && hotbarSlot < 9 && WeaponsSettings::hotbar[hotbarSlot])
        return true;

    if (!stack)
        return WeaponsSettings::fist;

    if (!g_swordCache.ok)
        SC_BuildCache(env);

    jobject itemObj = stack->GetItem(env);
    JniOk(env);
    bool isSword = false;
    bool isAxe = false;
    if (itemObj && g_swordCache.ok) {
        if (WeaponsSettings::swords && g_swordCache.clsItemSword)
            isSword = env->IsInstanceOf(itemObj, g_swordCache.clsItemSword) != JNI_FALSE;
        JniOk(env);
        if (WeaponsSettings::axes && g_swordCache.clsItemAxe)
            isAxe = env->IsInstanceOf(itemObj, g_swordCache.clsItemAxe) != JNI_FALSE;
        JniOk(env);
    }
    if (itemObj) env->DeleteLocalRef(itemObj);

    int id = stack->GetItemId(env);
    JniOk(env);
    if (WeaponsSettings::swords && (isSword || IdIsSword(id))) return true;
    if (WeaponsSettings::axes && (isAxe || IdIsAxe(id))) return true;
    if (Weapons_HasExtra(id)) return true;

    if (WeaponsSettings::sharpness && stack->GetEnchantmentLevel(16, env) > 0) {
        JniOk(env);
        return true;
    }
    JniOk(env);
    if (WeaponsSettings::knockback && stack->GetEnchantmentLevel(19, env) > 0) {
        JniOk(env);
        return true;
    }
    JniOk(env);
    if (WeaponsSettings::fireAspect && stack->GetEnchantmentLevel(20, env) > 0) {
        JniOk(env);
        return true;
    }
    JniOk(env);
    return false;
}

bool Weapons_IsHolding(JNIEnv* env) {
    if (!env || !g_Instance) return false;

    jobject lpObj = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!lpObj) return false;
    auto* lp = (Player*)lpObj;

    int slot = -1;
    jobject invObj = lp->GetInventoryPlayer(env);
    JniOk(env);
    if (invObj) {
        slot = ((InventoryPlayer*)invObj)->GetSlot(env);
        JniOk(env);
        env->DeleteLocalRef(invObj);
    }

    jobject stackObj = lp->GetHeldItem(env);
    JniOk(env);
    bool ok = Weapons_IsStack(env, stackObj ? (ItemStack*)stackObj : nullptr, slot);
    if (stackObj) env->DeleteLocalRef(stackObj);
    env->DeleteLocalRef(lpObj);
    return ok;
}
