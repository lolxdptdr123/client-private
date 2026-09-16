#include "pch.h"
#include "BlockIn.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Block.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Cheat/Hack.h"
#include "../../../Cheat/Modules/Settings.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int s_savedSlot = -1;
static ULONGLONG s_nextClick = 0;
static int s_aimTx = 0, s_aimTy = 0, s_aimTz = 0;
static bool s_hasAim = false;

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

static float Wrap180(float a) {
    while (a > 180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
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

static bool IsAirAt(JNIEnv* env, jobject world, int x, int y, int z) {
    jobject block = GetBlockAt(env, world, x, y, z);
    if (!block) return true;
    bool air = ((Block*)block)->IsAir(env);
    JniOk(env);
    env->DeleteLocalRef(block);
    return air;
}

static bool IsSolidAt(JNIEnv* env, jobject world, int x, int y, int z) {
    return !IsAirAt(env, world, x, y, z);
}

struct PlaceSpot {
    int sx, sy, sz;
    float hitX, hitY, hitZ;
    int tx, ty, tz;
    bool ok = false;
};

static const int kNOff[6][3] = {
    { 0, -1, 0 }, { 0, 1, 0 },
    { 0, 0, -1 }, { 0, 0, 1 },
    { -1, 0, 0 }, { 1, 0, 0 }
};
static const float kFace[6][3] = {
    { 0.5f, 1.0f, 0.5f },
    { 0.5f, 0.0f, 0.5f },
    { 0.5f, 0.5f, 1.0f },
    { 0.5f, 0.5f, 0.0f },
    { 1.0f, 0.5f, 0.5f },
    { 0.0f, 0.5f, 0.5f }
};

static PlaceSpot FindPlaceAgainst(JNIEnv* env, jobject world, int tx, int ty, int tz) {
    PlaceSpot s{};
    if (!IsAirAt(env, world, tx, ty, tz))
        return s;
    for (int i = 0; i < 6; i++) {
        int sx = tx + kNOff[i][0];
        int sy = ty + kNOff[i][1];
        int sz = tz + kNOff[i][2];
        if (!IsSolidAt(env, world, sx, sy, sz))
            continue;
        s.sx = sx; s.sy = sy; s.sz = sz;
        s.hitX = (float)sx + kFace[i][0];
        s.hitY = (float)sy + kFace[i][1];
        s.hitZ = (float)sz + kFace[i][2];
        s.tx = tx; s.ty = ty; s.tz = tz;
        s.ok = true;
        return s;
    }
    return s;
}

static void LookAt(float eyeX, float eyeY, float eyeZ, float hx, float hy, float hz, float& yaw, float& pitch) {
    double dx = (double)hx - eyeX;
    double dy = (double)hy - eyeY;
    double dz = (double)hz - eyeZ;
    double dist = sqrt(dx * dx + dz * dz);
    yaw = (float)(atan2(dz, dx) * 180.0 / M_PI) - 90.f;
    pitch = (float)(-(atan2(dy, dist) * 180.0 / M_PI));
    if (pitch > 90.f) pitch = 90.f;
    if (pitch < -90.f) pitch = -90.f;
}

static void SetLook(Player* p, JNIEnv* env, float yaw, float pitch) {
    p->SetRotationYaw(yaw, env);
    p->SetRotationPitch(pitch, env);
    p->SetPrevRotationYaw(yaw, env);
    p->SetPrevRotationPitch(pitch, env);
    JniOk(env);
}

static int FindHotbarBlock(JNIEnv* env, InventoryPlayer* inv) {
    if (!inv) return -1;
    for (int i = 0; i < 9; i++) {
        jobject st = inv->GetStackInSlot(i, env);
        JniOk(env);
        bool ok = st && ((ItemStack*)st)->IsBlock(env);
        JniOk(env);
        if (st) env->DeleteLocalRef(st);
        if (ok) return i;
    }
    return -1;
}

static void RestoreSlot(JNIEnv* env, Player* player) {
    if (s_savedSlot < 0 || !player) return;
    jobject invObj = player->GetInventoryPlayer(env);
    if (invObj) {
        ((InventoryPlayer*)invObj)->SetSlot(s_savedSlot, env);
        JniOk(env);
        env->DeleteLocalRef(invObj);
    }
    s_savedSlot = -1;
}

void BlockIn::Run(JNIEnv* env) {
    if (!IsLunar() || !env) {
        Sleep(40);
        return;
    }

    if (!enabled || Overlay::isOpen) {
        jobject p = Minecraft::GetThePlayer(env);
        if (p) {
            RestoreSlot(env, (Player*)p);
            env->DeleteLocalRef(p);
        }
        s_hasAim = false;
        Sleep(15);
        return;
    }

    jobject screen = Minecraft::GetCurrentScreen(env);
    JniOk(env);
    if (screen) {
        env->DeleteLocalRef(screen);
        Sleep(10);
        return;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject world = Minecraft::GetTheWorld(env);
    JniOk(env);
    if (!playerObj || !world) {
        if (playerObj) env->DeleteLocalRef(playerObj);
        if (world) env->DeleteLocalRef(world);
        Sleep(8);
        return;
    }

    auto* player = (Player*)playerObj;
    if (BlockInSettings::onlyOnGround && !player->IsOnGround(env)) {
        JniOk(env);
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(world);
        Sleep(5);
        return;
    }

    jobject invObj = player->GetInventoryPlayer(env);
    JniOk(env);
    if (!invObj) {
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(world);
        Sleep(8);
        return;
    }
    auto* inv = (InventoryPlayer*)invObj;

    jobject held = player->GetHeldItem(env);
    JniOk(env);
    bool holdingBlock = held && ((ItemStack*)held)->IsBlock(env);
    JniOk(env);
    if (held) env->DeleteLocalRef(held);

    if (!holdingBlock) {
        int slot = FindHotbarBlock(env, inv);
        if (slot < 0) {
            env->DeleteLocalRef(invObj);
            env->DeleteLocalRef(playerObj);
            env->DeleteLocalRef(world);
            Sleep(20);
            return;
        }
        int cur = inv->GetSlot(env);
        JniOk(env);
        if (s_savedSlot < 0) s_savedSlot = cur;
        if (cur != slot) {
            inv->SetSlot(slot, env);
            JniOk(env);
        }
    }

    Vec3D pos = player->GetPos(env);
    JniOk(env);
    int px = Floor(pos.x);
    int py = Floor(pos.y);
    int pz = Floor(pos.z);

    static const int kWalls[8][3] = {
        { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
        { 1, 1, 0 }, { -1, 1, 0 }, { 0, 1, 1 }, { 0, 1, -1 }
    };

    float eyeY = (float)pos.y + player->GetEyeHeight(env);
    JniOk(env);
    float cy = player->GetRotationYaw(env);
    float cp = player->GetRotationPitch(env);
    JniOk(env);

    PlaceSpot best{};
    float bestAng = 1e9f;
    for (int i = 0; i < 8; i++) {
        int tx = px + kWalls[i][0];
        int ty = py + kWalls[i][1];
        int tz = pz + kWalls[i][2];
        PlaceSpot spot = FindPlaceAgainst(env, world, tx, ty, tz);
        if (!spot.ok) continue;
        float tyaw, tpitch;
        LookAt((float)pos.x, eyeY, (float)pos.z, spot.hitX, spot.hitY, spot.hitZ, tyaw, tpitch);
        float ang = fabsf(Wrap180(tyaw - cy)) + fabsf(tpitch - cp);
        if (ang < bestAng) {
            bestAng = ang;
            best = spot;
        }
    }

    if (!best.ok) {
        RestoreSlot(env, player);
        s_hasAim = false;
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(world);
        Sleep(15);
        return;
    }

    float tyaw, tpitch;
    LookAt((float)pos.x, eyeY, (float)pos.z, best.hitX, best.hitY, best.hitZ, tyaw, tpitch);

    float spd = std::clamp(BlockInSettings::speed, 1.f, 10.f);
    float t = 0.18f + spd * 0.082f;
    if (t > 1.f) t = 1.f;
    float ny = cy + Wrap180(tyaw - cy) * t;
    float np = cp + (tpitch - cp) * t;
    if (np > 90.f) np = 90.f;
    if (np < -90.f) np = -90.f;
    SetLook(player, env, ny, np);

    float remain = fabsf(Wrap180(tyaw - ny)) + fabsf(tpitch - np);
    float aimOk = (std::max)(2.5f, 14.f - spd);
    ULONGLONG now = GetTickCount64();
    int clickMs = (int)((11.f - spd) * 22.f) + 20;

    bool same = s_hasAim && s_aimTx == best.tx && s_aimTy == best.ty && s_aimTz == best.tz;
    s_aimTx = best.tx; s_aimTy = best.ty; s_aimTz = best.tz;
    s_hasAim = true;

    if (remain <= aimOk && now >= s_nextClick) {
        Minecraft::SetRightClickDelayTimer(env, 0);
        Minecraft::RightClickMouse(env);
        JniOk(env);
        s_nextClick = now + (ULONGLONG)clickMs;
        s_hasAim = false;
    } else if (!same) {
        s_nextClick = now + (ULONGLONG)(clickMs / 3);
    }

    env->DeleteLocalRef(invObj);
    env->DeleteLocalRef(playerObj);
    env->DeleteLocalRef(world);
    Sleep(1);
}
