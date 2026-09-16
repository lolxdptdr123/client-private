#include "pch.h"
#include "NoItemRelease.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Classes/KeyBinding.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static jfieldID s_itemInUse = nullptr;
static jfieldID s_itemInUseCount = nullptr;
static bool s_triedFields = false;

static jclass s_c08Cls = nullptr;
static jmethodID s_c08CtorStack = nullptr;
static jmethodID s_c08Ctor17 = nullptr;
static jmethodID s_addQueue = nullptr;
static jfieldID s_sendQueue = nullptr;
static jmethodID s_sendUseItem = nullptr;
static bool s_triedNet = false;

static bool s_armed = false;
static bool s_rmbPrev = false;

static void EnsureFields(JNIEnv* env, jobject player) {
    if (s_triedFields || !env || !player) return;
    s_triedFields = true;

    std::string stackSig = Mapper::Get("net/minecraft/item/ItemStack", 2);
    std::string useName = Mapper::Get("itemInUse");
    if (useName.empty()) useName = "itemInUse";
    std::string countName = Mapper::Get("itemInUseCount");
    if (countName.empty()) countName = "itemInUseCount";

    jclass walk = env->GetObjectClass(player);
    for (int d = 0; walk && d < 8; d++) {
        if (!s_itemInUseCount) {
            s_itemInUseCount = env->GetFieldID(walk, countName.c_str(), "I");
            JniOk(env);
        }
        if (!s_itemInUse && !stackSig.empty()) {
            s_itemInUse = env->GetFieldID(walk, useName.c_str(), stackSig.c_str());
            JniOk(env);
        }
        if (s_itemInUse && s_itemInUseCount) break;
        jclass sup = env->GetSuperclass(walk);
        env->DeleteLocalRef(walk);
        walk = sup;
    }
    if (walk) env->DeleteLocalRef(walk);
}

static jclass GlobalMapped(JNIEnv* env, const char* key) {
    std::string n = Mapper::Get(key);
    if (n.empty()) n = key;
    Klass* k = g_Instance->FindClass(n);
    if (!k && n != key)
        k = g_Instance->FindClass(key);
    if (!k) return nullptr;
    return (jclass)env->NewGlobalRef((jclass)k);
}

static void EnsureNet(JNIEnv* env) {
    if (s_triedNet || !env) return;
    s_triedNet = true;

    s_c08Cls = GlobalMapped(env, "net/minecraft/network/play/client/C08PacketPlayerBlockPlacement");
    if (s_c08Cls) {
        std::string stackSig = Mapper::Get("net/minecraft/item/ItemStack", 2);
        if (!stackSig.empty()) {
            s_c08CtorStack = env->GetMethodID(s_c08Cls, "<init>", ("(" + stackSig + ")V").c_str());
            JniOk(env);
        }
        if (!s_c08CtorStack) {
            s_c08CtorStack = env->GetMethodID(s_c08Cls, "<init>", "(Lnet/minecraft/item/ItemStack;)V");
            JniOk(env);
        }
        if (!s_c08CtorStack) {
            std::string stackSig = Mapper::Get("net/minecraft/item/ItemStack", 2);
            if (!stackSig.empty()) {
                s_c08Ctor17 = env->GetMethodID(s_c08Cls, "<init>", ("(IIII" + stackSig + "FFF)V").c_str());
                JniOk(env);
            }
        }
    }

    std::string playerN = Mapper::Get("net/minecraft/client/entity/EntityClientPlayerMP");
    Klass* pk = playerN.empty() ? nullptr : g_Instance->FindClass(playerN);
    if (!pk) {
        std::string sp = Mapper::Get("net/minecraft/client/entity/EntityPlayerSP");
        pk = sp.empty() ? nullptr : g_Instance->FindClass(sp);
    }
    if (pk) {
        std::string sq = Mapper::Get("net/minecraft/client/network/NetHandlerPlayClient", 2);
        s_sendQueue = env->GetFieldID((jclass)pk, Mapper::Get("sendQueue").c_str(), sq.c_str());
        JniOk(env);
    }

    std::string nhN = Mapper::Get("net/minecraft/client/network/NetHandlerPlayClient");
    Klass* nh = nhN.empty() ? nullptr : g_Instance->FindClass(nhN);
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
        env->DeleteLocalRef(pcc);
        env->DeleteLocalRef(pc);
    }
}

static void ClearUse(JNIEnv* env, Player* lp) {
    if (!lp) return;
    EnsureFields(env, (jobject)lp);
    if (s_itemInUse)
        env->SetObjectField((jobject)lp, s_itemInUse, nullptr);
    JniOk(env);
    if (s_itemInUseCount)
        env->SetIntField((jobject)lp, s_itemInUseCount, 0);
    JniOk(env);
}

static bool PhysUseDown(JNIEnv* env) {
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
        return true;
    jobject gs = Minecraft::GetGameSettings(env);
    JniOk(env);
    if (!gs) return false;
    jobject kb = ((GameSettings*)gs)->GetKeyBindUseItem(env);
    env->DeleteLocalRef(gs);
    JniOk(env);
    if (!kb) return false;
    bool down = ((KeyBinding*)kb)->IsPhysDown(env);
    JniOk(env);
    env->DeleteLocalRef(kb);
    return down;
}

static bool IsClass(JNIEnv* env, ItemStack* st, const char* key) {
    std::string n = Mapper::Get(key);
    if (n.empty()) return false;
    bool ok = st->Is(n.c_str(), env);
    JniOk(env);
    return ok;
}

enum class Kind { None, Sword, Consume, Skip };

static Kind HeldKind(JNIEnv* env, Player* local) {
    jobject st = local->GetHeldItem(env);
    JniOk(env);
    if (!st) return Kind::None;
    auto* stack = (ItemStack*)st;
    int id = stack->GetItemId(env);
    JniOk(env);

    bool sword = IsClass(env, stack, "net/minecraft/item/ItemSword")
        || id == 267 || id == 268 || id == 272 || id == 276 || id == 283;
    bool bow = (id == 261) || IsClass(env, stack, "net/minecraft/item/ItemBow");
    bool rod = (id == 346) || stack->IsRod(env);
    JniOk(env);

    bool consume = id == 322 || id == 466 || id == 373 || id == 335 || id == 282
        || id == 260 || id == 297 || id == 319 || id == 320 || id == 350
        || id == 357 || id == 360 || id == 364 || id == 366 || id == 391
        || id == 393 || id == 396 || id == 400
        || IsClass(env, stack, "net/minecraft/item/ItemFood")
        || IsClass(env, stack, "net/minecraft/item/ItemAppleGold")
        || IsClass(env, stack, "net/minecraft/item/ItemPotion")
        || stack->IsSoup(env);
    JniOk(env);

    env->DeleteLocalRef(st);
    if (bow || rod) return Kind::Skip;
    if (sword) return Kind::Sword;
    if (consume) return Kind::Consume;
    return Kind::None;
}

static bool ModeAllows(Kind k) {
    int m = NoItemReleaseSettings::mode;
    if (k == Kind::Consume) return m == 0 || m == 2;
    if (k == Kind::Sword) return m == 1 || m == 2;
    return false;
}

static bool SendUseAir(JNIEnv* env, jobject playerObj, Player* local) {
    EnsureNet(env);
    jobject stack = local->GetHeldItem(env);
    JniOk(env);
    if (!stack) return false;

    bool ok = false;
    if (s_sendUseItem) {
        jobject pc = Minecraft::GetPlayerController(env);
        jobject world = Minecraft::GetTheWorld(env);
        JniOk(env);
        if (pc && world) {
            env->CallBooleanMethod(pc, s_sendUseItem, playerObj, world, stack);
            JniOk(env);
            ok = true;
        }
        if (pc) env->DeleteLocalRef(pc);
        if (world) env->DeleteLocalRef(world);
    }

    if (!ok && s_c08Cls && s_sendQueue && s_addQueue && (s_c08CtorStack || s_c08Ctor17)) {
        jobject queue = env->GetObjectField(playerObj, s_sendQueue);
        JniOk(env);
        if (queue) {
            jobject pkt = nullptr;
            if (s_c08CtorStack)
                pkt = env->NewObject(s_c08Cls, s_c08CtorStack, stack);
            else
                pkt = env->NewObject(s_c08Cls, s_c08Ctor17, -1, -1, -1, 255, stack, 0.f, 0.f, 0.f);
            JniOk(env);
            if (pkt) {
                env->CallVoidMethod(queue, s_addQueue, pkt);
                JniOk(env);
                env->DeleteLocalRef(pkt);
                ok = true;
            }
            env->DeleteLocalRef(queue);
        }
    }

    env->DeleteLocalRef(stack);
    return ok;
}

static void Tick(JNIEnv* env) {
    if (!env || Overlay::isOpen) {
        s_armed = false;
        s_rmbPrev = false;
        return;
    }

    jobject localObj = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!localObj) return;
    auto* local = (Player*)localObj;

    Kind kind = HeldKind(env, local);
    bool usingItem = local->IsUsingItem(env);
    JniOk(env);
    EnsureFields(env, localObj);
    int count = 0;
    if (s_itemInUseCount) {
        count = env->GetIntField(localObj, s_itemInUseCount);
        JniOk(env);
    }
    if (!usingItem)
        usingItem = count > 0;

    bool rmb = PhysUseDown(env);
    bool allow = ModeAllows(kind);
    bool press = rmb && !s_rmbPrev;
    bool release = !rmb && s_rmbPrev;
    s_rmbPrev = rmb;

    if (!allow || kind == Kind::None || kind == Kind::Skip) {
        s_armed = false;
        env->DeleteLocalRef(localObj);
        return;
    }

    if (kind == Kind::Consume) {
        if (press)
            s_armed = true;
        if (s_armed && !usingItem && (press || release))
            SendUseAir(env, localObj, local);
        if (s_armed && !rmb)
            ClearUse(env, local);
        if (!rmb && !usingItem && !press && !release)
            s_armed = false;
    } else if (kind == Kind::Sword) {
        if (usingItem && rmb)
            s_armed = true;
        if (s_armed && !rmb && usingItem)
            ClearUse(env, local);
        if (!usingItem && rmb)
            s_armed = false;
    }

    env->DeleteLocalRef(localObj);
}

void NoItemRelease::Run(JNIEnv* env) {
    if (!enabled) {
        s_armed = false;
        s_rmbPrev = false;
        return;
    }
    Tick(env);
}

void NoItemRelease::OnRender(JNIEnv* env) {
    if (!enabled || !env) return;
    Tick(env);
}
