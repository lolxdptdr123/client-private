#include "pch.h"
#include "BowBoost.h"

#include "Overlay.h"
#include "../Menu.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>

enum class BbState { Idle, Swap, Charging, Releasing, Recover };

static std::atomic<bool> g_fire{ false };
static BbState s_state = BbState::Idle;
static long long s_stateAt = 0;
static int s_origSlot = -1;
static float s_savedPitch = 0.f;
static float s_savedPrevPitch = 0.f;
static bool s_holdingUse = false;
static bool s_keyWasDown = false;

static jmethodID s_sendUseItem = nullptr;
static jmethodID s_onStoppedUsing = nullptr;
static jmethodID s_stopUsingItem = nullptr;
static jfieldID s_sendQueue = nullptr;
static jmethodID s_addQueue = nullptr;
static jclass s_c07Cls = nullptr;
static jmethodID s_c07Ctor18 = nullptr;
static jmethodID s_c07Ctor17 = nullptr;
static jobject s_releaseAction = nullptr;
static jobject s_blockOrigin = nullptr;
static jobject s_faceDown = nullptr;
static jfieldID s_itemInUseCount = nullptr;
static bool s_tried = false;

void BowBoost_Trigger() { g_fire = true; }

static long long NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static jclass GlobalMapped(JNIEnv* env, const char* key) {
    std::string n = Mapper::Get(key);
    if (n.empty()) n = key;
    Klass* k = g_Instance->FindClass(n.c_str());
    if (!k && n != key)
        k = g_Instance->FindClass(key);
    if (!k) return nullptr;
    return (jclass)env->NewGlobalRef((jclass)k);
}

static jobject EnumAt(JNIEnv* env, jclass enumCls, int index) {
    if (!enumCls) return nullptr;
    jclass clsClass = env->FindClass("java/lang/Class");
    JniOk(env);
    if (!clsClass) return nullptr;
    jmethodID getConsts = env->GetMethodID(clsClass, "getEnumConstants", "()[Ljava/lang/Object;");
    env->DeleteLocalRef(clsClass);
    if (!getConsts) { JniOk(env); return nullptr; }
    jobjectArray arr = (jobjectArray)env->CallObjectMethod(enumCls, getConsts);
    JniOk(env);
    if (!arr) return nullptr;
    const jsize n = env->GetArrayLength(arr);
    jobject out = nullptr;
    if (index >= 0 && index < n)
        out = env->GetObjectArrayElement(arr, index);
    else if (n > 0)
        out = env->GetObjectArrayElement(arr, n - 1);
    env->DeleteLocalRef(arr);
    return out;
}

static void Ensure(JNIEnv* env) {
    if (s_tried || !env) return;
    s_tried = true;

    s_c07Cls = GlobalMapped(env, "net/minecraft/network/play/client/C07PacketPlayerDigging");
    jclass actionCls = GlobalMapped(env, "net/minecraft/network/play/client/C07PacketPlayerDigging$Action");
    jclass bpCls = GlobalMapped(env, "net/minecraft/util/BlockPos");
    jclass faceCls = GlobalMapped(env, "net/minecraft/util/EnumFacing");

    if (s_c07Cls && actionCls && bpCls && faceCls) {
        std::string aSig = Mapper::Get("net/minecraft/network/play/client/C07PacketPlayerDigging$Action", 2);
        std::string bSig = Mapper::Get("net/minecraft/util/BlockPos", 2);
        std::string fSig = Mapper::Get("net/minecraft/util/EnumFacing", 2);
        if (!aSig.empty() && !bSig.empty() && !fSig.empty()) {
            std::string sig = "(" + aSig + bSig + fSig + ")V";
            s_c07Ctor18 = env->GetMethodID(s_c07Cls, "<init>", sig.c_str());
            JniOk(env);
        }
        jobject rel = EnumAt(env, actionCls, 5);
        if (rel) s_releaseAction = env->NewGlobalRef(rel);
        if (rel) env->DeleteLocalRef(rel);

        jmethodID bpCtor = env->GetMethodID(bpCls, "<init>", "(III)V");
        JniOk(env);
        if (bpCtor) {
            jobject origin = env->NewObject(bpCls, bpCtor, 0, 0, 0);
            JniOk(env);
            if (origin) s_blockOrigin = env->NewGlobalRef(origin);
            if (origin) env->DeleteLocalRef(origin);
        }
        jobject down = EnumAt(env, faceCls, 0);
        if (down) s_faceDown = env->NewGlobalRef(down);
        if (down) env->DeleteLocalRef(down);
    }
    if (s_c07Cls && !s_c07Ctor18) {
        s_c07Ctor17 = env->GetMethodID(s_c07Cls, "<init>", "(IIIII)V");
        JniOk(env);
    }
    if (actionCls) env->DeleteGlobalRef(actionCls);
    if (bpCls) env->DeleteGlobalRef(bpCls);
    if (faceCls) env->DeleteGlobalRef(faceCls);

    std::string playerN = Mapper::Get("net/minecraft/client/entity/EntityClientPlayerMP");
    Klass* pk = playerN.empty() ? nullptr : g_Instance->FindClass(playerN.c_str());
    if (!pk) {
        std::string sp = Mapper::Get("net/minecraft/client/entity/EntityPlayerSP");
        pk = sp.empty() ? nullptr : g_Instance->FindClass(sp.c_str());
    }
    if (pk) {
        std::string sq = Mapper::Get("net/minecraft/client/network/NetHandlerPlayClient", 2);
        s_sendQueue = env->GetFieldID((jclass)pk, Mapper::Get("sendQueue").c_str(), sq.c_str());
        JniOk(env);
        std::string stop = Mapper::Get("stopUsingItem");
        if (stop.empty()) stop = "stopUsingItem";
        s_stopUsingItem = env->GetMethodID((jclass)pk, stop.c_str(), "()V");
        JniOk(env);
        if (!s_stopUsingItem) {
            jclass walk = (jclass)env->NewLocalRef((jclass)pk);
            for (int d = 0; walk && d < 8 && !s_stopUsingItem; d++) {
                s_stopUsingItem = env->GetMethodID(walk, "stopUsingItem", "()V");
                JniOk(env);
                if (s_stopUsingItem) break;
                std::string mapped = Mapper::Get("stopUsingItem");
                if (!mapped.empty()) {
                    s_stopUsingItem = env->GetMethodID(walk, mapped.c_str(), "()V");
                    JniOk(env);
                }
                jclass sup = env->GetSuperclass(walk);
                env->DeleteLocalRef(walk);
                walk = sup;
            }
            if (walk) env->DeleteLocalRef(walk);
        }
    }

    std::string nhN = Mapper::Get("net/minecraft/client/network/NetHandlerPlayClient");
    Klass* nh = nhN.empty() ? nullptr : g_Instance->FindClass(nhN.c_str());
    if (nh) {
        std::string sig = "(" + Mapper::Get("net/minecraft/network/Packet", 2) + ")V";
        s_addQueue = env->GetMethodID((jclass)nh, Mapper::Get("addToSendQueue").c_str(), sig.c_str());
        JniOk(env);
    }

    jobject pc = Minecraft::GetPlayerController(env);
    JniOk(env);
    if (pc) {
        jclass pcc = env->GetObjectClass(pc);
        std::string name = Mapper::Get("sendUseItem");
        if (name.empty()) name = "sendUseItem";
        const char* players[] = {
            "net/minecraft/entity/player/EntityPlayer",
            "net/minecraft/client/entity/EntityPlayerSP",
            "net/minecraft/client/entity/EntityClientPlayerMP"
        };
        const char* worlds[] = {
            "net/minecraft/world/World",
            "net/minecraft/client/multiplayer/WorldClient"
        };
        std::string stackSig = Mapper::Get("net/minecraft/item/ItemStack", 2);
        for (const char* p : players) {
            if (s_sendUseItem) break;
            for (const char* w : worlds) {
                std::string ps = Mapper::Get(p, 2);
                std::string ws = Mapper::Get(w, 2);
                if (ps.empty() || ws.empty() || stackSig.empty()) continue;
                std::string sig = "(" + ps + ws + stackSig + ")Z";
                s_sendUseItem = env->GetMethodID(pcc, name.c_str(), sig.c_str());
                JniOk(env);
                if (s_sendUseItem) break;
            }
        }

        std::string stopName = Mapper::Get("onStoppedUsingItem");
        if (stopName.empty()) stopName = "onStoppedUsingItem";
        for (const char* p : players) {
            if (s_onStoppedUsing) break;
            std::string ps = Mapper::Get(p, 2);
            if (ps.empty()) continue;
            std::string sig = "(" + ps + ")V";
            s_onStoppedUsing = env->GetMethodID(pcc, stopName.c_str(), sig.c_str());
            JniOk(env);
        }
        env->DeleteLocalRef(pcc);
        env->DeleteLocalRef(pc);
    }
}

static void CacheUseCountField(JNIEnv* env, jobject player) {
    if (s_itemInUseCount || !player) return;
    std::string countName = Mapper::Get("itemInUseCount");
    if (countName.empty()) countName = "itemInUseCount";
    jclass walk = env->GetObjectClass(player);
    for (int d = 0; walk && d < 8; d++) {
        s_itemInUseCount = env->GetFieldID(walk, countName.c_str(), "I");
        JniOk(env);
        if (s_itemInUseCount) break;
        jclass sup = env->GetSuperclass(walk);
        env->DeleteLocalRef(walk);
        walk = sup;
    }
    if (walk) env->DeleteLocalRef(walk);
}

static void SetUse(JNIEnv* env, bool down) {
    jobject gs = Minecraft::GetGameSettings(env);
    JniOk(env);
    if (!gs) return;
    jobject bind = ((GameSettings*)gs)->GetKeyBindUseItem(env);
    JniOk(env);
    if (bind) {
        auto* kb = (KeyBinding*)bind;
        kb->SetPressed(down, env);
        if (down)
            kb->SetPressTime(1, env);
        env->DeleteLocalRef(bind);
    }
    s_holdingUse = down;
}

static void SendPacket(JNIEnv* env, jobject playerObj, jobject pkt) {
    if (!pkt || !s_sendQueue || !s_addQueue) return;
    jobject queue = env->GetObjectField(playerObj, s_sendQueue);
    JniOk(env);
    if (!queue) return;
    env->CallVoidMethod(queue, s_addQueue, pkt);
    JniOk(env);
    env->DeleteLocalRef(queue);
}

static void StartUse(JNIEnv* env, jobject playerObj, Player* local) {
    Ensure(env);
    jobject stack = local->GetHeldItem(env);
    JniOk(env);
    Minecraft::SetRightClickDelayTimer(env, 0);
    SetUse(env, true);
    if (s_sendUseItem && stack) {
        jobject pc = Minecraft::GetPlayerController(env);
        jobject world = Minecraft::GetTheWorld(env);
        JniOk(env);
        if (pc && world) {
            env->CallBooleanMethod(pc, s_sendUseItem, playerObj, world, stack);
            JniOk(env);
        }
        if (pc) env->DeleteLocalRef(pc);
        if (world) env->DeleteLocalRef(world);
    }
    Minecraft::RightClickMouse(env);
    if (stack) env->DeleteLocalRef(stack);
}

static void StopUse(JNIEnv* env, jobject playerObj, Player* local) {
    Ensure(env);
    SetUse(env, false);

    bool sent = false;
    if (s_onStoppedUsing) {
        jobject pc = Minecraft::GetPlayerController(env);
        JniOk(env);
        if (pc) {
            env->CallVoidMethod(pc, s_onStoppedUsing, playerObj);
            JniOk(env);
            sent = true;
            env->DeleteLocalRef(pc);
        }
    }

    if (!sent && s_c07Cls && s_addQueue && s_sendQueue) {
        jobject pkt = nullptr;
        if (s_c07Ctor18 && s_releaseAction && s_blockOrigin && s_faceDown)
            pkt = env->NewObject(s_c07Cls, s_c07Ctor18, s_releaseAction, s_blockOrigin, s_faceDown);
        else if (s_c07Ctor17)
            pkt = env->NewObject(s_c07Cls, s_c07Ctor17, 5, 0, 0, 0, 255);
        JniOk(env);
        if (pkt) {
            SendPacket(env, playerObj, pkt);
            env->DeleteLocalRef(pkt);
            sent = true;
        }
    }

    if (s_stopUsingItem) {
        env->CallVoidMethod(playerObj, s_stopUsingItem);
        JniOk(env);
    }
    CacheUseCountField(env, playerObj);
    if (s_itemInUseCount)
        env->SetIntField(playerObj, s_itemInUseCount, 0);
    JniOk(env);
}

static bool IsBowStack(JNIEnv* env, jobject stackObj) {
    if (!stackObj) return false;
    auto* st = (ItemStack*)stackObj;
    if (st->GetItemId(env) == 261) return true;
    JniOk(env);
    return st->Is("net/minecraft/item/ItemBow", env);
}

static int FindBowSlot(JNIEnv* env, InventoryPlayer* inv) {
    if (!inv) return -1;
    for (int i = 0; i < 9; i++) {
        jobject s = inv->GetStackInSlot(i, env);
        JniOk(env);
        if (!s) continue;
        const bool bow = IsBowStack(env, s);
        env->DeleteLocalRef(s);
        if (bow) return i;
    }
    return -1;
}

static bool HasArrows(JNIEnv* env, InventoryPlayer* inv) {
    if (!inv) return false;
    for (int i = 0; i < 36; i++) {
        jobject s = inv->GetStackInSlot(i, env);
        JniOk(env);
        if (!s) continue;
        const int id = ((ItemStack*)s)->GetItemId(env);
        env->DeleteLocalRef(s);
        if (id == 262) return true;
    }
    return false;
}

static void RestorePitch(JNIEnv* env, Player* local) {
    if (!local || !BowBoostSettings::lookUp) return;
    local->SetRotationPitch(s_savedPitch, env);
    local->SetPrevRotationPitch(s_savedPrevPitch, env);
}

static void Abort(JNIEnv* env, jobject playerObj, Player* local, InventoryPlayer* inv) {
    if (s_state == BbState::Charging || s_holdingUse)
        StopUse(env, playerObj, local);
    else if (s_holdingUse)
        SetUse(env, false);
    RestorePitch(env, local);
    if (inv && BowBoostSettings::switchItem && s_origSlot >= 0)
        inv->SetSlot(s_origSlot, env);
    s_origSlot = -1;
    s_state = BbState::Idle;
    g_fire = false;
}

static void BeginDraw(JNIEnv* env, jobject playerObj, Player* local) {
    if (BowBoostSettings::lookUp) {
        s_savedPitch = local->GetRotationPitch(env);
        s_savedPrevPitch = local->GetPrevRotationPitch(env);
        const float look = -(std::max)(10.f, (std::min)(89.f, BowBoostSettings::pitch));
        local->SetRotationPitch(look, env);
        local->SetPrevRotationPitch(look, env);
    }
    StartUse(env, playerObj, local);
    s_state = BbState::Charging;
    s_stateAt = NowMs();
}

void BowBoost::Run(JNIEnv* env) {
    if (!env) return;
    Ensure(env);

    if (MenuBinds::bboost_listening)
        s_keyWasDown = false;

    const int bind = MenuBinds::bboost_bind;
    const bool keyDown = !MenuBinds::bboost_listening && bind != 0
        && (GetAsyncKeyState(bind) & 0x8000) != 0;
    if (keyDown && !s_keyWasDown)
        g_fire = true;
    s_keyWasDown = keyDown;

    auto cleanupPlayer = [&](jobject p) {
        jobject invObj = p ? ((Player*)p)->GetInventoryPlayer(env) : nullptr;
        Abort(env, p, (Player*)p, (InventoryPlayer*)invObj);
        if (invObj) env->DeleteLocalRef(invObj);
        if (p) env->DeleteLocalRef(p);
    };

    if (!enabled) {
        if (s_state != BbState::Idle) {
            jobject p = Minecraft::GetThePlayer(env);
            cleanupPlayer(p);
        }
        g_fire = false;
        return;
    }

    if (Overlay::isOpen || !IsGameWindowFocused()) {
        if (s_state != BbState::Idle) {
            jobject p = Minecraft::GetThePlayer(env);
            cleanupPlayer(p);
        }
        g_fire = false;
        return;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    if (!playerObj) return;
    auto* local = (Player*)playerObj;
    const long long now = NowMs();

    jobject screen = Minecraft::GetCurrentScreen(env);
    if (screen) {
        env->DeleteLocalRef(screen);
        if (s_state != BbState::Idle) {
            jobject invObj = local->GetInventoryPlayer(env);
            Abort(env, playerObj, local, (InventoryPlayer*)invObj);
            if (invObj) env->DeleteLocalRef(invObj);
        }
        g_fire = false;
        env->DeleteLocalRef(playerObj);
        return;
    }

    jobject invObj = local->GetInventoryPlayer(env);
    auto* inv = (InventoryPlayer*)invObj;

    if (s_state == BbState::Recover) {
        if (now - s_stateAt >= (long long)(std::max)(0, BowBoostSettings::delayMs))
            s_state = BbState::Idle;
        if (invObj) env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return;
    }

    if (s_state == BbState::Swap) {
        if (now - s_stateAt >= 50)
            BeginDraw(env, playerObj, local);
        if (invObj) env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return;
    }

    if (s_state == BbState::Releasing) {
        if (now - s_stateAt >= 50) {
            RestorePitch(env, local);
            if (inv && BowBoostSettings::switchItem && s_origSlot >= 0)
                inv->SetSlot(s_origSlot, env);
            s_origSlot = -1;
            s_state = BbState::Recover;
            s_stateAt = now;
        }
        if (invObj) env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return;
    }

    if (s_state == BbState::Charging) {
        const int chargeMs = (std::max)(1, BowBoostSettings::chargeTicks) * 50;
        if (BowBoostSettings::lookUp)
            local->SetRotationPitch(-(std::max)(10.f, (std::min)(89.f, BowBoostSettings::pitch)), env);
        SetUse(env, true);
        if (now - s_stateAt >= chargeMs) {
            StopUse(env, playerObj, local);
            s_state = BbState::Releasing;
            s_stateAt = now;
        }
        if (invObj) env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return;
    }

    if (!g_fire.exchange(false)) {
        if (invObj) env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return;
    }

    if (!HasArrows(env, inv)) {
        if (invObj) env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return;
    }

    const int bowSlot = FindBowSlot(env, inv);
    if (bowSlot < 0) {
        if (invObj) env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(playerObj);
        return;
    }

    const int cur = inv ? inv->GetSlot(env) : -1;
    jobject held = local->GetHeldItem(env);
    const bool holdingBow = IsBowStack(env, held);
    if (held) env->DeleteLocalRef(held);

    if (!holdingBow) {
        if (!BowBoostSettings::switchItem || !inv) {
            if (invObj) env->DeleteLocalRef(invObj);
            env->DeleteLocalRef(playerObj);
            return;
        }
        s_origSlot = cur;
        inv->SetSlot(bowSlot, env);
        s_state = BbState::Swap;
        s_stateAt = now;
    } else {
        s_origSlot = cur;
        BeginDraw(env, playerObj, local);
    }

    if (invObj) env->DeleteLocalRef(invObj);
    env->DeleteLocalRef(playerObj);
}
