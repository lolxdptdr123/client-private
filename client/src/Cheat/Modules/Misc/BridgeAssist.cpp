#include "pch.h"
#include "BridgeAssist.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Block.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Cheat/Hack.h"
#include "../../../Cheat/Modules/Settings.h"

#include <algorithm>
#include <cmath>

static ULONGLONG s_keepSneakUntil = 0;
static bool s_forced = false;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static bool IsLunar() {
    return g_GameLauncher == LAUNCHER_LUNAR
        && (g_GameVersion == LUNAR_1_7_10 || g_GameVersion == LUNAR_1_8_9);
}

static int Floor(double v) {
    int i = (int)v;
    return v < (double)i ? i - 1 : i;
}

static jobject GetBlockAt(JNIEnv* env, jobject world, int x, int y, int z) {
    if (!env || !world) return nullptr;
    jclass worldC = env->GetObjectClass(world);
    if (!worldC) return nullptr;

    std::string blockSig = Mapper::Get("net/minecraft/block/Block", 2);
    std::string getBlock = Mapper::Get("getBlock");
    if (getBlock.empty()) getBlock = "getBlock";

    jobject result = nullptr;
    if (!blockSig.empty()) {
        jmethodID gb = env->GetMethodID(worldC, getBlock.c_str(), ("(III)" + blockSig).c_str());
        JniOk(env);
        if (gb) {
            result = env->CallObjectMethod(world, gb, x, y, z);
            JniOk(env);
        }
    }

    if (!result && !blockSig.empty()) {
        std::string bpName = Mapper::Get("net/minecraft/util/BlockPos");
        if (bpName.empty()) bpName = "net/minecraft/util/BlockPos";
        Klass* bpK = g_Instance->FindClass(bpName.c_str());
        if (bpK) {
            jmethodID ctor = env->GetMethodID((jclass)bpK, "<init>", "(III)V");
            JniOk(env);
            if (ctor) {
                jobject bp = env->NewObject((jclass)bpK, ctor, x, y, z);
                JniOk(env);
                if (bp) {
                    std::string ibs = Mapper::Get("net/minecraft/block/state/IBlockState");
                    if (ibs.empty()) ibs = "net/minecraft/block/state/IBlockState";
                    std::string gbsSig = "(L" + bpName + ";)L" + ibs + ";";
                    jmethodID gbs = env->GetMethodID(worldC, "getBlockState", gbsSig.c_str());
                    JniOk(env);
                    if (gbs) {
                        jobject state = env->CallObjectMethod(world, gbs, bp);
                        JniOk(env);
                        if (state) {
                            jclass stC = env->GetObjectClass(state);
                            jmethodID gb = env->GetMethodID(stC, getBlock.c_str(), ("()" + blockSig).c_str());
                            JniOk(env);
                            if (gb) {
                                result = env->CallObjectMethod(state, gb);
                                JniOk(env);
                            }
                            env->DeleteLocalRef(stC);
                            env->DeleteLocalRef(state);
                        }
                    }
                    env->DeleteLocalRef(bp);
                }
            }
        }
    }

    env->DeleteLocalRef(worldC);
    return result;
}

static bool IsAirAt(JNIEnv* env, jobject world, double x, double y, double z) {
    jobject block = GetBlockAt(env, world, Floor(x), Floor(y), Floor(z));
    if (!block) return true;
    bool air = ((Block*)block)->IsAir(env);
    JniOk(env);
    env->DeleteLocalRef(block);
    return air;
}

static bool OverEdge(JNIEnv* env, jobject world, const Vec3D& pos, float edgeOffset) {
    double hw = 0.3 - (double)edgeOffset;
    if (hw < 0.02) hw = 0.02;
    double yb = pos.y - 1.0;
    return IsAirAt(env, world, pos.x - hw, yb, pos.z - hw)
        || IsAirAt(env, world, pos.x + hw, yb, pos.z - hw)
        || IsAirAt(env, world, pos.x - hw, yb, pos.z + hw)
        || IsAirAt(env, world, pos.x + hw, yb, pos.z + hw)
        || IsAirAt(env, world, pos.x, yb, pos.z);
}

static void RestoreSneak(JNIEnv* env, KeyBinding* sneak) {
    if (!sneak) return;
    bool phys = sneak->IsPhysDown(env);
    JniOk(env);
    sneak->SetPressed(phys, env);
    JniOk(env);
    s_forced = false;
}

void BridgeAssist::Run(JNIEnv* env) {
    if (!IsLunar() || !env) {
        Sleep(40);
        return;
    }

    jobject gsObj = Minecraft::GetGameSettings(env);
    JniOk(env);
    jobject sneakObj = gsObj ? ((GameSettings*)gsObj)->GetKeyBindSneak(env) : nullptr;
    JniOk(env);
    auto* sneak = (KeyBinding*)sneakObj;

    auto dropGs = [&]() {
        if (sneakObj) env->DeleteLocalRef(sneakObj);
        if (gsObj) env->DeleteLocalRef(gsObj);
    };

    if (!enabled || Overlay::isOpen) {
        if (sneak && s_forced)
            RestoreSneak(env, sneak);
        s_keepSneakUntil = 0;
        dropGs();
        Sleep(15);
        return;
    }

    jobject screen = Minecraft::GetCurrentScreen(env);
    JniOk(env);
    if (screen) {
        if (sneak && s_forced)
            RestoreSneak(env, sneak);
        env->DeleteLocalRef(screen);
        dropGs();
        Sleep(10);
        return;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject world = Minecraft::GetTheWorld(env);
    JniOk(env);
    if (!playerObj || !world || !sneak) {
        if (playerObj) env->DeleteLocalRef(playerObj);
        if (world) env->DeleteLocalRef(world);
        dropGs();
        Sleep(8);
        return;
    }

    auto* player = (Player*)playerObj;
    bool want = true;

    if (BridgeAssistSettings::lookingDown) {
        float pitch = player->GetRotationPitch(env);
        JniOk(env);
        if (pitch < BridgeAssistSettings::pitch)
            want = false;
    }

    if (want && BridgeAssistSettings::onlyBlocks) {
        jobject held = player->GetHeldItem(env);
        JniOk(env);
        bool ok = held && ((ItemStack*)held)->IsBlock(env);
        JniOk(env);
        if (held) env->DeleteLocalRef(held);
        if (!ok) want = false;
    }

    bool onGround = player->IsOnGround(env);
    JniOk(env);
    if (want && !onGround && !BridgeAssistSettings::sneakOnJump)
        want = false;

    Vec3D pos = player->GetPos(env);
    JniOk(env);
    bool edge = want && OverEdge(env, world, pos, BridgeAssistSettings::edgeOffset);
    if (want && !onGround && BridgeAssistSettings::sneakOnJump)
        edge = true;

    ULONGLONG now = GetTickCount64();
    if (edge)
        s_keepSneakUntil = now + (ULONGLONG)(std::max)(0, BridgeAssistSettings::unsneakDelay);

    bool shouldSneak = edge || (now < s_keepSneakUntil);
    bool phys = sneak->IsPhysDown(env);
    JniOk(env);

    if (shouldSneak || phys) {
        sneak->SetPressed(true, env);
        s_forced = !phys;
    } else if (s_forced) {
        RestoreSneak(env, sneak);
    } else {
        sneak->SetPressed(false, env);
        JniOk(env);
    }

    env->DeleteLocalRef(playerObj);
    env->DeleteLocalRef(world);
    dropGs();
    Sleep(1);
}
