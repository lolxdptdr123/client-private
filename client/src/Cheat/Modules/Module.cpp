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
#include "Visuals/Nametag.h"
#include "Visuals/Tracer.h"
#include "Visuals/Trajectories.h"
#include "Visuals/Notifications.h"
#include "Combat/Clicker.h"
#include "Combat/AimAssist.h"
#include "Combat/velocity.h"
#include "Combat/AutoRefill.h"
#include "Combat/Throw.h"
#include "Combat/Piercing.h"
#include "Combat/KeepSprint.h"
#include "Combat/Criticals.h"
#include "Combat/AutoRod.h"
#include "Combat/AntiBot.h"
#include "Misc/Friends.h"
#include "Misc/Enemies.h"
#include "Misc/FastPlace.h"
#include "Misc/FastBreak.h"
#include "Misc/TickLocker.h"
#include "Misc/InvWalk.h"
#include "Misc/FastStop.h"
#include "Misc/NoJumpDelay.h"
#include "Misc/QuickAccel.h"
#include "Misc/SnapTap.h"
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
    m_Modules.push_back(new Nametag());
    m_Modules.push_back(new Tracer());
    m_Modules.push_back(new Trajectories());
    m_Modules.push_back(new Chams());
    m_Modules.push_back(new NotificationsModule());
    m_Modules.push_back(new Velocity());
    m_Modules.push_back(new AimAssist());
    m_Modules.push_back(new ThrowModule());
    m_Modules.push_back(new Piercing());
    m_Modules.push_back(new KeepSprint());
    m_Modules.push_back(new Criticals());
    m_Modules.push_back(new AutoRod());
    m_Modules.push_back(new AntiBot());
    m_Modules.push_back(new LeftClicker());
    m_Modules.push_back(new ArmorSwitcher());
    m_Modules.push_back(new ScrollModule());
    m_Modules.push_back(new FastPlace());
    m_Modules.push_back(new FastBreak());
    if (g_GameVersion == LUNAR_1_8_9)
        m_Modules.push_back(new TickLocker());
    m_Modules.push_back(new InvWalk());
    m_Modules.push_back(new FastStop());
    m_Modules.push_back(new NoJumpDelay());
    m_Modules.push_back(new QuickAccel());
    m_Modules.push_back(new SnapTap());
    m_Modules.push_back(new FriendsModule());
    m_Modules.push_back(new EnemiesModule());
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
                bool  cachedInInventory = false;
                const char* modName = mod->GetName();
                bool  isAimAssistModule = (strcmp(modName, "Aim Assist") == 0);
                bool  isPiercingModule = (strcmp(modName, "Piercing") == 0);
                bool  isKeepSprintModule = (strcmp(modName, "KeepSprint") == 0);
                bool  isCriticalsModule = (strcmp(modName, "Criticals") == 0);
                bool  isInvWalkModule = (strcmp(modName, "InvWalk") == 0);
                bool  isNoJumpDelayModule = (strcmp(modName, "NoJumpDelay") == 0);
                bool  isQuickAccelModule = (strcmp(modName, "QuickAccel") == 0);

                if (isAimAssistModule || isPiercingModule || isKeepSprintModule || isCriticalsModule)
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

                while (!Communication::GetSettings()->m_Destruct)
                {
                    if (env->PushLocalFrame(64) != JNI_OK) { Sleep(50); continue; }

                    if (env->ExceptionCheck()) env->ExceptionClear();

                    if (Overlay::isOpen) { env->PopLocalFrame(nullptr); Sleep(50); continue; }

                    DWORD now = GetTickCount();
                    if ((now - screenCheckTick) > 200 || isInvWalkModule) {
                        jobject screenObj = Minecraft::GetCurrentScreen(env);
                        auto* screen = (GuiScreen*)screenObj;
                        cachedInGame = (screen == nullptr);
                        cachedInInventory = (screen != nullptr) && screen->IsInventory(env);
                        g_playerInGame.store(cachedInGame);
                        if (screenObj) env->DeleteLocalRef(screenObj);
                        screenCheckTick = now;
                    }

                    const bool hasPlayer = Minecraft::GetThePlayer(env) != nullptr;
                    const bool hasWorld = Minecraft::GetTheWorld(env) != nullptr;

                    bool shouldRun;
                    if (isInvWalkModule)
                        shouldRun = !cachedInGame;
                    else
                        shouldRun = hasPlayer && hasWorld;

                    if (shouldRun)
                    {
                        try {
                            mod->Run(env);
                        }
                        catch (...) {
                        }
                        env->PopLocalFrame(nullptr);
                        Sleep((isAimAssistModule || isPiercingModule || isKeepSprintModule || isCriticalsModule || isInvWalkModule || isNoJumpDelayModule || isQuickAccelModule) ? 2 : 5);
                    }
                    else
                    {
                        env->PopLocalFrame(nullptr);
                        Sleep(50);
                    }
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