#include "pch.h"
#include "AutoTool.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/MovingObjectPosition.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Block.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Method.h"
#include "../../../Cheat/Hack.h"
#include "../../../Cheat/Modules/Settings.h"

static int s_savedSlot = -1;
static bool s_switched = false;
static ULONGLONG s_lastSwitch = 0;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static bool IsLunar17or18() {
    return g_GameLauncher == LAUNCHER_LUNAR
        && (g_GameVersion == LUNAR_1_7_10 || g_GameVersion == LUNAR_1_8_9);
}

static bool IsSilkable(int blockId) {
    switch (blockId) {
    case 1: case 2: case 16: case 18: case 20: case 21:
    case 56: case 73: case 74: case 79: case 89: case 103:
    case 110: case 129: case 153: case 161: case 174:
        return true;
    default:
        return false;
    }
}

static float ToolTier(int itemId) {
    switch (itemId) {
    case 269: case 270: case 271: return 2.f;
    case 273: case 274: case 275: return 4.f;
    case 256: case 257: case 258: return 6.f;
    case 277: case 278: case 279: return 8.f;
    case 284: case 285: case 286: return 12.f;
    case 359: return 5.f;
    default: return 0.f;
    }
}

static bool IsPickId(int id) { return id == 270 || id == 274 || id == 257 || id == 278 || id == 285; }
static bool IsAxeId(int id)  { return id == 271 || id == 275 || id == 258 || id == 279 || id == 286; }
static bool IsShovelId(int id) { return id == 269 || id == 273 || id == 256 || id == 277 || id == 284; }

static bool BlockWantsPick(int b) {
    switch (b) {
    case 1: case 4: case 14: case 15: case 16: case 21: case 22: case 24:
    case 41: case 42: case 43: case 44: case 45: case 48: case 49: case 56:
    case 57: case 67: case 73: case 74: case 87: case 98: case 108: case 109:
    case 112: case 121: case 129: case 133: case 152: case 153: case 155:
    case 159: case 168: case 172: case 173: case 179: case 181: case 182:
        return true;
    default: return false;
    }
}

static bool BlockWantsAxe(int b) {
    switch (b) {
    case 5: case 17: case 25: case 47: case 53: case 54: case 58: case 84:
    case 85: case 96: case 103: case 107: case 125: case 126: case 134:
    case 135: case 136: case 146: case 162: case 163: case 164: case 183:
    case 184: case 185: case 186: case 187:
        return true;
    default: return false;
    }
}

static bool BlockWantsShovel(int b) {
    switch (b) {
    case 2: case 3: case 12: case 13: case 78: case 80: case 82: case 88:
    case 110: case 198:
        return true;
    default: return false;
    }
}

static bool BlockWantsShears(int b) {
    return b == 18 || b == 30 || b == 31 || b == 35 || b == 106 || b == 161;
}

static bool IsInstanceOfMapped(JNIEnv* env, jobject obj, const char* mapping) {
    if (!env || !obj) return false;
    std::string n = Mapper::Get(mapping);
    if (n.empty()) n = mapping;
    Klass* k = g_Instance->FindClass(n.c_str());
    if (!k) return false;
    bool ok = env->IsInstanceOf(obj, (jclass)k) != JNI_FALSE;
    JniOk(env);
    return ok;
}

static bool IsToolItem(JNIEnv* env, jobject item, int itemId) {
    if (IsPickId(itemId) || IsAxeId(itemId) || IsShovelId(itemId) || itemId == 359)
        return true;
    if (!item) return false;
    return IsInstanceOfMapped(env, item, "net/minecraft/item/ItemPickaxe")
        || IsInstanceOfMapped(env, item, "net/minecraft/item/ItemAxe")
        || IsInstanceOfMapped(env, item, "net/minecraft/item/ItemSpade")
        || IsInstanceOfMapped(env, item, "net/minecraft/item/ItemShears");
}

static float GetStrVsBlock(JNIEnv* env, jobject stack, jobject block) {
    if (!env || !stack || !block) return 1.f;
    jclass sc = env->GetObjectClass(stack);
    if (!sc) return 1.f;
    std::string sig = "(" + Mapper::Get("net/minecraft/block/Block", 2) + ")F";
    if (sig.size() < 5) {
        env->DeleteLocalRef(sc);
        return 1.f;
    }
    const char* names[] = { "getStrVsBlock", "func_150997_a" };
    float str = -1.f;
    for (const char* name : names) {
        jmethodID m = env->GetMethodID(sc, name, sig.c_str());
        JniOk(env);
        if (!m) continue;
        str = env->CallFloatMethod(stack, m, block);
        JniOk(env);
        break;
    }
    env->DeleteLocalRef(sc);
    if (str < 0.f) return 1.f;
    return str;
}

static float FallbackScore(int itemId, int blockId) {
    float tier = ToolTier(itemId);
    if (tier <= 0.f) return 1.f;
    if (itemId == 359) {
        if (blockId == 30) return 15.f;
        if (BlockWantsShears(blockId)) return 5.f;
        return 1.f;
    }
    if (IsPickId(itemId) && BlockWantsPick(blockId)) return tier;
    if (IsAxeId(itemId) && BlockWantsAxe(blockId)) return tier;
    if (IsShovelId(itemId) && BlockWantsShovel(blockId)) return tier;
    return 1.f;
}

static jobject GetLookedBlock(JNIEnv* env, jobject mop, jobject world) {
    if (!env || !mop || !world) return nullptr;

    jclass mopC = env->GetObjectClass(mop);
    jclass worldC = env->GetObjectClass(world);
    if (!mopC || !worldC) {
        if (mopC) env->DeleteLocalRef(mopC);
        if (worldC) env->DeleteLocalRef(worldC);
        return nullptr;
    }

    std::string bpName = Mapper::Get("net/minecraft/util/BlockPos");
    if (bpName.empty()) bpName = "net/minecraft/util/BlockPos";
    std::string bpSig = "L" + bpName + ";";
    std::string blockSig = Mapper::Get("net/minecraft/block/Block", 2);
    std::string ibsName = Mapper::Get("net/minecraft/block/state/IBlockState");
    if (ibsName.empty()) ibsName = "net/minecraft/block/state/IBlockState";

    jobject result = nullptr;
    jobject bp = nullptr;

    std::string getBpName = Mapper::Get("getBlockPos");
    if (getBpName.empty()) getBpName = "getBlockPos";
    jmethodID getBP = env->GetMethodID(mopC, getBpName.c_str(), ("()" + bpSig).c_str());
    JniOk(env);
    if (getBP) {
        bp = env->CallObjectMethod(mop, getBP);
        JniOk(env);
    }
    if (!bp) {
        std::string bpField = Mapper::Get("blockPos");
        if (bpField.empty()) bpField = "blockPos";
        jfieldID f = env->GetFieldID(mopC, bpField.c_str(), bpSig.c_str());
        JniOk(env);
        if (f) {
            bp = env->GetObjectField(mop, f);
            JniOk(env);
        }
    }

    if (bp && !blockSig.empty()) {
        std::string gbsSig = "(" + bpSig + ")L" + ibsName + ";";
        jmethodID gbs = env->GetMethodID(worldC, "getBlockState", gbsSig.c_str());
        JniOk(env);
        if (gbs) {
            jobject state = env->CallObjectMethod(world, gbs, bp);
            JniOk(env);
            if (state) {
                jclass stC = env->GetObjectClass(state);
                jmethodID gb = env->GetMethodID(stC, Mapper::Get("getBlock").c_str(), ("()" + blockSig).c_str());
                JniOk(env);
                if (gb) {
                    result = env->CallObjectMethod(state, gb);
                    JniOk(env);
                }
                env->DeleteLocalRef(stC);
                env->DeleteLocalRef(state);
            }
        }
        if (!result) {
            jmethodID gb = env->GetMethodID(worldC, Mapper::Get("getBlock").c_str(), ("(" + bpSig + ")" + blockSig).c_str());
            JniOk(env);
            if (gb) {
                result = env->CallObjectMethod(world, gb, bp);
                JniOk(env);
            }
        }
        env->DeleteLocalRef(bp);
    }

    if (!result && !blockSig.empty()) {
        jfieldID fx = env->GetFieldID(mopC, "blockX", "I"); JniOk(env);
        jfieldID fy = env->GetFieldID(mopC, "blockY", "I"); JniOk(env);
        jfieldID fz = env->GetFieldID(mopC, "blockZ", "I"); JniOk(env);
        if (fx && fy && fz) {
            int x = env->GetIntField(mop, fx);
            int y = env->GetIntField(mop, fy);
            int z = env->GetIntField(mop, fz);
            jmethodID gb = env->GetMethodID(worldC, Mapper::Get("getBlock").c_str(), ("(III)" + blockSig).c_str());
            JniOk(env);
            if (gb) {
                result = env->CallObjectMethod(world, gb, x, y, z);
                JniOk(env);
            }
        }
    }

    env->DeleteLocalRef(mopC);
    env->DeleteLocalRef(worldC);
    return result;
}

static float ScoreStack(JNIEnv* env, jobject stack, jobject block, int blockId) {
    if (!stack) return 1.f;
    auto* is = (ItemStack*)stack;
    if (is->IsEmpty(env)) return 1.f;

    jobject item = is->GetItem(env);
    int itemId = is->GetItemId(env);
    JniOk(env);

    float str = GetStrVsBlock(env, stack, block);
    if (str <= 1.01f)
        str = FallbackScore(itemId, blockId);

    if (AutoToolSettings::preferSilk && IsSilkable(blockId) && is->GetEnchantmentLevel(33, env) > 0)
        str += 50.f;

    if (IsToolItem(env, item, itemId) && str > 1.01f)
        str += 0.01f;

    if (item) env->DeleteLocalRef(item);
    return str;
}

static void RestoreSlot(JNIEnv* env, InventoryPlayer* inv) {
    if (!inv || s_savedSlot < 0) {
        s_savedSlot = -1;
        s_switched = false;
        return;
    }
    inv->SetSlot(s_savedSlot, env);
    JniOk(env);
    s_savedSlot = -1;
    s_switched = false;
}

void AutoTool::Run(JNIEnv* env) {
    if (!IsLunar17or18()) {
        Sleep(50);
        return;
    }

    if (!enabled || !env || Overlay::isOpen) {
        if (env && s_switched && AutoToolSettings::switchBack) {
            jobject playerObj = Minecraft::GetThePlayer(env);
            JniOk(env);
            if (playerObj) {
                jobject invObj = ((Player*)playerObj)->GetInventoryPlayer(env);
                if (invObj) {
                    RestoreSlot(env, (InventoryPlayer*)invObj);
                    env->DeleteLocalRef(invObj);
                }
                env->DeleteLocalRef(playerObj);
            }
        } else {
            s_savedSlot = -1;
            s_switched = false;
        }
        Sleep(20);
        return;
    }

    jobject screen = Minecraft::GetCurrentScreen(env);
    JniOk(env);
    if (screen) {
        env->DeleteLocalRef(screen);
        jobject playerObj = Minecraft::GetThePlayer(env);
        if (playerObj) {
            jobject invObj = ((Player*)playerObj)->GetInventoryPlayer(env);
            if (invObj && AutoToolSettings::switchBack && s_switched)
                RestoreSlot(env, (InventoryPlayer*)invObj);
            if (invObj) env->DeleteLocalRef(invObj);
            env->DeleteLocalRef(playerObj);
        }
        Sleep(8);
        return;
    }

    const bool mining = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    jobject mop = Minecraft::GetObjectMouseOver(env);
    JniOk(env);
    jobject world = Minecraft::GetTheWorld(env);
    JniOk(env);
    jobject playerObj = Minecraft::GetThePlayer(env);
    JniOk(env);

    bool aimingBlock = mop && ((MovingObjectPosition*)mop)->IsAimingBlock(env);
    JniOk(env);

    const bool shouldPick = aimingBlock && (!AutoToolSettings::onlyMining || mining);

    if (!playerObj || !world) {
        if (mop) env->DeleteLocalRef(mop);
        if (world) env->DeleteLocalRef(world);
        if (playerObj) env->DeleteLocalRef(playerObj);
        Sleep(4);
        return;
    }

    jobject invObj = ((Player*)playerObj)->GetInventoryPlayer(env);
    JniOk(env);
    if (!invObj) {
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(world);
        if (mop) env->DeleteLocalRef(mop);
        Sleep(4);
        return;
    }
    auto* inv = (InventoryPlayer*)invObj;

    if (!shouldPick) {
        if (AutoToolSettings::switchBack && s_switched)
            RestoreSlot(env, inv);
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(world);
        if (mop) env->DeleteLocalRef(mop);
        Sleep(2);
        return;
    }

    jobject block = GetLookedBlock(env, mop, world);
    if (!block || ((Block*)block)->IsAir(env)) {
        if (AutoToolSettings::switchBack && s_switched)
            RestoreSlot(env, inv);
        if (block) env->DeleteLocalRef(block);
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(world);
        env->DeleteLocalRef(mop);
        Sleep(2);
        return;
    }

    int blockId = ((Block*)block)->GetID(env);
    JniOk(env);

    int cur = inv->GetSlot(env);
    JniOk(env);
    if (cur < 0 || cur > 8) cur = 0;

    jobject curStack = inv->GetStackInSlot(cur, env);
    float bestScore = ScoreStack(env, curStack, block, blockId);
    int best = cur;
    if (curStack) env->DeleteLocalRef(curStack);

    for (int i = 0; i < 9; i++) {
        if (i == cur) continue;
        jobject st = inv->GetStackInSlot(i, env);
        JniOk(env);
        float s = ScoreStack(env, st, block, blockId);
        if (st) env->DeleteLocalRef(st);
        if (s > bestScore + 0.001f) {
            bestScore = s;
            best = i;
        }
    }

    if (bestScore <= 1.05f) {
        if (AutoToolSettings::switchBack && s_switched)
            RestoreSlot(env, inv);
    } else if (best != cur) {
        ULONGLONG now = GetTickCount64();
        if (now - s_lastSwitch >= (ULONGLONG)(AutoToolSettings::delayMs < 0 ? 0 : AutoToolSettings::delayMs)) {
            if (!s_switched)
                s_savedSlot = cur;
            inv->SetSlot(best, env);
            JniOk(env);
            s_switched = true;
            s_lastSwitch = now;
        }
    }

    env->DeleteLocalRef(block);
    env->DeleteLocalRef(invObj);
    env->DeleteLocalRef(playerObj);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mop);
    Sleep(1);
}
