#include "pch.h"
#include "Module.h"

#include "Settings.h"
#include "../Hack.h"
#include "../../Helper/Communication.h"
#include "../../Helper/Utils.h"
#include "../Hooks/WSA.h"
#include "../Hooks/wglSwapBuffers.h"
#pragma region Modules includes
#include "Visuals/ArrayList.h"
#include "Visuals/Chams.h"
#include "Visuals/Esp.h"
#include "Visuals/ItemEsp.h"
#include "Visuals/PlayerEsp.h"
#include "Visuals/StorageEsp.h"
#include "Visuals/BlockEsp.h"
#include "Visuals/Nametag.h"
#include "Visuals/Tracer.h"
#include "Visuals/Trajectories.h"
#include "Visuals/Notifications.h"
#include "Visuals/Pointers.h"
#include "Visuals/Indicators.h"
#include "Visuals/NoHurtCam.h"
#include "Combat/Clicker.h"
#include "Combat/AimAssist.h"
#include "Combat/velocity.h"
#include "Combat/AutoRefill.h"
#include "Combat/Throw.h"
#include "Combat/KeepSprint.h"
#include "Combat/Criticals.h"
#include "Combat/SprintReset.h"
#include "Combat/LagRange.h"
#include "Combat/Reach.h"
#include "Combat/Blink.h"
#include "Combat/AutoRod.h"
#include "Combat/AntiBot.h"
#include "Combat/AutoBlock.h"
#include "Combat/Backtrack.h"
#include "Combat/AutoWeapon.h"
#include "Misc/Friends.h"
#include "Misc/Enemies.h"
#include "Misc/NoItemRelease.h"
#include "Misc/AntiDebuff.h"
#include "Misc/PingFix.h"
#include "Misc/RightClicker.h"
#include "Misc/BowBoost.h"
#include "Misc/FastPlace.h"
#include "Misc/FastBreak.h"
#include "Misc/AutoTool.h"
#include "Misc/ChestStealer.h"
#include "Misc/InvManager.h"
#include "Misc/BridgeAssist.h"
#include "Misc/BlockIn.h"
#include "Misc/Clutch.h"
#include "Misc/TickLocker.h"
#include "Misc/InvWalk.h"
#include "Misc/FastStop.h"
#include "Misc/NoJumpDelay.h"
#include "Misc/QuickAccel.h"
#include "Misc/SnapTap.h"
#include "Misc/Sprint.h"
#include "Misc/NoSlow.h"
#include "Misc/Strafe.h"
#include "Misc/Scroll.h"
#include "Misc/armor.h"
#include "Misc/ArmorSwitcher.h"
#include "Menu.h"
#include "Misc/Overlay.h"
#pragma endregion

#pragma region Game classes includes
#include "../../Game/Classes/Minecraft.h"
#include "../../Game/Classes/ActiveRenderInfo.h"
#include "../../Game/Classes/GuiScreen.h"
#pragma endregion

void Modules::InitializeModules()
{

    Clicker::Start();
    RightClicker::Start();
    Scroll::Start();
    Armor::Start();
    AutoRefill_Start();
    Throw_Start();
    if (g_GameVersion == LUNAR_1_8_9)
        TickLocker_Start();

    m_Modules.push_back(new ClientMenu());
    m_Modules.push_back(new ArrayList());
    m_Modules.push_back(new Esp());
    m_Modules.push_back(new ItemEsp());
    m_Modules.push_back(new PlayerEsp());
    m_Modules.push_back(new StorageEsp());
    m_Modules.push_back(new BlockEsp());
    m_Modules.push_back(new Nametag());
    m_Modules.push_back(new Tracer());
    m_Modules.push_back(new Trajectories());
    m_Modules.push_back(new Chams());
    m_Modules.push_back(new NotificationsModule());
    m_Modules.push_back(new Pointers());
    m_Modules.push_back(new Indicators());
    m_Modules.push_back(new NoHurtCam());
    m_Modules.push_back(new Velocity());
    m_Modules.push_back(new AimAssist());
    m_Modules.push_back(new ThrowModule());
    m_Modules.push_back(new KeepSprint());
    m_Modules.push_back(new Strafe());
    m_Modules.push_back(new Criticals());
    m_Modules.push_back(new SprintReset());
    m_Modules.push_back(new LagRange());
    m_Modules.push_back(new Reach());
    m_Modules.push_back(new Blink());
    m_Modules.push_back(new AutoRod());
    m_Modules.push_back(new AntiBot());
    m_Modules.push_back(new AutoBlock());
    m_Modules.push_back(new Backtrack());
    m_Modules.push_back(new AutoWeapon());
    m_Modules.push_back(new LeftClicker());
    m_Modules.push_back(new ArmorSwitcher());
    m_Modules.push_back(new ScrollModule());
    m_Modules.push_back(new FastPlace());
    m_Modules.push_back(new FastBreak());
    m_Modules.push_back(new AutoTool());
    m_Modules.push_back(new ChestStealer());
    m_Modules.push_back(new InvManager());
    m_Modules.push_back(new BridgeAssist());
    m_Modules.push_back(new BlockIn());
    m_Modules.push_back(new Clutch());
    if (g_GameVersion == LUNAR_1_8_9)
        m_Modules.push_back(new TickLocker());
    m_Modules.push_back(new InvWalk());
    m_Modules.push_back(new FastStop());
    m_Modules.push_back(new NoJumpDelay());
    m_Modules.push_back(new QuickAccel());
    m_Modules.push_back(new SnapTap());
    m_Modules.push_back(new Sprint());
    m_Modules.push_back(new NoSlow());
    m_Modules.push_back(new FriendsModule());
    m_Modules.push_back(new EnemiesModule());
    m_Modules.push_back(new NoItemRelease());
    m_Modules.push_back(new AntiDebuff());
    m_Modules.push_back(new PingFix());
    m_Modules.push_back(new RightClickerModule());
    m_Modules.push_back(new BowBoost());
    m_Modules.push_back(new AutoRefill());

    for (Module* mod : m_Modules)
    {
        auto thread = std::thread([mod]()
            {
                JavaVM* jvm = g_Instance->GetJVM();

                JNIEnv* env = nullptr;
                jint res = jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
                if (res == JNI_EDETACHED) {
                    res = jvm->AttachCurrentThreadAsDaemon(
                        reinterpret_cast<void**>(&env), nullptr);
                    if (res != JNI_OK) return;
                }

                DWORD screenCheckTick = 0;
                bool  cachedInGame = false;
                const char* modName = mod->GetName();
                bool  isAimAssistModule = (strcmp(modName, "Aim Assist") == 0);
                bool  isKeepSprintModule = (strcmp(modName, "KeepSprint") == 0);
                bool  isCriticalsModule = (strcmp(modName, "Criticals") == 0);
                bool  isSprintResetModule = (strcmp(modName, "Sprint Reset") == 0);
                bool  isLagRangeModule = (strcmp(modName, "LagRange") == 0);
                bool  isReachModule = (strcmp(modName, "Reach") == 0);
                bool  isBlinkModule = (strcmp(modName, "Blink") == 0);
                bool  isBacktrackModule = (strcmp(modName, "Backtrack") == 0);
                bool  isInvWalkModule = (strcmp(modName, "InvWalk") == 0);
                bool  isNoJumpDelayModule = (strcmp(modName, "NoJumpDelay") == 0);
                bool  isQuickAccelModule = (strcmp(modName, "QuickAccel") == 0);
                bool  isSprintModule = (strcmp(modName, "Sprint") == 0);
                bool  isNoSlowModule = (strcmp(modName, "NoSlow") == 0);
                bool  isStrafeModule = (strcmp(modName, "Strafe") == 0);
                bool  isNoItemReleaseModule = (strcmp(modName, "No Item Release") == 0);
                bool  isAntiDebuffModule = (strcmp(modName, "Anti Debuff") == 0);
                bool  isNoHurtCamModule = (strcmp(modName, "No Hurt Cam") == 0);
                bool  isBowBoostModule = (strcmp(modName, "BowBoost") == 0);
                const bool tightLoop = isNoSlowModule || isQuickAccelModule || isNoItemReleaseModule
                    || isAntiDebuffModule;
                const bool midLoop = isAimAssistModule || isKeepSprintModule
                    || isCriticalsModule || isSprintResetModule || isLagRangeModule || isReachModule
                    || isBlinkModule || isBacktrackModule || isInvWalkModule || isNoJumpDelayModule
                    || isSprintModule || isStrafeModule || isNoHurtCamModule || isBowBoostModule;

                bool wasEnabled = false;
                DWORD entityCheckTick = 0;
                bool cachedHasPlayer = false;
                bool cachedHasWorld = false;

                while (!Communication::GetSettings()->m_Destruct)
                {
                    if (Overlay::isOpen) {
                        if (isBowBoostModule) {
                            if (env->PushLocalFrame(32) == JNI_OK) {
                                try { mod->Run(env); } catch (...) {}
                                env->PopLocalFrame(nullptr);
                            }
                        }
                        wasEnabled = false;
                        Sleep(40);
                        continue;
                    }

                    const bool en = mod->IsEnabled();
                    if (!en && !wasEnabled) {
                        Sleep(32);
                        continue;
                    }

                    if (env->PushLocalFrame(32) != JNI_OK) { Sleep(50); continue; }
                    if (env->ExceptionCheck()) env->ExceptionClear();

                    DWORD now = GetTickCount();
                    bool shouldRun;
                    if (isInvWalkModule) {
                        if ((now - screenCheckTick) > 150) {
                            jobject screenObj = Minecraft::GetCurrentScreen(env);
                            cachedInGame = (screenObj == nullptr);
                            if (screenObj) env->DeleteLocalRef(screenObj);
                            screenCheckTick = now;
                        }
                        shouldRun = !cachedInGame;
                    }
                    else {
                        if ((now - entityCheckTick) > 80) {
                            cachedHasPlayer = Minecraft::GetThePlayer(env) != nullptr;
                            cachedHasWorld = Minecraft::GetTheWorld(env) != nullptr;
                            entityCheckTick = now;
                        }
                        shouldRun = cachedHasPlayer && cachedHasWorld;
                    }

                    if (shouldRun || (wasEnabled && !en)) {
                        try {
                            mod->Run(env);
                        }
                        catch (...) {
                        }
                    }

                    env->PopLocalFrame(nullptr);
                    wasEnabled = en;

                    int slp = 32;
                    if (en && shouldRun)
                        slp = tightLoop ? 1 : (midLoop ? 4 : 8);
                    else if (en)
                        slp = 16;
                    Sleep(slp);
                }

                jvm->DetachCurrentThread();
            });

        m_ModuleThread.push_back(std::move(thread));
    }
}

void Modules::DestroyModules()
{
    WSA_SignalShutdown();

    for (auto& thread : m_ModuleThread)
        if (thread.joinable()) thread.join();

    Clicker::Stop();
    RightClicker::Stop();
    Scroll::Stop();
    Armor::Stop();
    AutoRefill_Stop();
    Throw_Stop();
    TickLocker_Stop();

    for (const auto mod : m_Modules)
        delete mod;

    m_Modules.clear();
    m_ModuleThread.clear();
}

std::vector<Module*>     Modules::m_Modules;
std::vector<std::thread> Modules::m_ModuleThread;