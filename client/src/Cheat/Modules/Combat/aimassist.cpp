#include "pch.h"
#include "AimAssist.h"

#include "SwordCheck.h"
#include "clicker.h"
#include "../Misc/Friends.h"
#include "AntiBot.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/MovingObjectPosition.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Mapper.h"
#include "../../Hack.h"
#include "../Misc/Overlay.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

#define AA_MAXV 80
#define AA_MINV 50
#define AA_MINF 0.10f
#define AA_MAXF 0.15f

namespace {
    struct Target {
        int entityId = -1;
        double distance = 0.0;
        float yaw = 0.f;
        float pitch = 0.f;
        int hurtTime = 0;
    };

    struct AAHorizontalBound {
        double x;
        double z;
    };

    Target  g_lastTarget{};
    float   g_maxAddedH = 0.f;
    float   g_addedH = 0.f;
    bool    g_cycledH = true;
    bool    g_upH = true;
    float   g_randomSpeed = 5.f;
    int     g_nextRandomSpeed = 0;
    bool    g_keepOnTargetKeybindPressed = false;

    static float RandomFloat(float mn, float mx) {
        return mn + (mx - mn) * ((float)rand() / (float)RAND_MAX);
    }

    static int RandomInt(int mn, int mx) {
        if (mx <= mn) return mn;
        return mn + rand() % ((mx + 1) - mn);
    }

    static double RadToDeg(double x) {
        return x * 180.0 / 3.14159265359;
    }

    static void Direction3D(double* to, double* from, double* output) {
        double dx = to[0] - from[0];
        double dy = to[1] - from[1];
        double dz = to[2] - from[2];
        output[0] = RadToDeg(atan2(dz, dx)) - 90.0;
        double horiz = sqrt(dx * dx + dz * dz);
        output[1] = horiz > 1e-6 ? RadToDeg(-atan(dy / horiz)) : 0.0;
    }

    static float AngleTo180(float f) {
        f = fmodf(f, 360.f);
        if (f >= 180.f) f -= 360.f;
        if (f < -180.f) f += 360.f;
        return f;
    }

    static bool Exhaust() {
        return AimAssistSettings::currentMode == 1;
    }

    static void UpdateRandomSpeed(bool success) {
        if (!success) return;
        if (g_nextRandomSpeed > 0) {
            g_nextRandomSpeed--;
            return;
        }
        const float speed = AimAssistSettings::speed;
        if (Exhaust() && RandomInt(0, 100) < 50)
            g_randomSpeed = RandomFloat(speed * 0.25f, speed * 0.5f);
        else
            g_randomSpeed = RandomFloat(speed * 0.5f, speed);
        g_nextRandomSpeed = RandomInt(400, 750);
    }

    static double Dist2D(Player* a, Player* b, JNIEnv* env) {
        Vec3D pa = a->GetPos(env);
        Vec3D pb = b->GetPos(env);
        double dx = pa.x - pb.x;
        double dz = pa.z - pb.z;
        return sqrt(dx * dx + dz * dz);
    }

    static bool IsNaked(Player* player, JNIEnv* env) {
        jobject invObj = player->GetInventoryPlayer(env);
        if (!invObj) return true;
        auto* inv = reinterpret_cast<InventoryPlayer*>(invObj);
        for (int i = 36; i < 40; i++) {
            jobject stack = inv->GetStackInSlot(i, env);
            if (stack) {
                env->DeleteLocalRef(stack);
                return false;
            }
        }
        return true;
    }

    static void GetRotationNeeded(Player* local, Player* target, float* rotBuff, JNIEnv* env) {
        if (!rotBuff) return;

        Vec3D me = local->GetPos(env);
        Vec3D them = target->GetPos(env);

        double thePlayerPos[3]{ me.x, me.y, me.z };
        double playerPos[3]{ them.x, them.y + 1.62, them.z };

        if (AimAssistSettings::multipoint > 0) {
            double width = RandomFloat(0.25f, 0.35f) * (AimAssistSettings::multipoint / 100.f);
            AAHorizontalBound bounds[4] = {
                { playerPos[0] - width, playerPos[2] - width },
                { playerPos[0] - width, playerPos[2] + width },
                { playerPos[0] + width, playerPos[2] - width },
                { playerPos[0] + width, playerPos[2] + width }
            };
            double selectedBoundDist = 0;
            int selectedBoundIndex = -1;
            for (int i = 0; i < 4; i++) {
                double cBoundDist = sqrt(pow(bounds[i].x - thePlayerPos[0], 2) + pow(bounds[i].z - thePlayerPos[2], 2));
                if (!selectedBoundDist || selectedBoundDist > cBoundDist) {
                    selectedBoundDist = cBoundDist;
                    selectedBoundIndex = i;
                }
            }
            if (selectedBoundIndex != -1) {
                playerPos[0] = bounds[selectedBoundIndex].x;
                playerPos[2] = bounds[selectedBoundIndex].z;
            }
        }

        double directions[2];
        Direction3D(playerPos, thePlayerPos, directions);
        rotBuff[0] = AngleTo180(fmodf((float)directions[0], 360.f) - local->GetRotationYaw(env));
        rotBuff[1] = AngleTo180((float)directions[1] - local->GetRotationPitch(env));
    }

    static bool IsWorseThanTarget(const Target& target, double distance, float yaw, int hurtTime) {
        if (target.entityId == -1) return false;
        switch (AimAssistSettings::priority) {
        case 0: return distance > target.distance;
        case 1: return abs(yaw) > abs(target.yaw);
        case 2:
            if (target.hurtTime == 0 && hurtTime > 0) return true;
            if (target.hurtTime > 0 && hurtTime == 0) return false;
            return distance > target.distance;
        default: return distance > target.distance;
        }
    }

    static Player* FindById(const std::vector<Player*>& list, int id, JNIEnv* env) {
        for (Player* p : list) {
            if (p && p->GetEntityId(env) == id) return p;
        }
        return nullptr;
    }

    static Target GetTarget(World* world, Player* local, JNIEnv* env) {
        Target target{ -1, 0.0, 0.f, 0.f, 0 };
        auto players = world->GetPlayerEntities(env);
        const int localId = local->GetEntityId(env);

        if (AimAssistSettings::keepOnTarget && g_lastTarget.entityId != -1) {
            Player* current = FindById(players, g_lastTarget.entityId, env);
            if (current && current->GetEntityId(env) != localId) {
                if (!FriendsSettings::IsFriend(env, current) && !AntiBot_IsBot(env, (jobject)current)) {
                    double distance = Dist2D(local, current, env);
                    if (distance > 6.0) distance = 6.0;
                    float rotations[2]{ 0.f, 0.f };
                    GetRotationNeeded(local, current, rotations, env);
                    return { current->GetEntityId(env), distance, rotations[0], rotations[1], current->GetHurtTime(env) };
                }
            }
        }

        if (!AimAssistSettings::targetPlayers)
            return target;

        for (Player* player : players) {
            if (!player) continue;
            if (player->GetEntityId(env) == localId) continue;

            if (FriendsSettings::IsFriend(env, player)) continue;
            if (AntiBot_IsBot(env, (jobject)player)) continue;
            if (player->IsDead(env)) continue;
            if (!AimAssistSettings::allowInvisible && player->IsInvisible(env)) continue;
            if (!AimAssistSettings::allowNaked && IsNaked(player, env)) continue;

            double dist = Dist2D(local, player, env);
            float rotations[2]{ 0.f, 0.f };
            GetRotationNeeded(local, player, rotations, env);
            float yaw = rotations[0];
            float pitch = rotations[1];
            int ht = player->GetHurtTime(env);

            const float minDistEff = (AimAssistSettings::priority == 2) ? 0.f : AimAssistSettings::distanceMin;
            const float maxDistEff = (AimAssistSettings::priority == 2) ? 3.3f : AimAssistSettings::distanceMax;
            if (dist > maxDistEff || dist < minDistEff) continue;
            if (abs(yaw) > AimAssistSettings::fovMax / 2.f || abs(yaw) < AimAssistSettings::fovMin / 2.f) continue;
            if (AimAssistSettings::priority == 2 && ht > 0) continue;
            if (IsWorseThanTarget(target, dist, yaw, ht)) continue;

            target = { player->GetEntityId(env), dist, yaw, pitch, ht };
        }
        return target;
    }
}

void AimAssist::Run(JNIEnv* env) {
    if (!env) return;

    if (env->PushLocalFrame(256) != JNI_OK)
        return;

    auto pop = [env]() { env->PopLocalFrame(nullptr); };

    if (!enabled) {
        g_lastTarget = {};
        pop();
        return;
    }

    if (Overlay::isOpen) {
        pop();
        return;
    }

    if (AimAssistSettings::keepOnTargetKeybind != 0) {
        const bool isPressed = (GetAsyncKeyState(AimAssistSettings::keepOnTargetKeybind) & 0x8000) != 0;
        if (isPressed && !g_keepOnTargetKeybindPressed) {
            AimAssistSettings::keepOnTarget = !AimAssistSettings::keepOnTarget;
            g_keepOnTargetKeybindPressed = true;
        }
        else if (!isPressed && g_keepOnTargetKeybindPressed) {
            g_keepOnTargetKeybindPressed = false;
        }
    }

    jobject lpObj = Minecraft::GetThePlayer(env);
    jobject worldObj = Minecraft::GetTheWorld(env);
    jobject gsObj = Minecraft::GetGameSettings(env);
    jobject mopObj = Minecraft::GetObjectMouseOver(env);
    if (!lpObj || !worldObj) {
        UpdateRandomSpeed(false);
        pop();
        return;
    }

    auto* local = reinterpret_cast<Player*>(lpObj);
    auto* world = reinterpret_cast<World*>(worldObj);
    auto* gs = reinterpret_cast<GameSettings*>(gsObj);
    auto* mop = reinterpret_cast<MovingObjectPosition*>(mopObj);

    const bool clickOk = !AimAssistSettings::requireClick
        || g_physicalDown.load()
        || (GetAsyncKeyState(VK_LBUTTON) & 0x8000);
    const bool weaponOk = !AimAssistSettings::weaponsOnly || SC_IsHoldingSword(env);
    const bool blockOk = !AimAssistSettings::breakBlock || !mop || !mop->IsAimingBlock(env);

    if (!clickOk || !weaponOk || !blockOk) {
        if (g_lastTarget.entityId != -1)
            g_lastTarget = {};
        UpdateRandomSpeed(false);
        pop();
        return;
    }

    const Target target = GetTarget(world, local, env);
    if (target.entityId == -1) {
        UpdateRandomSpeed(false);
        pop();
        return;
    }

    const double distForSpeed = (g_lastTarget.entityId == target.entityId && g_lastTarget.distance > 0.05)
        ? g_lastTarget.distance
        : (target.distance > 0.05 ? target.distance : 1.0);
    float deltaYaw = target.yaw * (g_randomSpeed / 50.f * (float)sqrt(distForSpeed));
    deltaYaw = (target.yaw < 0.f ? -1.f : 1.f) * std::clamp(abs(deltaYaw), 0.f, abs(target.yaw));

    float var4 = 0.15f * 8.f;
    if (gs) {
        const float var3 = gs->GetMouseSensitivity(env) * 0.6f + 0.2f;
        var4 = var3 * var3 * var3 * 8.f;
        if (var4 < 1e-4f) var4 = 1e-4f;
    }

    const int deltaX = static_cast<int>(round(deltaYaw / var4));
    if (deltaX != 0)
        deltaYaw = deltaX * var4;

    float deltaPitch = 0.f;
    if (abs(deltaYaw) >= 0.f) {
        if (g_cycledH) {
            g_maxAddedH = RandomFloat((float)AA_MINV, (float)AA_MAXV);
            g_cycledH = false;
            g_addedH = 0.f;
        }

        deltaPitch = g_upH ? abs(RandomFloat(AA_MINF, AA_MAXF)) : -abs(RandomFloat(AA_MINF, AA_MAXF));
        deltaPitch = deltaPitch * abs(deltaYaw);
        g_addedH += deltaPitch;

        if (abs(g_addedH) > g_maxAddedH) {
            g_upH = !g_upH;
            g_cycledH = !g_cycledH;
        }

        const int deltaY = static_cast<int>(round(deltaPitch / var4));
        if (deltaY != 0)
            deltaPitch = deltaY * var4;
    }

    local->SetAngles(deltaYaw, deltaPitch, env);
    g_lastTarget = target;
    UpdateRandomSpeed(true);
    pop();
}

void AimAssist::OnRender(JNIEnv* env) {
    Run(env);
}
