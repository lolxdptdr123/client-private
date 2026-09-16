#include "pch.h"
#include "InvManager.h"

#include "Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/GuiScreen.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/InventoryPlayer.h"
#include "../../../Game/Classes/ItemStack.h"
#include "../../../Game/Classes/GameSettings.h"
#include "../../../Game/Klass.h"
#include "../../../Game/Field.h"
#include "../../../Game/Mapper.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"

#include <algorithm>
#include <cmath>
#include <random>

static std::mt19937 g_rng{ std::random_device{}() };
static bool s_open = false;
static bool s_shiftHeld = false;
static ULONGLONG s_next = 0;
static int s_lastSlot = -1;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static int RandRange(int a, int b) {
    if (b < a) std::swap(a, b);
    if (a == b) return a;
    std::uniform_int_distribution<int> d(a, b);
    return d(g_rng);
}

static int InvToCont(int inv) {
    if (inv >= 0 && inv <= 8) return 36 + inv;
    if (inv >= 9 && inv <= 35) return inv;
    return -1;
}

static int ArmorPiece(int id) {
    if (id < 298 || id > 317) return -1;
    return (id - 298) % 4;
}

static int ArmorTier(int id) {
    if (id >= 310 && id <= 313) return 5;
    if (id >= 306 && id <= 309) return 4;
    if (id >= 302 && id <= 305) return 3;
    if (id >= 314 && id <= 317) return 2;
    if (id >= 298 && id <= 301) return 1;
    return 0;
}

static int SwordTier(int id) {
    if (id == 276) return 5;
    if (id == 267) return 4;
    if (id == 272) return 3;
    if (id == 268 || id == 283) return 2;
    return 0;
}

static int AxeTier(int id) {
    if (id == 279) return 5;
    if (id == 258) return 4;
    if (id == 275) return 3;
    if (id == 271 || id == 286) return 2;
    return 0;
}

static int PickTier(int id) {
    if (id == 278) return 5;
    if (id == 257) return 4;
    if (id == 274) return 3;
    if (id == 270 || id == 285) return 2;
    return 0;
}

static bool IsSwordId(int id) { return SwordTier(id) > 0; }
static bool IsAxeId(int id) { return AxeTier(id) > 0; }
static bool IsPickId(int id) { return PickTier(id) > 0; }
static bool IsBowId(int id) { return id == 261; }
static bool IsPearlId(int id) { return id == 368; }
static bool IsRodId(int id) { return id == 346; }
static bool IsProjId(int id) { return id == 332 || id == 344; }
static bool IsGappleId(int id) { return id == 322 || id == 466; }
static bool IsSoupId(int id) { return id == 282; }
static bool IsPotionId(int id) { return id == 373; }
static bool IsWaterId(int id) { return id == 326; }
static bool IsLavaId(int id) { return id == 327; }
static bool IsFlintId(int id) { return id == 259; }
static bool IsWebId(int id) { return id == 30; }
static bool IsFoodId(int id) {
    switch (id) {
    case 260: case 282: case 297: case 319: case 320: case 322: case 350:
    case 357: case 360: case 364: case 366: case 391: case 393: case 396: case 400:
        return true;
    default: return false;
    }
}

static float ScoreStack(JNIEnv* env, ItemStack* st, int want) {
    if (!st || st->IsEmpty(env)) return -1.f;
    int id = st->GetItemId(env);
    JniOk(env);
    int sz = st->GetStackSize(env);
    JniOk(env);
    int meta = st->GetMetadata(env);
    JniOk(env);

    auto ench = [&](int e) { int v = st->GetEnchantmentLevel(e, env); JniOk(env); return v; };

    switch (want) {
    case 1: {
        int t = SwordTier(id);
        if (t <= 0) return -1.f;
        return t * 20.f + ench(16) * 6.f + ench(19) * 2.f + ench(20);
    }
    case 2: {
        int t = AxeTier(id);
        if (t <= 0) return -1.f;
        return t * 20.f + ench(16) * 6.f + ench(32) * 2.f;
    }
    case 3: {
        if (!IsBowId(id)) return -1.f;
        return 10.f + ench(48) * 8.f + ench(49) * 3.f + ench(50) * 4.f + ench(51) * 5.f;
    }
    case 4: {
        bool blk = st->IsBlock(env);
        JniOk(env);
        if (!blk && id != 1 && id != 3 && id != 4 && id != 5 && id != 24 && id != 87 && id != 98)
            return -1.f;
        return (float)sz + (id == 24 || id == 1 ? 8.f : 0.f);
    }
    case 5: {
        if (!IsGappleId(id)) return -1.f;
        float g = (id == 466 || meta == 1) ? 80.f : 40.f;
        return g + (float)sz;
    }
    case 6: return IsPearlId(id) ? 10.f + (float)sz : -1.f;
    case 7: return IsRodId(id) ? 10.f : -1.f;
    case 8: return IsProjId(id) ? 10.f + (float)sz : -1.f;
    case 9: return IsWaterId(id) ? 10.f : -1.f;
    case 10: return IsLavaId(id) ? 10.f : -1.f;
    case 11: return IsSoupId(id) ? 10.f + (float)sz : -1.f;
    case 12: return IsPotionId(id) ? 10.f + (float)sz : -1.f;
    case 13: {
        int t = PickTier(id);
        if (t <= 0) return -1.f;
        return t * 20.f + ench(32) * 4.f;
    }
    case 14: return IsFoodId(id) && !IsGappleId(id) && !IsSoupId(id) ? 5.f + (float)sz : -1.f;
    case 15: return IsFlintId(id) ? 10.f : -1.f;
    case 16: return IsWebId(id) ? 10.f + (float)sz : -1.f;
    default: return -1.f;
    }
}

static float ScoreArmor(JNIEnv* env, ItemStack* st, int piece) {
    if (!st || st->IsEmpty(env)) return -1.f;
    int id = st->GetItemId(env);
    JniOk(env);
    if (ArmorPiece(id) != piece) return -1.f;
    return ArmorTier(id) * 20.f + st->GetEnchantmentLevel(0, env) * 6.f
        + st->GetEnchantmentLevel(1, env) * 3.f
        + st->GetEnchantmentLevel(3, env) * 3.f
        + st->GetEnchantmentLevel(4, env) * 2.f
        + st->GetEnchantmentLevel(34, env);
}

static float DamageScore(JNIEnv* env, ItemStack* st) {
    if (!st || st->IsEmpty(env)) return -1.f;
    int id = st->GetItemId(env);
    JniOk(env);
    int sh = st->GetEnchantmentLevel(16, env);
    JniOk(env);
    int stier = SwordTier(id);
    int atier = AxeTier(id);
    if (stier <= 0 && atier <= 0) return -1.f;
    float dmg = stier > 0 ? (2.f + stier) : (1.f + atier);
    return dmg * 10.f + sh * 6.f + (stier > 0 ? 3.f : 0.f);
}

static void HoldShift() {
    if (s_shiftHeld) return;
    keybd_event(VK_SHIFT, (BYTE)MapVirtualKey(VK_SHIFT, 0), 0, 0);
    s_shiftHeld = true;
}

static void ReleaseShift() {
    if (!s_shiftHeld) return;
    keybd_event(VK_SHIFT, (BYTE)MapVirtualKey(VK_SHIFT, 0), KEYEVENTF_KEYUP, 0);
    s_shiftHeld = false;
}

static jobject GetFieldObj(JNIEnv* env, jobject obj, const char* name, const char* sig) {
    if (!env || !obj || !name || !sig) return nullptr;
    jclass c = env->GetObjectClass(obj);
    if (!c) return nullptr;
    jfieldID f = env->GetFieldID(c, name, sig);
    JniOk(env);
    jobject r = nullptr;
    if (f) {
        r = env->GetObjectField(obj, f);
        JniOk(env);
    }
    env->DeleteLocalRef(c);
    return r;
}

static jobject GetContainerFromScreen(JNIEnv* env, jobject screen) {
    std::string contSig = Mapper::Get("net/minecraft/inventory/Container", 2);
    if (contSig.empty()) return nullptr;
    std::string name = Mapper::Get("inventorySlots");
    if (name.empty()) name = "inventorySlots";
    return GetFieldObj(env, screen, name.c_str(), contSig.c_str());
}

static jobject GetSlotList(JNIEnv* env, jobject container) {
    return GetFieldObj(env, container, Mapper::Get("inventorySlots").c_str(), "Ljava/util/List;");
}

static jobject ListGet(JNIEnv* env, jobject list, int i) {
    if (!list) return nullptr;
    jclass lc = env->FindClass("java/util/List");
    if (!lc) return nullptr;
    jmethodID m = env->GetMethodID(lc, "get", "(I)Ljava/lang/Object;");
    env->DeleteLocalRef(lc);
    if (!m) return nullptr;
    jobject r = env->CallObjectMethod(list, m, i);
    JniOk(env);
    return r;
}

static int FieldInt(JNIEnv* env, jobject obj, const char* name) {
    if (!obj || !name) return 0;
    jclass c = env->GetObjectClass(obj);
    if (!c) return 0;
    jfieldID f = env->GetFieldID(c, name, "I");
    JniOk(env);
    int v = 0;
    if (f) v = env->GetIntField(obj, f);
    env->DeleteLocalRef(c);
    return v;
}

static int CeilD(double v) {
    int i = (int)v;
    return v > (double)i ? i + 1 : i;
}

struct ScaledRes { int scaledWidth; int scaledHeight; int scaleFactor; };

static ScaledRes CalcScale(int dw, int dh, int guiScale) {
    ScaledRes d{};
    d.scaledWidth = dw;
    d.scaledHeight = dh;
    d.scaleFactor = 1;
    int i = guiScale == 0 ? 1000 : guiScale;
    while (d.scaleFactor < i &&
        d.scaledWidth / (d.scaleFactor + 1) >= 320 &&
        d.scaledHeight / (d.scaleFactor + 1) >= 240)
        ++d.scaleFactor;
    d.scaledWidth = CeilD((double)dw / (double)d.scaleFactor);
    d.scaledHeight = CeilD((double)dh / (double)d.scaleFactor);
    return d;
}

static void GetGuiOrigin(JNIEnv* env, jobject screen, const ScaledRes& sr, int& guiLeft, int& guiTop) {
    guiLeft = (sr.scaledWidth - 176) / 2;
    guiTop = (sr.scaledHeight - 166) / 2;
    if (!screen) return;
    Klass* scls = (Klass*)env->GetObjectClass(screen);
    if (!scls) return;
    Field* fl = scls->GetField(env, Mapper::Get("guiLeft").c_str(), "I");
    Field* ft = scls->GetField(env, Mapper::Get("guiTop").c_str(), "I");
    env->DeleteLocalRef((jclass)scls);
    if (fl && ft) {
        int gl = fl->GetIntField(env, screen);
        int gt = ft->GetIntField(env, screen);
        if (gl >= 0 && gt >= 0 && gl < sr.scaledWidth && gt < sr.scaledHeight) {
            guiLeft = gl;
            guiTop = gt;
        }
    }
}

static void GetSlotDisplay(JNIEnv* env, jobject container, int slot, int& x, int& y) {
    x = 8 + (slot % 9) * 18;
    y = 84 + (slot / 9) * 18;
    jobject list = GetSlotList(env, container);
    jobject so = ListGet(env, list, slot);
    if (so) {
        std::string xn = Mapper::Get("xDisplayPosition");
        std::string yn = Mapper::Get("yDisplayPosition");
        if (xn.empty()) xn = "xDisplayPosition";
        if (yn.empty()) yn = "yDisplayPosition";
        x = FieldInt(env, so, xn.c_str());
        y = FieldInt(env, so, yn.c_str());
        env->DeleteLocalRef(so);
    }
    if (list) env->DeleteLocalRef(list);
}

static void SmoothMouseMove(int targetX, int targetY) {
    POINT cur{};
    GetCursorPos(&cur);
    int dx = targetX - cur.x;
    int dy = targetY - cur.y;
    double dist = sqrt((double)dx * dx + (double)dy * dy);
    if (dist < 5.0) {
        SetCursorPos(targetX, targetY);
        return;
    }
    int steps = (int)(dist / 16.0);
    steps = (std::max)(4, (std::min)(steps, 18));
    int startX = cur.x, startY = cur.y;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float ease = t * t * (3.f - 2.f * t);
        int x = startX + (int)(dx * ease);
        int y = startY + (int)(dy * ease);
        if (i < steps) {
            std::uniform_int_distribution jitter(-1, 1);
            x += jitter(g_rng);
            y += jitter(g_rng);
        }
        SetCursorPos(x, y);
        Sleep(2);
    }
    SetCursorPos(targetX, targetY);
}

static bool SlotToScreen(JNIEnv* env, jobject screen, jobject container, int slot, int& outX, int& outY) {
    int dw = Minecraft::GetDisplayWidth(env);
    int dh = Minecraft::GetDisplayHeight(env);
    if (dw <= 0 || dh <= 0) return false;
    int guiScale = 0;
    jobject gs = Minecraft::GetGameSettings(env);
    if (gs) {
        guiScale = ((GameSettings*)gs)->GetGuiScale(env);
        env->DeleteLocalRef(gs);
    }
    ScaledRes sr = CalcScale(dw, dh, guiScale);
    int guiLeft = 0, guiTop = 0, slotX = 0, slotY = 0;
    GetGuiOrigin(env, screen, sr, guiLeft, guiTop);
    GetSlotDisplay(env, container, slot, slotX, slotY);
    int sx = guiLeft + slotX + 8;
    int sy = guiTop + slotY + 8;
    double scaleX = (double)dw / (double)sr.scaledWidth;
    double scaleY = (double)dh / (double)sr.scaledHeight;
    outX = (int)(sx * scaleX);
    outY = (int)(sy * scaleY);
    if (!Minecraft::IsFullscreen(env)) {
        HWND wnd = FindLunarWindow();
        if (wnd) {
            POINT o{ 0, 0 };
            ClientToScreen(wnd, &o);
            outX += o.x;
            outY += o.y;
        }
    }
    std::uniform_int_distribution off(-3, 3);
    outX += off(g_rng);
    outY += off(g_rng);
    return true;
}

static void ShiftClickSlot(JNIEnv* env, jobject screen, jobject container, int slot) {
    int x = 0, y = 0;
    if (!SlotToScreen(env, screen, container, slot, x, y))
        return;
    SmoothMouseMove(x, y);
    HoldShift();
    Sleep(18);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(12);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

static int BaseDelay() {
    int sp = (std::max)(1, (std::min)(10, InvManagerSettings::speed));
    int d = 20 + (10 - sp) * 25;
    if (InvManagerSettings::randomize)
        d = (int)(d * (RandRange(70, 130) / 100.f));
    return (std::max)(10, d);
}

static int DelayFor(int slot) {
    int d = BaseDelay();
    if (InvManagerSettings::smartSpeed && s_lastSlot >= 0 && slot >= 0) {
        int ax = s_lastSlot % 9, ay = s_lastSlot / 9;
        int bx = slot % 9, by = slot / 9;
        int dist = abs(ax - bx) + abs(ay - by);
        if (dist <= 1) d = (int)(d * 0.45f);
        else if (dist >= 6) d = (int)(d * 1.45f);
    }
    return (std::max)(10, d);
}

static jobject StackAt(JNIEnv* env, InventoryPlayer* inv, int slot) {
    jobject s = inv->GetStackInSlot(slot, env);
    JniOk(env);
    return s;
}

static void Reset() {
    ReleaseShift();
    s_open = false;
    s_next = 0;
    s_lastSlot = -1;
}

static bool FindBest(JNIEnv* env, InventoryPlayer* inv, int want, const bool used[36], int& outSlot, float& outScore, bool fallback) {
    outSlot = -1;
    outScore = -1.f;
    for (int i = 0; i < 36; i++) {
        if (used[i]) continue;
        jobject o = StackAt(env, inv, i);
        if (!o) continue;
        float sc = ScoreStack(env, (ItemStack*)o, want);
        if (fallback && sc < 0.f) {
            if (want == 1) sc = DamageScore(env, (ItemStack*)o);
            else if (want == 7) {
                if (ScoreStack(env, (ItemStack*)o, 8) > 0.f) sc = 5.f + (float)((ItemStack*)o)->GetStackSize(env);
            } else if (want == 8) {
                if (ScoreStack(env, (ItemStack*)o, 7) > 0.f) sc = 5.f;
            }
        }
        env->DeleteLocalRef(o);
        if (sc > outScore) {
            outScore = sc;
            outSlot = i;
        }
    }
    return outSlot >= 0 && outScore >= 0.f;
}

static bool TryArmor(JNIEnv* env, InventoryPlayer* inv, jobject screen, jobject container) {
    for (int piece = 0; piece < 4; piece++) {
        int armorIdx = 3 - piece;
        jobject eq = inv->GetArmorItem(armorIdx, env);
        JniOk(env);
        float eqSc = ScoreArmor(env, eq ? (ItemStack*)eq : nullptr, piece);
        if (eq) env->DeleteLocalRef(eq);

        int best = -1;
        float bestSc = eqSc;
        for (int i = 0; i < 36; i++) {
            jobject o = StackAt(env, inv, i);
            if (!o) continue;
            float sc = ScoreArmor(env, (ItemStack*)o, piece);
            env->DeleteLocalRef(o);
            if (sc > bestSc) {
                bestSc = sc;
                best = i;
            }
        }
        if (best < 0) continue;
        int cont = InvToCont(best);
        if (cont < 0) continue;
        ShiftClickSlot(env, screen, container, cont);
        s_lastSlot = best;
        s_next = GetTickCount64() + DelayFor(best);
        return true;
    }
    return false;
}

static bool TryHotbar(JNIEnv* env, InventoryPlayer* inv, jobject screen, jobject container) {
    bool used[36] = {};
    for (int hb = 0; hb < 9; hb++) {
        int want = InvManagerSettings::hotbar[hb];
        if (want <= 0) continue;

        int cur = -1;
        float curSc = -1.f;
        jobject ho = StackAt(env, inv, hb);
        if (ho) {
            curSc = ScoreStack(env, (ItemStack*)ho, want);
            if (InvManagerSettings::smartFallbacks && curSc < 0.f) {
                if (want == 1) curSc = DamageScore(env, (ItemStack*)ho);
                else if (want == 7 && ScoreStack(env, (ItemStack*)ho, 8) > 0.f) curSc = 4.f;
                else if (want == 8 && ScoreStack(env, (ItemStack*)ho, 7) > 0.f) curSc = 4.f;
            }
            env->DeleteLocalRef(ho);
            if (curSc >= 0.f) {
                cur = hb;
                used[hb] = true;
            }
        }

        int best = -1;
        float bestSc = -1.f;
        if (!FindBest(env, inv, want, used, best, bestSc, InvManagerSettings::smartFallbacks)) {
            if (cur >= 0) used[cur] = true;
            continue;
        }

        if (cur >= 0 && curSc + 0.01f >= bestSc) {
            used[cur] = true;
            continue;
        }

        if (best == hb) {
            used[hb] = true;
            continue;
        }

        int clickInv = best;
        if (cur >= 0 && curSc < bestSc)
            clickInv = hb;

        int cont = InvToCont(clickInv);
        if (cont < 0) continue;
        ShiftClickSlot(env, screen, container, cont);
        used[hb] = true;
        s_lastSlot = clickInv;
        s_next = GetTickCount64() + DelayFor(clickInv);
        return true;
    }
    return false;
}

void InvManager::Run(JNIEnv* env) {
    if (!enabled || !env) {
        Reset();
        Sleep(20);
        return;
    }
    if (Overlay::isOpen) {
        ReleaseShift();
        Sleep(20);
        return;
    }

    jobject screen = Minecraft::GetCurrentScreen(env);
    JniOk(env);
    bool invOpen = screen && ((GuiScreen*)screen)->IsInventory(env);
    if (!invOpen) {
        if (screen) env->DeleteLocalRef(screen);
        Reset();
        Sleep(15);
        return;
    }

    ULONGLONG now = GetTickCount64();
    if (!s_open) {
        s_open = true;
        int d = InvManagerSettings::delayAfterOpen;
        if (InvManagerSettings::randomize)
            d = (int)(d * (RandRange(70, 130) / 100.f));
        s_next = now + (ULONGLONG)(std::max)(0, d);
        s_lastSlot = -1;
    }
    if (now < s_next) {
        env->DeleteLocalRef(screen);
        Sleep(5);
        return;
    }

    jobject player = Minecraft::GetThePlayer(env);
    JniOk(env);
    if (!player) {
        env->DeleteLocalRef(screen);
        Sleep(10);
        return;
    }
    jobject invObj = ((Player*)player)->GetInventoryPlayer(env);
    JniOk(env);
    if (!invObj) {
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(screen);
        Sleep(10);
        return;
    }
    auto* inv = (InventoryPlayer*)invObj;
    jobject cont = GetContainerFromScreen(env, screen);
    if (!cont)
        cont = ((Player*)player)->GetOpenContainer(env);
    JniOk(env);
    if (!cont) {
        env->DeleteLocalRef(invObj);
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(screen);
        Sleep(10);
        return;
    }

    bool did = false;
    if (InvManagerSettings::equipArmor)
        did = TryArmor(env, inv, screen, cont);
    if (!did && InvManagerSettings::sortHotbar)
        did = TryHotbar(env, inv, screen, cont);

    env->DeleteLocalRef(cont);
    env->DeleteLocalRef(invObj);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(screen);

    if (!did) {
        ReleaseShift();
        s_next = now + 80;
    }
    Sleep(1);
}
