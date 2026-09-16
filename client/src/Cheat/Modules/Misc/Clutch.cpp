#include "pch.h"
#include "Clutch.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Block.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Cheat/Hack.h"
#include "../../../Cheat/Modules/Settings.h"

#include <algorithm>
#include <cmath>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum class ClutchPhase { Idle, Aim, SnapWait, SnapBack };

static ClutchPhase s_phase = ClutchPhase::Idle;
static int s_savedSlot = -1;
static float s_savedYaw = 0.f, s_savedPitch = 0.f;
static float s_snapFromYaw = 0.f, s_snapFromPitch = 0.f;
static ULONGLONG s_nextClick = 0;
static ULONGLONG s_phaseAt = 0;
static bool s_wasHoldingBlock = false;
static std::mt19937 g_rng{ std::random_device{}() };

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

static float Rand01() {
    std::uniform_real_distribution<float> d(0.f, 1.f);
    return d(g_rng);
}

static float RandSigned() {
    return Rand01() * 2.f - 1.f;
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

static int CountAirBelow(JNIEnv* env, jobject world, int x, int y, int z) {
    int n = 0;
    for (int yy = y - 1; yy >= y - 16 && yy >= 0; yy--) {
        if (!IsAirAt(env, world, x, yy, z)) break;
        n++;
    }
    return n;
}

static int MaxAirBelow(JNIEnv* env, jobject world, const Vec3D& pos) {
    int py = Floor(pos.y);
    int best = CountAirBelow(env, world, Floor(pos.x), py, Floor(pos.z));
    const double o = 0.31;
    int corners[4][2] = {
        { Floor(pos.x - o), Floor(pos.z - o) },
        { Floor(pos.x + o), Floor(pos.z - o) },
        { Floor(pos.x - o), Floor(pos.z + o) },
        { Floor(pos.x + o), Floor(pos.z + o) }
    };
    for (auto& c : corners)
        best = (std::max)(best, CountAirBelow(env, world, c[0], py, c[1]));
    return best;
}

struct PlaceSpot {
    int sx, sy, sz;
    float hitX, hitY, hitZ;
    int tx, ty, tz;
    int cost = 99;
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

static void ApplyMultipoint(float eyeX, float eyeY, float eyeZ, int face, int sx, int sy, int sz, float& hx, float& hy, float& hz) {
    const float pad = 0.08f;
    float x0 = (float)sx + pad, x1 = (float)sx + 1.f - pad;
    float y0 = (float)sy + pad, y1 = (float)sy + 1.f - pad;
    float z0 = (float)sz + pad, z1 = (float)sz + 1.f - pad;
    hx = std::clamp(eyeX, x0, x1);
    hy = std::clamp(eyeY, y0, y1);
    hz = std::clamp(eyeZ, z0, z1);
    if (face == 0) hy = (float)sy + 1.f;
    if (face == 1) hy = (float)sy;
    if (face == 2) hz = (float)sz + 1.f;
    if (face == 3) hz = (float)sz;
    if (face == 4) hx = (float)sx + 1.f;
    if (face == 5) hx = (float)sx;
}

static PlaceSpot FindPlaceAgainst(JNIEnv* env, jobject world, int tx, int ty, int tz, bool sidewaysOnly,
    float eyeX, float eyeY, float eyeZ, bool multipoint, float jitter)
{
    PlaceSpot s{};
    if (!IsAirAt(env, world, tx, ty, tz))
        return s;
    int begin = sidewaysOnly ? 2 : 0;
    int end = sidewaysOnly ? 6 : 6;
    for (int i = begin; i < end; i++) {
        int sx = tx + kNOff[i][0];
        int sy = ty + kNOff[i][1];
        int sz = tz + kNOff[i][2];
        if (!IsSolidAt(env, world, sx, sy, sz))
            continue;
        s.sx = sx; s.sy = sy; s.sz = sz;
        s.hitX = (float)sx + kFace[i][0];
        s.hitY = (float)sy + kFace[i][1];
        s.hitZ = (float)sz + kFace[i][2];
        if (multipoint)
            ApplyMultipoint(eyeX, eyeY, eyeZ, i, sx, sy, sz, s.hitX, s.hitY, s.hitZ);
        s.hitX += RandSigned() * 0.12f * jitter;
        s.hitY += RandSigned() * 0.08f * jitter;
        s.hitZ += RandSigned() * 0.12f * jitter;
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

static float AngleTo(float cy, float cp, float ty, float tp) {
    return fabsf(Wrap180(ty - cy)) + fabsf(tp - cp);
}

static float FovTo(float cy, float cp, float ty, float tp) {
    float dy = Wrap180(ty - cy);
    float dp = tp - cp;
    return sqrtf(dy * dy + dp * dp);
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

static PlaceSpot FindBestSpot(JNIEnv* env, jobject world, int px, int py, int pz,
    float eyeX, float eyeY, float eyeZ, float cy, float cp)
{
    PlaceSpot best{};
    float bestScore = 1e9f;
    int range = (std::max)(1, ClutchSettings::range);
    float jitter = std::clamp(ClutchSettings::randomization, 0.f, 100.f) / 100.f;
    float fov = ClutchSettings::fov * (1.f + RandSigned() * 0.08f * jitter);
    if (fov < 15.f) fov = 15.f;

    int feetY = py - 1;
    for (int dy = 0; dy >= -range; dy--) {
        for (int dx = -range; dx <= range; dx++) {
            for (int dz = -range; dz <= range; dz++) {
                int cost = abs(dx) + abs(dz) + abs(dy);
                if (cost < 1 || cost > range) continue;
                int tx = px + dx;
                int ty = feetY + dy;
                int tz = pz + dz;
                PlaceSpot spot = FindPlaceAgainst(env, world, tx, ty, tz, ClutchSettings::onlySideways,
                    eyeX, eyeY, eyeZ, ClutchSettings::multipoint, jitter);
                if (!spot.ok) continue;
                spot.cost = cost;
                float tyaw, tpitch;
                LookAt(eyeX, eyeY, eyeZ, spot.hitX, spot.hitY, spot.hitZ, tyaw, tpitch);
                float fovAng = FovTo(cy, cp, tyaw, tpitch);
                if (fovAng > fov * 0.5f) continue;
                float score = (float)cost * 18.f + fovAng;
                if (tx == px && tz == pz) score -= 8.f;
                if (score < bestScore) {
                    bestScore = score;
                    best = spot;
                }
            }
        }
    }
    return best;
}

static void ResetSession(JNIEnv* env, Player* player, bool restoreLook) {
    if (restoreLook && player && s_phase != ClutchPhase::Idle)
        SetLook(player, env, s_savedYaw, s_savedPitch);
    RestoreSlot(env, player);
    s_phase = ClutchPhase::Idle;
    s_wasHoldingBlock = false;
}

void Clutch::Run(JNIEnv* env) {
    if (!IsLunar() || !env) {
        Sleep(40);
        return;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    JniOk(env);
    auto* player = (Player*)playerObj;

    if (!enabled || Overlay::isOpen) {
        if (player) ResetSession(env, player, true);
        if (playerObj) env->DeleteLocalRef(playerObj);
        Sleep(15);
        return;
    }

    jobject screen = Minecraft::GetCurrentScreen(env);
    JniOk(env);
    if (screen) {
        env->DeleteLocalRef(screen);
        if (playerObj) env->DeleteLocalRef(playerObj);
        Sleep(10);
        return;
    }

    jobject world = Minecraft::GetTheWorld(env);
    JniOk(env);
    if (!player || !world) {
        if (playerObj) env->DeleteLocalRef(playerObj);
        if (world) env->DeleteLocalRef(world);
        Sleep(8);
        return;
    }

    bool onGround = player->IsOnGround(env);
    JniOk(env);
    Vec3D pos = player->GetPos(env);
    JniOk(env);
    int airBelow = MaxAirBelow(env, world, pos);

    bool jumpDown = false;
    jobject gs = Minecraft::GetGameSettings(env);
    if (gs) {
        jobject jmp = ((GameSettings*)gs)->GetKeyBindJump(env);
        if (jmp) {
            jumpDown = ((KeyBinding*)jmp)->IsPhysDown(env);
            env->DeleteLocalRef(jmp);
        }
        env->DeleteLocalRef(gs);
        JniOk(env);
    }

    ULONGLONG now = GetTickCount64();

    if (s_phase == ClutchPhase::SnapWait || s_phase == ClutchPhase::SnapBack) {
        if (ClutchSettings::keepJumpDir && jumpDown) {
            SetLook(player, env, s_savedYaw, s_savedPitch);
            RestoreSlot(env, player);
            s_phase = ClutchPhase::Idle;
            if (ClutchSettings::disableAfter)
                enabled = false;
            env->DeleteLocalRef(playerObj);
            env->DeleteLocalRef(world);
            Sleep(5);
            return;
        }
        if (s_phase == ClutchPhase::SnapWait) {
            if (now >= s_phaseAt) {
                s_snapFromYaw = player->GetRotationYaw(env);
                s_snapFromPitch = player->GetRotationPitch(env);
                s_phase = ClutchPhase::SnapBack;
                s_phaseAt = now;
            }
            env->DeleteLocalRef(playerObj);
            env->DeleteLocalRef(world);
            Sleep(1);
            return;
        }
        float dur = (float)(std::max)(1, ClutchSettings::snapDuration);
        float t = (float)(now - s_phaseAt) / dur;
        if (t >= 1.f) {
            SetLook(player, env, s_savedYaw, s_savedPitch);
            RestoreSlot(env, player);
            s_phase = ClutchPhase::Idle;
            if (ClutchSettings::disableAfter)
                enabled = false;
        } else {
            float ny = s_snapFromYaw + Wrap180(s_savedYaw - s_snapFromYaw) * t;
            float np = s_snapFromPitch + (s_savedPitch - s_snapFromPitch) * t;
            SetLook(player, env, ny, np);
        }
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(world);
        Sleep(1);
        return;
    }

    bool condOk = true;
    if (ClutchSettings::midAir && onGround && s_phase == ClutchPhase::Idle)
        condOk = false;
    if (ClutchSettings::midAir && onGround && s_phase == ClutchPhase::Aim) {
        s_phase = ClutchPhase::SnapWait;
        s_phaseAt = now + (ULONGLONG)(std::max)(0, ClutchSettings::snapDelay);
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(world);
        Sleep(1);
        return;
    }
    if (ClutchSettings::onHurt && player->GetHurtTime(env) <= 0 && s_phase == ClutchPhase::Idle)
        condOk = false;
    JniOk(env);
    if (ClutchSettings::backwards && player->GetMoveForward(env) >= -0.01f && s_phase == ClutchPhase::Idle)
        condOk = false;
    JniOk(env);
    if (airBelow < ClutchSettings::minHeight && s_phase == ClutchPhase::Idle)
        condOk = false;

    if (!condOk) {
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(world);
        Sleep(8);
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

    if (s_phase == ClutchPhase::Idle)
        s_wasHoldingBlock = holdingBlock;

    const int sel = ClutchSettings::selectBlocks;
    if (!holdingBlock) {
        bool canSwitch = (sel == 2) || (sel == 1 && s_wasHoldingBlock);
        if (!canSwitch) {
            env->DeleteLocalRef(invObj);
            env->DeleteLocalRef(playerObj);
            env->DeleteLocalRef(world);
            Sleep(12);
            return;
        }
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

    double mx = player->GetMotionX(env);
    double mz = player->GetMotionZ(env);
    JniOk(env);
    int px = Floor(pos.x + mx * 0.35);
    int py = Floor(pos.y);
    int pz = Floor(pos.z + mz * 0.35);

    float eyeY = (float)pos.y + player->GetEyeHeight(env);
    float cy = player->GetRotationYaw(env);
    float cp = player->GetRotationPitch(env);
    JniOk(env);

    if (s_phase == ClutchPhase::Idle) {
        s_savedYaw = cy;
        s_savedPitch = cp;
        s_phase = ClutchPhase::Aim;
    }

    PlaceSpot best = FindBestSpot(env, world, px, py, pz, (float)pos.x, eyeY, (float)pos.z, cy, cp);
    if (!best.ok) {
        if (s_phase == ClutchPhase::Aim && (onGround || airBelow < 1)) {
            s_phase = ClutchPhase::SnapWait;
            s_phaseAt = now + (ULONGLONG)(std::max)(0, ClutchSettings::snapDelay);
        }
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(world);
        Sleep(4);
        return;
    }

    float tyaw, tpitch;
    LookAt((float)pos.x, eyeY, (float)pos.z, best.hitX, best.hitY, best.hitZ, tyaw, tpitch);
    float remain = AngleTo(cy, cp, tyaw, tpitch);
    float ang = (std::max)(remain, 1.f);
    float mix = std::clamp(ClutchSettings::accelStrength, 0.f, 100.f) / 100.f;
    float accelPart = ClutchSettings::acceleration * std::clamp(ang / 70.f, 0.15f, 1.6f);
    float rot = ClutchSettings::baseSpeed * (1.f - mix) + accelPart * mix;
    rot *= 1.f + RandSigned() * 0.04f * (ClutchSettings::randomization / 100.f);
    if (rot < 1.2f) rot = 1.2f;
    float t = std::clamp(rot / ang, 0.22f, 1.f);
    float ny = cy + Wrap180(tyaw - cy) * t;
    float np = cp + (tpitch - cp) * t;
    if (np > 90.f) np = 90.f;
    if (np < -90.f) np = -90.f;
    SetLook(player, env, ny, np);

    remain = AngleTo(ny, np, tyaw, tpitch);
    float cps = std::clamp(ClutchSettings::clickSpeed, 1.f, 24.f);
    cps *= 1.f - Rand01() * 0.08f * (ClutchSettings::randomization / 100.f);
    int clickMs = (int)(1000.f / cps);
    if (clickMs < 18) clickMs = 18;

    if (remain <= 14.f && now >= s_nextClick) {
        Minecraft::SetRightClickDelayTimer(env, 0);
        Minecraft::RightClickMouse(env);
        JniOk(env);
        s_nextClick = now + (ULONGLONG)clickMs;
    }

    env->DeleteLocalRef(invObj);
    env->DeleteLocalRef(playerObj);
    env->DeleteLocalRef(world);
    Sleep(1);
}
