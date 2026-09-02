#include "pch.h"
#include "Menu.h"
#include <algorithm>
#include <unordered_map>
#include <functional>

#include "Combat/Clicker.h"
#include "Combat/AimAssist.h"
#include "Combat/velocity.h"
#include "Misc/Friends.h"
#include "Misc/Enemies.h"
#include "../Config.h"
#include "Misc/FastPlace.h"
#include "Misc/FastBreak.h"
#include "Misc/TickLocker.h"
#include "Misc/InvWalk.h"
#include "Misc/FastStop.h"
#include "Misc/NoJumpDelay.h"
#include "Misc/QuickAccel.h"
#include "Misc/SnapTap.h"
#include "Combat/AutoRefill.h"
#include "Combat/Throw.h"
#include "Combat/Piercing.h"
#include "Combat/KeepSprint.h"
#include "Combat/Criticals.h"
#include "Combat/AutoRod.h"
#include "Combat/AntiBot.h"
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
#include "Misc/Overlay.h"
#include "Misc/Scroll.h"
#include "Misc/armor.h"
#include "Module.h"
#include "../../Cheat/Modules/Settings.h"
#include "../../Helper/Communication.h"
#include "../../Helper/Utils.h"

#include "../../../vendors/imgui/imgui.h"
#include <cmath>

extern void Armor_Request_Scan();
extern void Armor_Trigger_Manual();

static ULONGLONG g_bindReleasedAt = 0;

// ====================================================================
//  THEME — dark cards, orange accent (Snap-style)
// ====================================================================
static const ImVec4 AC = { 1.00f, 0.548f, 0.220f, 1.00f };
static const ImVec4 AC_H = { 1.00f, 0.66f, 0.36f, 1.00f };
static const ImVec4 AC_DIM = { 0.86f, 0.42f, 0.14f, 1.00f };

static const ImVec4 BG0 = { 0.102f, 0.102f, 0.102f, 0.94f };
static const ImVec4 BG1 = { 0.13f, 0.13f, 0.13f, 0.96f };
static const ImVec4 BG2 = { 0.18f, 0.18f, 0.18f, 1.00f };
static const ImVec4 BG3 = { 0.24f, 0.24f, 0.24f, 1.00f };
static const ImVec4 BG_CARD = { 0.145f, 0.145f, 0.145f, 1.00f };

static const ImVec4 TEXT = { 0.92f, 0.92f, 0.92f, 1.00f };
static const ImVec4 TEXT_DIM = { 0.55f, 0.55f, 0.55f, 1.00f };
static const ImVec4 BORDER = { 0.18f, 0.18f, 0.18f, 0.00f };
static const ImVec4 BORDER_AC = { 0.28f, 0.28f, 0.28f, 0.00f };

static const ImVec4 RED = { 0.81f, 0.36f, 0.36f, 1.00f };
static const ImVec4 RED_H = { 0.92f, 0.46f, 0.46f, 1.00f };

static void ApplyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding = 16.f;
    s.ChildRounding = 12.f;
    s.FrameRounding = 14.f;
    s.GrabRounding = 12.f;
    s.PopupRounding = 8.f;
    s.ScrollbarRounding = 6.f;
    s.TabRounding = 12.f;

    s.FramePadding = { 9.f, 5.f };
    s.ItemSpacing = { 8.f, 8.f };
    s.WindowPadding = { 16.f, 14.f };
    s.WindowBorderSize = 0.f;
    s.ChildBorderSize = 0.f;
    s.ScrollbarSize = 4.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = BG0;
    c[ImGuiCol_ChildBg] = BG1;
    c[ImGuiCol_Border] = BORDER;
    c[ImGuiCol_FrameBg] = BG1;
    c[ImGuiCol_FrameBgHovered] = BG2;
    c[ImGuiCol_FrameBgActive] = BG3;
    c[ImGuiCol_SliderGrab] = AC;
    c[ImGuiCol_SliderGrabActive] = AC_H;
    c[ImGuiCol_CheckMark] = AC;
    c[ImGuiCol_Button] = BG2;
    c[ImGuiCol_ButtonHovered] = BG3;
    c[ImGuiCol_ButtonActive] = AC_DIM;
    c[ImGuiCol_Header] = BG2;
    c[ImGuiCol_HeaderHovered] = BG3;
    c[ImGuiCol_HeaderActive] = AC_DIM;
    c[ImGuiCol_Tab] = BG0;
    c[ImGuiCol_TabHovered] = BG2;
    c[ImGuiCol_TabActive] = BG3;
    c[ImGuiCol_TitleBg] = BG0;
    c[ImGuiCol_TitleBgActive] = BG0;
    c[ImGuiCol_Separator] = { 0.20f, 0.20f, 0.20f, 1.f };
    c[ImGuiCol_Text] = TEXT;
    c[ImGuiCol_TextDisabled] = TEXT_DIM;
    c[ImGuiCol_ScrollbarBg] = BG0;
    c[ImGuiCol_ScrollbarGrab] = BG2;
    c[ImGuiCol_ScrollbarGrabHovered] = BG3;
    c[ImGuiCol_ScrollbarGrabActive] = AC_DIM;
    c[ImGuiCol_PopupBg] = BG1;
}

// ====================================================================
//  HELPERS
// ====================================================================
static bool AnyListening() {
    return MenuBinds::aa_listening || MenuBinds::lc_listening
        || MenuBinds::vel_listening || MenuBinds::al_listening
        || MenuBinds::ch_listening
        || MenuBinds::esp_listening
        || MenuBinds::itemesp_listening
        || MenuBinds::pesp_listening
        || MenuBinds::sesp_listening
        || MenuBinds::ntag_listening
        || MenuBinds::tr_listening
        || MenuBinds::tj_listening
        || MenuBinds::prc_listening
        || MenuBinds::ks_listening
        || MenuBinds::cr_listening
        || MenuBinds::rod_listening
        || MenuBinds::ab_listening
        || AimAssistSettings::keepBindListening
        || MenuBinds::ar_listening
        || ThrowSettings::potListening || ThrowSettings::soupListening
        || ThrowSettings::debuffListening || ThrowSettings::pearlListening
        || MenuBinds::notif_listening
        || MenuBinds::fr_listening
        || MenuBinds::en_listening
        || MenuBinds::fp_listening
        || MenuBinds::fb_listening
        || MenuBinds::tl_listening
        || MenuBinds::tl_target_listening
        || MenuBinds::iw_listening
        || MenuBinds::fs_listening
        || MenuBinds::njd_listening
        || MenuBinds::qa_listening
        || MenuBinds::st_listening
        || MenuBinds::destruct_listening || MenuBinds::open_listening
        || Scroll::scroll_listen
        || Armor::listening
        || FriendsSettings::addFriendListening
        || FriendsSettings::addNearbyListening
        || FriendsSettings::clearListening
        || EnemiesSettings::addEnemyListening
        || EnemiesSettings::addNearbyListening
        || EnemiesSettings::clearListening;
}

void ClientMenu::DrawBindButton(const char* id, int& key, bool& listening) {
    std::string text = listening
        ? "..."
        : (key == 0 ? "NONE" : MenuBinds::VKToString(key));
    ImVec2 ts = ImGui::CalcTextSize(text.c_str());
    float bw = (std::max)(ts.x + 14.f, 36.f);
    float bh = 18.f;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg = listening ? IM_COL32(70, 48, 20, 255) : IM_COL32(48, 48, 48, 255);
    ImU32 fg = listening ? IM_COL32(255, 170, 60, 255) : IM_COL32(170, 170, 170, 255);

    if (ImGui::InvisibleButton(id, { bw, bh }) && !listening)
        listening = true;
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    dl->AddRectFilled(pos, { pos.x + bw, pos.y + bh }, bg, 6.f);
    dl->AddText({ pos.x + (bw - ts.x) * 0.5f, pos.y + (bh - ts.y) * 0.5f }, fg, text.c_str());

    if (listening) {
        for (int vk = 1; vk < 256; vk++) {
            if (vk == VK_LBUTTON || vk == VK_RBUTTON) continue;
            if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU
                || vk == VK_LSHIFT
                || vk == VK_LCONTROL || vk == VK_RCONTROL) continue;
            if (vk == VK_F4 && (GetAsyncKeyState(VK_MENU) & 0x8000)) continue;
            if (vk == VK_ESCAPE) {
                if (GetAsyncKeyState(vk) & 0x8000) { key = 0; listening = false; g_bindReleasedAt = GetTickCount64(); }
                continue;
            }
            if (GetAsyncKeyState(vk) & 0x8000) { key = vk; listening = false; g_bindReleasedAt = GetTickCount64(); break; }
        }
    }
}

void ClientMenu::SectionHeader(const char* label) {
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::Spacing();
}

static float AnimTowards(ImGuiID id, float target, float speed) {
    static std::unordered_map<ImGuiID, float> s_anim;
    auto it = s_anim.find(id);
    if (it == s_anim.end()) {
        s_anim[id] = target;
        return target;
    }
    float& v = it->second;
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.f || dt > 0.05f) dt = 1.f / 60.f;
    v += (target - v) * (1.f - expf(-speed * dt));
    if (fabsf(v - target) < 0.0015f) v = target;
    return v;
}

static ImU32 MixU32(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    auto ch = [](ImU32 c, int s) { return (int)((c >> s) & 0xFFu); };
    int r = (int)(ch(a, 0) + (ch(b, 0) - ch(a, 0)) * t);
    int g = (int)(ch(a, 8) + (ch(b, 8) - ch(a, 8)) * t);
    int bl = (int)(ch(a, 16) + (ch(b, 16) - ch(a, 16)) * t);
    int al = (int)(ch(a, 24) + (ch(b, 24) - ch(a, 24)) * t);
    return IM_COL32(r, g, bl, al);
}

static bool PhantomToggle(const char* id, bool& val) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    const float w = 36.f, h = 18.f, r = h * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool clicked = ImGui::InvisibleButton(id, { w, h });
    if (clicked) val = !val;
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    const ImGuiID iid = ImGui::GetItemID();
    const float t = AnimTowards(iid, val ? 1.f : 0.f, 16.f);
    const ImU32 bgOff = IM_COL32(46, 46, 46, 255);
    const ImU32 bgOn = ImGui::ColorConvertFloat4ToU32(AC);
    dl->AddRectFilled(pos, { pos.x + w, pos.y + h }, MixU32(bgOff, bgOn, t), r);

    const float knr = r - 2.4f;
    const float kx0 = pos.x + r + 0.4f;
    const float kx1 = pos.x + w - r - 0.4f;
    const float kx = kx0 + (kx1 - kx0) * t;
    dl->AddCircleFilled({ kx, pos.y + r }, knr, IM_COL32(245, 245, 245, 255));
    return clicked;
}

// ====================================================================
//  MODULE HEADER (ancien, conserve pour ArrayList / modules simples)
// ====================================================================
bool ClientMenu::ModuleHeader(const char* label, bool& enabled, int& bind, bool& listening, float W, const char* desc) {
    (void)W;
    return SnapCard(label, desc, &enabled, &bind, &listening);
}

bool ClientMenu::ExpandButton(const char* label) {
    bool& expanded = m_expanded[label];
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.f, 0.f, 0.f, 0.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG2);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, BG3);
    ImGui::PushStyleColor(ImGuiCol_Text, expanded ? TEXT : TEXT_DIM);
    if (ImGui::Button(expanded ? "-##ei" : "+##ei", { 26.f, 26.f }))
        expanded = !expanded;
    ImGui::PopStyleColor(4);
    return expanded;
}

bool ClientMenu::SnapCard(const char* name, const char* desc, bool* enabled, int* bind, bool* listening) {
    bool& expanded = m_expanded[name];
    float W = ImGui::GetContentRegionAvail().x;
    const float cardH = 64.f;
    bool on = enabled && *enabled;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.f, 0.f, 0.f, 0.f });
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.f);
    ImGui::BeginChild((std::string("##snap_") + name).c_str(),
        { W, cardH }, false, ImGuiWindowFlags_NoScrollbar);

    const float cw = ImGui::GetWindowSize().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    dl->AddRectFilled(wp, { wp.x + cw, wp.y + cardH },
        ImGui::ColorConvertFloat4ToU32(BG_CARD), 12.f);

    ImGui::SetCursorPos({ 14.f, 10.f });
    ImGui::TextColored(on ? TEXT : ImVec4{ 0.72f, 0.72f, 0.72f, 1.f }, "%s", name);

    if (bind && listening) {
        ImGui::SameLine(0.f, 8.f);
        ImGui::SetCursorPosY(10.f);
        DrawBindButton((std::string("bdg_") + name).c_str(), *bind, *listening);
    }

    if (enabled) {
        ImGui::SetCursorPos({ cw - 48.f, 12.f });
        PhantomToggle((std::string("##tog_") + name).c_str(), *enabled);
    }

    ImGui::SetCursorPos({ 14.f, 36.f });
    ImVec2 plusPos = ImGui::GetCursorScreenPos();
    std::string plusId = std::string("##plus_") + name;
    if (ImGui::InvisibleButton(plusId.c_str(), { 18.f, 18.f }))
        expanded = !expanded;
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    dl->AddRectFilled(plusPos, { plusPos.x + 18.f, plusPos.y + 18.f },
        IM_COL32(32, 32, 32, 255), 4.f);
    ImU32 plusCol = expanded ? IM_COL32(230, 230, 230, 255) : IM_COL32(200, 200, 200, 255);
    dl->AddRectFilled({ plusPos.x + 4.f, plusPos.y + 8.f }, { plusPos.x + 14.f, plusPos.y + 10.f }, plusCol, 1.f);
    if (!expanded)
        dl->AddRectFilled({ plusPos.x + 8.f, plusPos.y + 4.f }, { plusPos.x + 10.f, plusPos.y + 14.f }, plusCol, 1.f);

    if (desc && desc[0]) {
        ImGui::SetCursorPos({ 38.f, 38.f });
        ImGui::PushTextWrapPos(cw - 16.f);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::TextUnformatted(desc);
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::Dummy({ 0.f, 6.f });
    return expanded;
}

static bool PhantomSliderCore(const char* label, const char* id, float& v, float mn, float mx, const char* fmt, bool asInt) {
    if (mx < mn) std::swap(mn, mx);
    v = std::clamp(v, mn, mx);

    ImGui::PushID(id);
    const float W = ImGui::GetContentRegionAvail().x;
    const float headerH = 16.f;
    const float bodyH = 18.f;
    const float totalH = headerH + bodyH;
    const float trackH = 2.4f;
    const float knobR = 5.5f;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("##sl", { W, totalH });
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    if (hovered || active) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    const float span = mx - mn;
    float t = span > 0.f ? (v - mn) / span : 0.f;
    t = std::clamp(t, 0.f, 1.f);

    const float trackY = origin.y + headerH + bodyH * 0.55f;
    const float trackL = origin.x + knobR;
    const float trackR = origin.x + W - knobR;
    const float usable = trackR - trackL;

    if (active && usable > 0.f) {
        float nt = (ImGui::GetIO().MousePos.x - trackL) / usable;
        nt = std::clamp(nt, 0.f, 1.f);
        v = mn + nt * span;
        if (asInt) v = (float)(int)lroundf(v);
        t = span > 0.f ? std::clamp((v - mn) / span, 0.f, 1.f) : 0.f;
    }

    const ImU32 colTrack = IM_COL32(58, 58, 58, 255);
    const ImU32 colFill = ImGui::ColorConvertFloat4ToU32(AC);
    const float fillX = trackL + t * usable;

    dl->AddRectFilled({ trackL, trackY - trackH * 0.5f }, { trackR, trackY + trackH * 0.5f }, colTrack, 2.f);
    if (t > 0.001f)
        dl->AddRectFilled({ trackL, trackY - trackH * 0.5f }, { fillX, trackY + trackH * 0.5f }, colFill, 2.f);

    const float hoverT = AnimTowards(ImGui::GetItemID() ^ 0x51u, (hovered || active) ? 1.f : 0.f, 18.f);
    const float r = knobR + hoverT * 1.4f;
    dl->AddCircleFilled({ fillX, trackY }, r + 1.2f, IM_COL32(255, 140, 50, 70));
    dl->AddCircleFilled({ fillX, trackY }, r, colFill);

    char buf[64];
    if (asInt) snprintf(buf, sizeof(buf), "%d", (int)lroundf(v));
    else snprintf(buf, sizeof(buf), fmt, v);
    const ImVec2 vs = ImGui::CalcTextSize(buf);
    const ImVec2 ls = ImGui::CalcTextSize(label);
    ImVec2 vp{ fillX - vs.x * 0.5f, origin.y };
    vp.x = std::clamp(vp.x, origin.x + ls.x + 10.f, origin.x + W - vs.x);
    dl->AddText(vp, IM_COL32(190, 190, 190, 230), buf);

    dl->AddText(origin, ImGui::ColorConvertFloat4ToU32(TEXT), label);

    ImGui::PopID();
    ImGui::Spacing();
    return active;
}

static void PhantomSliderFloat(const char* label, const char* id, float& v, float mn, float mx, const char* fmt = "%.2f") {
    PhantomSliderCore(label, id, v, mn, mx, fmt, false);
}
static void PhantomSliderInt(const char* label, const char* id, int& v, int mn, int mx) {
    float f = (float)v;
    PhantomSliderCore(label, id, f, (float)mn, (float)mx, "%d", true);
    v = (int)lroundf(f);
}
static void PhantomCombo(const char* label, const char* id, int& v, const char** items, int count) {
    ImGui::TextColored(TEXT, "%s", label);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG2);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, BG3);
    ImGui::PushStyleColor(ImGuiCol_Button, BG2);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, BG1);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
    ImGui::Combo(id, &v, items, count);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    ImGui::Spacing();
}

static bool PhantomRangeSliderFloat(const char* id, float& vMin, float& vMax,
    float rangeMin, float rangeMax)
{
    if (vMin > vMax) std::swap(vMin, vMax);
    vMin = std::clamp(vMin, rangeMin, rangeMax);
    vMax = std::clamp(vMax, rangeMin, rangeMax);

    ImGui::TextColored(TEXT, "%.0f° - %.0f°", vMin, vMax);

    ImGui::PushID(id);
    const float w = ImGui::GetContentRegionAvail().x - 2.f;
    const float h = 18.f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("##track", { w, h });
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    const float span = rangeMax - rangeMin;
    auto toT = [&](float v) { return span > 0.f ? (v - rangeMin) / span : 0.f; };
    auto toV = [&](float t) { return rangeMin + std::clamp(t, 0.f, 1.f) * span; };

    float tMin = toT(vMin);
    float tMax = toT(vMax);
    const float grabR = 5.5f;
    const float trackH = 2.4f;
    const float cy = pos.y + h * 0.5f;
    const float trackL = pos.x + grabR;
    const float usable = w - grabR * 2.f;

    const ImU32 colFill = ImGui::ColorConvertFloat4ToU32(AC);

    dl->AddRectFilled({ trackL, cy - trackH * 0.5f }, { trackL + usable, cy + trackH * 0.5f },
        IM_COL32(58, 58, 58, 255), 2.f);
    dl->AddRectFilled(
        { trackL + tMin * usable, cy - trackH * 0.5f },
        { trackL + tMax * usable, cy + trackH * 0.5f },
        colFill, 2.f);

    auto drawGrab = [&](float t, bool hot) {
        const float cx = trackL + t * usable;
        dl->AddCircleFilled({ cx, cy }, grabR + (hot ? 1.4f : 0.f), colFill);
    };

    bool changed = false;
    static int s_activeGrab = -1;
    static ImGuiID s_activeId = 0;

    const ImGuiID wid = ImGui::GetID("##track");
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool held = ImGui::IsMouseDown(0);

    if (ImGui::IsItemActivated()) {
        const float dMin = fabsf(mouse.x - (trackL + tMin * usable));
        const float dMax = fabsf(mouse.x - (trackL + tMax * usable));
        s_activeGrab = (dMin <= dMax) ? 0 : 1;
        s_activeId = wid;
    }
    if (!held || s_activeId != wid)
        s_activeGrab = -1;

    if (s_activeGrab >= 0 && s_activeId == wid) {
        float t = usable > 0.f ? (mouse.x - trackL) / usable : 0.f;
        if (s_activeGrab == 0) {
            tMin = std::clamp(t, 0.f, tMax);
            vMin = toV(tMin);
            changed = true;
        }
        else {
            tMax = std::clamp(t, tMin, 1.f);
            vMax = toV(tMax);
            changed = true;
        }
    }

    drawGrab(tMin, s_activeGrab == 0);
    drawGrab(tMax, s_activeGrab == 1);

    ImGui::PopID();
    ImGui::Spacing();
    return changed;
}
static void PhantomToggleRow(const char* id, const char* label, bool& v) {
    float W = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(TEXT, "%s", label);
    ImGui::SameLine(W - 40.f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.f);
    PhantomToggle(id, v);
    ImGui::Spacing();
}

// ====================================================================
//  TABS
// ====================================================================

void ClientMenu::RenderCombatTab()
{
    float W = ImGui::GetContentRegionAvail().x;
    float colW = (W - 10.f) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.f, 0.f, 0.f, 0.f });
    ImGui::BeginChild("##col_left", { colW, 0.f }, false);

    {
        AimAssist* aa = nullptr;
        for (auto* m : Modules::GetRegisteredModules())
            if ((aa = dynamic_cast<AimAssist*>(m))) break;

        bool open = SnapCard("Aim Assist", "Pulls your crosshair towards other players.",
            aa ? &aa->enabled : nullptr, &MenuBinds::aa_bind, &MenuBinds::aa_listening);

        if (open && aa) {
            SectionHeader("Mode");
            {
                const char* modes[] = { "Blatant", "Legit" };
                PhantomCombo("Mode", "##aa_mode", AimAssistSettings::currentMode, modes, 2);
            }
            SectionHeader("Speed");
            PhantomSliderFloat("Speed", "##aa_spd", AimAssistSettings::speed, 1.f, 10.f, "%.1f");
            SectionHeader("FOV");
            PhantomRangeSliderFloat("##aa_fov", AimAssistSettings::fovMin,
                AimAssistSettings::fovMax, 0.f, 360.f);
            if (AimAssistSettings::priority != 2) {
                SectionHeader("Distance");
                PhantomSliderFloat("Min", "##aa_dmin", AimAssistSettings::distanceMin, 0.f, AimAssistSettings::distanceMax, "%.1f");
                PhantomSliderFloat("Max", "##aa_dmax", AimAssistSettings::distanceMax, AimAssistSettings::distanceMin, 6.f, "%.1f");
            }
            SectionHeader("Priority");
            {
                const char* sorts[] = { "Distance", "FOV", "HurtTime" };
                PhantomCombo("Priority", "##aa_prio", AimAssistSettings::priority, sorts, 3);
            }
            SectionHeader("Targets");
            PhantomToggleRow("##aa_pl", "Players", AimAssistSettings::targetPlayers);
            PhantomToggleRow("##aa_inv", "Invisible", AimAssistSettings::allowInvisible);
            PhantomToggleRow("##aa_nk", "Naked", AimAssistSettings::allowNaked);
            SectionHeader("Conditions");
            PhantomToggleRow("##aa_clk", "Require click", AimAssistSettings::requireClick);
            PhantomToggleRow("##aa_wpn", "Weapons only", AimAssistSettings::weaponsOnly);
            PhantomToggleRow("##aa_blk", "Don't break blocks", AimAssistSettings::breakBlock);
            PhantomToggleRow("##aa_keep", "Keep on target", AimAssistSettings::keepOnTarget);
            if (AimAssistSettings::keepOnTarget) {
                ImGui::TextColored(TEXT_DIM, "Keep bind");
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 88.f);
                DrawBindButton("aa_keepbind", AimAssistSettings::keepOnTargetKeybind, AimAssistSettings::keepBindListening);
            }
            SectionHeader("Multipoint");
            PhantomSliderInt("Multipoint", "##aa_mp", AimAssistSettings::multipoint, 0, 100);
            SectionHeader("Bind");
            DrawBindButton("aa_bind", MenuBinds::aa_bind, MenuBinds::aa_listening);
            ImGui::Spacing();
        }
    }

    {
        bool open = SnapCard("Throw", "Throws pots, soup, pearls and debuffs.",
            &ThrowSettings::enabled, nullptr, nullptr);
        if (open) {
            SectionHeader("Health");
            PhantomToggleRow("##th_pot", "Enable Health", ThrowSettings::potEnabled);
            PhantomSliderFloat("Speed", "##th_pot_spd", ThrowSettings::potSpeed, 0.f, 10.f, "%.1f");
            PhantomToggleRow("##th_pot_sm", "Smart mode", ThrowSettings::potSmart);
            PhantomToggleRow("##th_pot_dbl", "Double throw", ThrowSettings::potDouble);
            ImGui::TextColored(TEXT_DIM, "Bind");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 88.f);
            DrawBindButton("th_pot_bind", ThrowSettings::potBind, ThrowSettings::potListening);
            SectionHeader("Soup");
            PhantomToggleRow("##th_soup", "Enable Soup", ThrowSettings::soupEnabled);
            PhantomSliderFloat("Speed", "##th_soup_spd", ThrowSettings::soupSpeed, 0.f, 10.f, "%.1f");
            PhantomToggleRow("##th_soup_sm", "Smart mode", ThrowSettings::soupSmart);
            PhantomToggleRow("##th_soup_dbl", "Double use", ThrowSettings::soupDouble);
            PhantomToggleRow("##th_soup_drop", "Auto drop", ThrowSettings::soupAutoDrop);
            ImGui::TextColored(TEXT_DIM, "Bind");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 88.f);
            DrawBindButton("th_soup_bind", ThrowSettings::soupBind, ThrowSettings::soupListening);
            SectionHeader("Debuff");
            PhantomToggleRow("##th_deb", "Enable Debuff", ThrowSettings::debuffEnabled);
            PhantomSliderFloat("Speed", "##th_deb_spd", ThrowSettings::debuffSpeed, 0.f, 10.f, "%.1f");
            PhantomToggleRow("##th_deb_dbl", "Double throw", ThrowSettings::debuffDouble);
            ImGui::TextColored(TEXT_DIM, "Bind");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 88.f);
            DrawBindButton("th_deb_bind", ThrowSettings::debuffBind, ThrowSettings::debuffListening);
            SectionHeader("Pearl");
            PhantomToggleRow("##th_pearl", "Enable Pearl", ThrowSettings::pearlEnabled);
            PhantomSliderFloat("Speed", "##th_pearl_spd", ThrowSettings::pearlSpeed, 0.f, 10.f, "%.1f");
            ImGui::TextColored(TEXT_DIM, "Bind");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 88.f);
            DrawBindButton("th_pearl_bind", ThrowSettings::pearlBind, ThrowSettings::pearlListening);
            ImGui::Spacing();
        }
    }

    {
        Piercing* pr = nullptr;
        for (auto* m : Modules::GetRegisteredModules())
            if ((pr = dynamic_cast<Piercing*>(m))) break;
        bool open = SnapCard("Piercing", "Allows hitting entities through obstructions.",
            pr ? &pr->enabled : nullptr, &MenuBinds::prc_bind, &MenuBinds::prc_listening);
        if (open && pr) {
            SectionHeader("Conditions");
            PhantomToggleRow("##pr_wpn", "Weapons only", PiercingSettings::weaponsOnly);
            PhantomToggleRow("##pr_blk", "Through blocks", PiercingSettings::throughBlock);
            PhantomToggleRow("##pr_en", "Enemies only", PiercingSettings::targetEnemiesOnly);
            SectionHeader("Bind");
            DrawBindButton("prc_bind", MenuBinds::prc_bind, MenuBinds::prc_listening);
            ImGui::Spacing();
        }
    }

    {
        KeepSprint* ks = nullptr;
        for (auto* m : Modules::GetRegisteredModules())
            if ((ks = dynamic_cast<KeepSprint*>(m))) break;
        bool open = SnapCard("KeepSprint", "Resets your sprint state to deal more knockback.",
            ks ? &ks->enabled : nullptr, &MenuBinds::ks_bind, &MenuBinds::ks_listening);
        if (open && ks) {
            SectionHeader("Mode");
            {
                const char* modes[] = { "Dynamic", "Static" };
                PhantomCombo("Mode", "##ks_mode", KeepSprintSettings::mode, modes, 2);
            }
            SectionHeader("Speed");
            PhantomSliderFloat("Speed", "##ks_spd", KeepSprintSettings::speed, 0.6f, 1.f, "%.2f");
            SectionHeader("Conditions");
            PhantomSliderInt("Chance (%)", "##ks_ch", KeepSprintSettings::chance, 0, 100);
            PhantomToggleRow("##ks_wpn", "Weapons only", KeepSprintSettings::weaponsOnly);
            PhantomToggleRow("##ks_beh", "Only on behind", KeepSprintSettings::onlyOnBehind);
            SectionHeader("Bind");
            DrawBindButton("ks_bind", MenuBinds::ks_bind, MenuBinds::ks_listening);
            ImGui::Spacing();
        }
    }

    {
        Criticals* cr = nullptr;
        for (auto* m : Modules::GetRegisteredModules())
            if ((cr = dynamic_cast<Criticals*>(m))) break;
        bool open = SnapCard("Criticals", "Increases chance of landing critical hits.",
            cr ? &cr->enabled : nullptr, &MenuBinds::cr_bind, &MenuBinds::cr_listening);
        if (open && cr) {
            SectionHeader("Mode");
            {
                const char* modes[] = { "Packet", "Timer" };
                PhantomCombo("Mode", "##cr_mode", CriticalsSettings::mode, modes, 2);
            }
            SectionHeader("Conditions");
            PhantomSliderInt("Chance (%)", "##cr_ch", CriticalsSettings::chance, 0, 100);
            if (CriticalsSettings::mode == 1)
                PhantomSliderFloat("Timer speed", "##cr_ts", CriticalsSettings::timerSpeed, 0.1f, 0.9f, "%.2f");
            else
                PhantomSliderInt("Max queue (ms)", "##cr_q", CriticalsSettings::maxQueueTime, 200, 1000);
            SectionHeader("Bind");
            DrawBindButton("cr_bind", MenuBinds::cr_bind, MenuBinds::cr_listening);
            ImGui::Spacing();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 10.f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.f, 0.f, 0.f, 0.f });
    ImGui::BeginChild("##col_right", { colW, 0.f }, false);

    {
        bool open = SnapCard("Auto Clicker", "Clicks for you when holding down left-click.",
            &Clicker::enabled, &MenuBinds::lc_bind, &MenuBinds::lc_listening);
        if (open) {
            SectionHeader("Mode");
            {
                const char* modes[] = { "Blatant", "Butterfly", "Jitter" };
                PhantomCombo("Mode", "##lc_mode", Clicker::mode, modes, 3);
            }
            SectionHeader("CPS");
            PhantomSliderInt("Clics par seconde", "##lc_cps", Clicker::cps, 5, 25);
            ImGui::TextColored(TEXT_DIM, "~%.1f ms entre clics", 1000.f / (float)Clicker::cps);
            ImGui::Spacing();
            if (Clicker::mode == 1 || Clicker::mode == 2) {
                SectionHeader("Randomisation");
                PhantomToggleRow("##lc_exh", "Exhaust", Clicker::exhaust);
            }
            SectionHeader("Conditions");
            PhantomToggleRow("##lc_req", "Require Click (LMB)", Clicker::requireClick);
            PhantomToggleRow("##lc_wpn", "Weapons Only", Clicker::weaponsOnly);
            SectionHeader("Bind");
            DrawBindButton("lc_bind", MenuBinds::lc_bind, MenuBinds::lc_listening);
        }
    }

    {
        Velocity* vel = nullptr;
        for (auto* m : Modules::GetRegisteredModules())
            if ((vel = dynamic_cast<Velocity*>(m))) break;
        bool open = SnapCard("Velocity", "Reduces the amount of knockback you take.",
            vel ? &vel->enabled : nullptr, &MenuBinds::vel_bind, &MenuBinds::vel_listening);
        if (open && vel) {
            SectionHeader("Mode");
            {
                const char* modes[] = { "Blatant", "Reverse", "Jump", "Reduce" };
                PhantomCombo("Mode", "##vel_mode", VelocitySettings::mode, modes, 4);
            }
            if (VelocitySettings::mode == 0) {
                SectionHeader("Knockback");
                PhantomSliderFloat("Horizontal (%)", "##vel_h", VelocitySettings::horizontal, 0.f, 100.f, "%.0f%%");
                PhantomSliderFloat("Vertical (%)", "##vel_v", VelocitySettings::vertical, 0.f, 100.f, "%.0f%%");
            } else if (VelocitySettings::mode == 1) {
                SectionHeader("Reverse");
                PhantomSliderFloat("Strength (%)", "##vel_rev", VelocitySettings::reverseStrength, 0.f, 100.f, "%.0f%%");
            } else if (VelocitySettings::mode == 2) {
                SectionHeader("Jump");
                PhantomSliderInt("Delay (ms)", "##vel_jdelay", VelocitySettings::jumpDelayMs, 0, 50);
            } else {
                SectionHeader("Reduce");
                PhantomSliderFloat("Strength (%)", "##vel_red", VelocitySettings::reduceH, 0.f, 100.f, "%.0f%%");
                PhantomToggleRow("##vel_agc", "AGC Bypass", VelocitySettings::agcBypass);
            }
            SectionHeader("Conditions");
            PhantomSliderInt("Chance (%)", "##vel_chance", VelocitySettings::chance, 0, 100);
            PhantomToggleRow("##vel_wpn", "Weapons only", VelocitySettings::weaponsOnly);
            PhantomToggleRow("##vel_fwd", "Only when moving forward", VelocitySettings::onlyWhenMovingForward);
            PhantomToggleRow("##vel_look", "Only looking at player", VelocitySettings::onlyLookingAtPlayer);
            PhantomToggleRow("##vel_lmb", "Only mouse pressed", VelocitySettings::onlyMousePressed);
            SectionHeader("Bind");
            DrawBindButton("vel_bind", MenuBinds::vel_bind, MenuBinds::vel_listening);
            ImGui::Spacing();
        }
    }

    {
        AutoRefill* ar = nullptr;
        for (auto* m : Modules::GetRegisteredModules())
            if ((ar = dynamic_cast<AutoRefill*>(m))) break;
        bool open = SnapCard("AutoRefill", "Refills potions or soup from your inventory.",
            ar ? &ar->enabled : nullptr, &MenuBinds::ar_bind, &MenuBinds::ar_listening);
        if (open && ar) {
            SectionHeader("Configuration");
            static const char* arModes[] = { "Blatant", "Legit", "Semi Blatant" };
            PhantomCombo("Mode", "##ar_mode", AutoRefillSettings::mode, arModes, 3);
            static const char* arItems[] = { "Potion", "Soup", "Both" };
            PhantomCombo("Item", "##ar_item", AutoRefillSettings::itemMode, arItems, 3);
            PhantomSliderInt("Speed", "##ar_speed", AutoRefillSettings::speed, 0, 10);
            PhantomToggleRow("##ar_rand", "Random slots", AutoRefillSettings::randomMode);
            if (AutoRefillSettings::mode == 1) {
                PhantomToggleRow("##ar_dyn", "Dynamic speed", AutoRefillSettings::dynamicSpeed);
                PhantomToggleRow("##ar_tr", "Transition", AutoRefillSettings::transition);
            }
            SectionHeader("Bind");
            DrawBindButton("ar_bind_exp", MenuBinds::ar_bind, MenuBinds::ar_listening);
            ImGui::Spacing();
        }
    }

    {
        AutoRod* rod = nullptr;
        for (auto* m : Modules::GetRegisteredModules())
            if ((rod = dynamic_cast<AutoRod*>(m))) break;
        bool open = SnapCard("AutoRod", "Automatically rods players out of melee range.",
            rod ? &rod->enabled : nullptr, &MenuBinds::rod_bind, &MenuBinds::rod_listening);
        if (open && rod) {
            SectionHeader("Range");
            PhantomSliderFloat("FOV", "##rod_fov", AutoRodSettings::fov, 20.f, 180.f, "%.0f");
            PhantomSliderFloat("Look FOV", "##rod_lfov", AutoRodSettings::maxLookFov, 0.f, 180.f, "%.0f");
            PhantomSliderFloat("Max range", "##rod_rng", AutoRodSettings::maxRange, 0.f, 20.f, "%.1f");
            PhantomSliderInt("Cooldown (ms)", "##rod_cd", AutoRodSettings::cooldownMs, 50, 2000);
            SectionHeader("Conditions");
            PhantomToggleRow("##rod_eat", "Ignore eating", AutoRodSettings::ignoreEating);
            PhantomToggleRow("##rod_mf", "Move fix", AutoRodSettings::moveFix);
            PhantomToggleRow("##rod_nr", "Only if not in reach", AutoRodSettings::onlyIfNotInReach);
            if (AutoRodSettings::onlyIfNotInReach)
                PhantomSliderFloat("Melee reach", "##rod_ml", AutoRodSettings::meleeReach, 2.f, 6.f, "%.1f");
            PhantomToggleRow("##rod_clk", "Click / swap", AutoRodSettings::click);
            PhantomToggleRow("##rod_rot", "Rotations", AutoRodSettings::rotations);
            PhantomToggleRow("##rod_lucky", "Lucky throw", AutoRodSettings::luckyThrow);
            SectionHeader("Targets");
            PhantomToggleRow("##rod_pl", "Players", AutoRodSettings::targetPlayers);
            PhantomToggleRow("##rod_mob", "Mobs", AutoRodSettings::targetMobs);
            PhantomToggleRow("##rod_en", "Enemies only", AutoRodSettings::targetEnemiesOnly);
            SectionHeader("ESP");
            PhantomToggleRow("##rod_esp", "ESP", AutoRodSettings::esp);
            if (AutoRodSettings::esp) {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
                ImGui::ColorEdit4("##rod_ec", AutoRodSettings::espColor,
                    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
                    ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
            }
            SectionHeader("Bind");
            DrawBindButton("rod_bind", MenuBinds::rod_bind, MenuBinds::rod_listening);
            ImGui::Spacing();
        }
    }

    {
        AntiBot* ab = nullptr;
        for (auto* m : Modules::GetRegisteredModules())
            if ((ab = dynamic_cast<AntiBot*>(m))) break;
        bool open = SnapCard("AntiBot", "Filters bots from targeting and visuals.",
            ab ? &ab->enabled : nullptr, &MenuBinds::ab_bind, &MenuBinds::ab_listening);
        if (open && ab) {
            SectionHeader("Checks");
            PhantomSliderInt("Min ticks", "##ab_tk", AntiBotSettings::minTicks, 0, 100);
            PhantomToggleRow("##ab_tab", "Check tab list", AntiBotSettings::checkTab);
            PhantomToggleRow("##ab_pkt", "Check movement", AntiBotSettings::checkPackets);
            if (AntiBotSettings::checkPackets)
                PhantomSliderInt("Packet grace", "##ab_pg", AntiBotSettings::packetGrace, 10, 120);
            SectionHeader("Bind");
            DrawBindButton("ab_bind", MenuBinds::ab_bind, MenuBinds::ab_listening);
            ImGui::Spacing();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ------------------------------------------------------------------
//  CLICKER TAB  (standalone — plus utilise directement mais conserve)
// ------------------------------------------------------------------
void ClientMenu::RenderClickerTab() {
    RenderCombatTab();
}

// ------------------------------------------------------------------
//  ARRAYLIST TAB
// ------------------------------------------------------------------
void ClientMenu::RenderArrayListTab() {
    ArrayList* al = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((al = dynamic_cast<ArrayList*>(m))) break;
    if (!al) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("ArrayList", al->enabled, MenuBinds::al_bind, MenuBinds::al_listening, W, "Lists enabled modules on your HUD.");

    if (!expanded) return;

    // ── Titre ─────────────────────────────────────────────────────────
    SectionHeader("Titre");

    {
        float rowW = ImGui::GetContentRegionAvail().x;

        ImGui::TextColored(TEXT_DIM, "Afficher le titre");
        ImGui::SameLine(rowW - 38.f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.f);
        PhantomToggle("##al_title_en", ArrayListSettings::showTitle);
        ImGui::Spacing();

        if (ArrayListSettings::showTitle) {
            ImGui::TextColored(TEXT_DIM, "Couleur du titre");
            ImGui::SetNextItemWidth(rowW - 2.f);
            ImGui::ColorEdit4("##al_titlecol", ArrayListSettings::titleColor,
                ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel |
                ImGuiColorEditFlags_NoAlpha);

            ImGui::Spacing();
            ImGui::TextColored(TEXT_DIM, "Apercu : ");
            ImGui::SameLine();
            ImGui::TextColored(
                ImVec4(ArrayListSettings::titleColor[0], ArrayListSettings::titleColor[1],
                    ArrayListSettings::titleColor[2], ArrayListSettings::titleColor[3]),
                ArrayListSettings::titleText);
            ImGui::Spacing();
        }
    }

    // ── Background ────────────────────────────────────────────────────
    SectionHeader("Background");

    {
        float rowW = ImGui::GetContentRegionAvail().x;

        // Toggle background on/off
        ImGui::TextColored(TEXT_DIM, "Activer le background");
        ImGui::SameLine(rowW - 38.f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.f);
        PhantomToggle("##al_bg_en", ArrayListSettings::bgEnabled);
        ImGui::Spacing();

        // Color picker background (couleur + alpha)
        if (ArrayListSettings::bgEnabled) {
            ImGui::TextColored(TEXT_DIM, "Couleur du background");
            ImGui::SetNextItemWidth(rowW - 2.f);
            ImGui::ColorEdit4("##al_bgcol", ArrayListSettings::bgColor,
                ImGuiColorEditFlags_AlphaBar |
                ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel |
                ImGuiColorEditFlags_NoAlpha |
                ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::Spacing();
        }
    }

    // ── Couleur du nom ────────────────────────────────────────────────
    SectionHeader("Nom du module");
    {
        float rowW = ImGui::GetContentRegionAvail().x;
        ImGui::TextColored(TEXT_DIM, "Couleur du nom  (ex: \"Clicker\")");
        ImGui::SetNextItemWidth(rowW - 2.f);
        ImGui::ColorEdit4("##al_namecol", ArrayListSettings::nameColor,
            ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_NoLabel |
            ImGuiColorEditFlags_NoAlpha);

        // Apercu
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "Apercu : ");
        ImGui::SameLine();
        ImGui::TextColored(
            ImVec4(ArrayListSettings::nameColor[0], ArrayListSettings::nameColor[1],
                ArrayListSettings::nameColor[2], ArrayListSettings::nameColor[3]),
            "Clicker");
        ImGui::SameLine(0.f, 4.f);
        ImGui::TextColored(
            ImVec4(ArrayListSettings::suffixColor[0], ArrayListSettings::suffixColor[1],
                ArrayListSettings::suffixColor[2], ArrayListSettings::suffixColor[3]),
            "12 CPS");
        ImGui::Spacing();
    }

    // ── Couleur du suffix ─────────────────────────────────────────────
    SectionHeader("Settings du module");
    {
        float rowW = ImGui::GetContentRegionAvail().x;
        ImGui::TextColored(TEXT_DIM, "Couleur des settings  (ex: \"12 CPS\")");
        ImGui::SetNextItemWidth(rowW - 2.f);
        ImGui::ColorEdit4("##al_suffcol", ArrayListSettings::suffixColor,
            ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_NoLabel |
            ImGuiColorEditFlags_NoAlpha);
        ImGui::Spacing();
    }

    // ── Barre latérale droite ─────────────────────────────────────────
    SectionHeader("Barre decorative");
    {
        float rowW = ImGui::GetContentRegionAvail().x;

        ImGui::TextColored(TEXT_DIM, "Activer la barre");
        ImGui::SameLine(rowW - 38.f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.f);
        PhantomToggle("##al_bar_en", ArrayListSettings::barEnabled);
        ImGui::Spacing();

        if (ArrayListSettings::barEnabled) {
            ImGui::TextColored(TEXT_DIM, "Couleur de la barre");
            ImGui::SetNextItemWidth(rowW - 2.f);
            ImGui::ColorEdit4("##al_barcol", ArrayListSettings::barColor,
                ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel |
                ImGuiColorEditFlags_NoAlpha);
            ImGui::Spacing();
        }
    }

    // ── Taille de police ──────────────────────────────────────────────
    SectionHeader("Police");
    {
        float rowW = ImGui::GetContentRegionAvail().x;
        ImGui::TextColored(TEXT_DIM, "Taille");
        ImGui::SetNextItemWidth(rowW - 2.f);
        ImGui::SliderFloat("##al_fontsize", &ArrayListSettings::fontSize, 0.8f, 2.5f, "x%.2f");
        ImGui::Spacing();
    }

    // ── Preview modules actifs ────────────────────────────────────────
    SectionHeader("Modules actifs");
    {
        bool any = false;
        for (auto* m : Modules::GetRegisteredModules()) {
            if (!m->IsEnabled() || strlen(m->GetName()) == 0) continue;
            const char* suffix = m->GetSuffix();

            ImGui::TextColored(AC, "•");
            ImGui::SameLine(0.f, 6.f);
            ImGui::TextColored(
                ImVec4(ArrayListSettings::nameColor[0], ArrayListSettings::nameColor[1],
                    ArrayListSettings::nameColor[2], ArrayListSettings::nameColor[3]),
                "%s", m->GetName());

            if (suffix && suffix[0] != '\0') {
                ImGui::SameLine(0.f, 6.f);
                ImGui::TextColored(
                    ImVec4(ArrayListSettings::suffixColor[0], ArrayListSettings::suffixColor[1],
                        ArrayListSettings::suffixColor[2], ArrayListSettings::suffixColor[3]),
                    "%s", suffix);
            }
            any = true;
        }
        if (!any) ImGui::TextColored(TEXT_DIM, "Aucun module actif.");
    }
}

void ClientMenu::RenderChamsTab() {
    Chams* ch = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((ch = dynamic_cast<Chams*>(m))) break;
    if (!ch) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("Chams", ch->enabled, MenuBinds::ch_bind, MenuBinds::ch_listening, W, "Renders players through walls.");
    if (!expanded) return;

    SectionHeader("Entities");
    PhantomToggleRow("##ch_pl", "Players", ChamsSettings::players);
    PhantomToggleRow("##ch_mob", "Mobs", ChamsSettings::mobs);
    PhantomToggleRow("##ch_an", "Animals", ChamsSettings::animals);
    PhantomToggleRow("##ch_vil", "Villager", ChamsSettings::villagers);
    PhantomToggleRow("##ch_as", "Armor Stands", ChamsSettings::armorStands);
    PhantomToggleRow("##ch_inv", "Invisible", ChamsSettings::invisibles);

    SectionHeader("Mode");
    PhantomToggleRow("##ch_tex", "Render texture", ChamsSettings::renderTexture);
    PhantomToggleRow("##ch_glow", "Glow", ChamsSettings::glowMode);
    PhantomToggleRow("##ch_hf", "Hide friends", ChamsSettings::hideFriends);
    PhantomToggleRow("##ch_eo", "Enemies only", ChamsSettings::enemiesOnly);

    SectionHeader("Colors");
    {
        float rowW = ImGui::GetContentRegionAvail().x;
        ImGui::TextColored(TEXT_DIM, "Neutral");
        ImGui::SetNextItemWidth(rowW - 2.f);
        ImGui::ColorEdit4("##ch_ncol", ChamsSettings::colorNeutral,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "Friends");
        ImGui::SetNextItemWidth(rowW - 2.f);
        ImGui::ColorEdit4("##ch_fcol", ChamsSettings::colorFriend,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "Enemies");
        ImGui::SetNextItemWidth(rowW - 2.f);
        ImGui::ColorEdit4("##ch_ecol", ChamsSettings::colorEnemy,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::Spacing();
    }

    SectionHeader("Bind");
    DrawBindButton("ch_bind", MenuBinds::ch_bind, MenuBinds::ch_listening);
    ImGui::Spacing();
}

void ClientMenu::RenderEspTab() {
    Esp* esp = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((esp = dynamic_cast<Esp*>(m))) break;
    if (!esp) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("ESP", esp->enabled, MenuBinds::esp_bind, MenuBinds::esp_listening, W, "Draws boxes around other players.");
    if (!expanded) return;

    static const char* renderModes[] = { "2D", "3D", "Both" };
    static const char* boxModes[] = { "Outline", "Fill", "Both" };
    PhantomCombo("Render", "##esp_rm", EspSettings::renderMode, renderModes, 3);

    if (EspSettings::renderMode == 1 || EspSettings::renderMode == 2) {
        SectionHeader("3D");
        PhantomCombo("Mode 3D", "##esp_m3d", EspSettings::mode3d, boxModes, 3);
        if (EspSettings::mode3d == 0 || EspSettings::mode3d == 2) {
            ImGui::TextColored(TEXT_DIM, "Outline 3D");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
            ImGui::ColorEdit4("##esp_o3d", EspSettings::outline3dColor,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
            PhantomSliderFloat("Outline width", "##esp_o3dw", EspSettings::outline3dWidth, 0.5f, 5.f, "%.1f");
        }
        if (EspSettings::mode3d == 1 || EspSettings::mode3d == 2) {
            ImGui::TextColored(TEXT_DIM, "Fill 3D");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
            ImGui::ColorEdit4("##esp_f3d", EspSettings::fill3dColor,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
            PhantomSliderFloat("Fill opacity", "##esp_f3do", EspSettings::fill3dOpacity, 0.f, 1.f, "%.2f");
        }
    }

    if (EspSettings::renderMode == 0 || EspSettings::renderMode == 2) {
        SectionHeader("2D");
        PhantomCombo("Mode 2D", "##esp_m2d", EspSettings::mode2d, boxModes, 3);
        if (EspSettings::mode2d == 0 || EspSettings::mode2d == 2) {
            ImGui::TextColored(TEXT_DIM, "Outline 2D");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
            ImGui::ColorEdit4("##esp_o2d", EspSettings::outline2dColor,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
            PhantomSliderFloat("Outline width", "##esp_o2dw", EspSettings::outline2dWidth, 0.5f, 3.f, "%.1f");
        }
        if (EspSettings::mode2d == 1 || EspSettings::mode2d == 2) {
            ImGui::TextColored(TEXT_DIM, "Fill 2D");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
            ImGui::ColorEdit4("##esp_f2d", EspSettings::fill2dColor,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
        }

        PhantomToggleRow("##esp_hp", "Health bar", EspSettings::showHealthBar);
        if (EspSettings::showHealthBar) {
            ImGui::TextColored(TEXT_DIM, "Health bg");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
            ImGui::ColorEdit4("##esp_hbg", EspSettings::healthBarBg,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::TextColored(TEXT_DIM, "Health full");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
            ImGui::ColorEdit4("##esp_hfull", EspSettings::healthBarFull,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::TextColored(TEXT_DIM, "Health low");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
            ImGui::ColorEdit4("##esp_hlow", EspSettings::healthBarLow,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
            PhantomSliderFloat("Bar width", "##esp_hbw", EspSettings::healthBarWidth, 1.f, 10.f, "%.1f");
            PhantomSliderFloat("Bar offset", "##esp_hbo", EspSettings::healthBarOffset, 0.f, 20.f, "%.1f");
        }
    }

    SectionHeader("Filters");
    PhantomToggleRow("##esp_hf", "Hide friends", EspSettings::hideFriends);
    PhantomToggleRow("##esp_eo", "Enemies only", EspSettings::enemiesOnly);
    PhantomSliderFloat("Max distance", "##esp_dist", EspSettings::maxRenderDistance, 16.f, 128.f, "%.0f");

    SectionHeader("Colors");
    ImGui::TextColored(TEXT_DIM, "Neutral");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
    ImGui::ColorEdit4("##esp_neu", EspSettings::neutralColor,
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
    ImGui::TextColored(TEXT_DIM, "Friends");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
    ImGui::ColorEdit4("##esp_fr", EspSettings::friendColor,
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
    ImGui::TextColored(TEXT_DIM, "Enemies");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
    ImGui::ColorEdit4("##esp_en", EspSettings::enemyColor,
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);

    SectionHeader("Bind");
    DrawBindButton("esp_bind", MenuBinds::esp_bind, MenuBinds::esp_listening);
    ImGui::Spacing();
}

void ClientMenu::RenderItemEspTab() {
    ItemEsp* ie = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((ie = dynamic_cast<ItemEsp*>(m))) break;
    if (!ie) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("Item ESP", ie->enabled, MenuBinds::itemesp_bind, MenuBinds::itemesp_listening, W, "Highlights dropped items in the world.");
    if (!expanded) return;

    PhantomSliderFloat("Max distance", "##iesp_dist", ItemEspSettings::maxDistance, 8.f, 64.f, "%.0f");
    ImGui::TextColored(TEXT_DIM, "Color");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
    ImGui::ColorEdit4("##iesp_col", ItemEspSettings::color,
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);

    SectionHeader("Bind");
    DrawBindButton("itemesp_bind", MenuBinds::itemesp_bind, MenuBinds::itemesp_listening);
    ImGui::Spacing();
}

void ClientMenu::RenderPlayerEspTab() {
    PlayerEsp* pe = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((pe = dynamic_cast<PlayerEsp*>(m))) break;
    if (!pe) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("Player ESP", pe->enabled, MenuBinds::pesp_bind, MenuBinds::pesp_listening, W, "Shows armor, potions and held items.");
    if (!expanded) return;

    SectionHeader("Features");
    PhantomToggleRow("##pe_ar", "Armor", PlayerEspSettings::armor);
    PhantomToggleRow("##pe_pot", "Potions", PlayerEspSettings::potions);
    PhantomToggleRow("##pe_hi", "Held item", PlayerEspSettings::heldItem);
    PhantomToggleRow("##pe_sk", "Skeleton", PlayerEspSettings::skeleton);
    PhantomToggleRow("##pe_ol", "Outline", PlayerEspSettings::outline);
    PhantomToggleRow("##pe_gp", "Gapple", PlayerEspSettings::gapple);
    PhantomToggleRow("##pe_hf", "Hide friends", PlayerEspSettings::hideFriends);
    PhantomToggleRow("##pe_eo", "Enemies only", PlayerEspSettings::enemiesOnly);

    if (PlayerEspSettings::outline) {
        static const char* olModes[] = { "3D", "2D" };
        PhantomCombo("Outline mode", "##pe_olm", PlayerEspSettings::outlineMode, olModes, 2);
        PhantomToggleRow("##pe_og", "Outline glow", PlayerEspSettings::outlineGlow);
        PhantomSliderFloat("Outline thickness", "##pe_olt", PlayerEspSettings::outlineThickness, 0.5f, 5.f, "%.1f");
        if (PlayerEspSettings::outlineGlow)
            PhantomSliderFloat("Glow radius", "##pe_ogr", PlayerEspSettings::outlineGlowRadius, 1.f, 20.f, "%.1f");
        ImGui::TextColored(TEXT_DIM, "Outline color");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
        ImGui::ColorEdit4("##pe_olc", PlayerEspSettings::outlineColor,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
    }

    if (PlayerEspSettings::skeleton) {
        PhantomSliderFloat("Skeleton thickness", "##pe_skt", PlayerEspSettings::skeletonThickness, 0.5f, 5.f, "%.1f");
        ImGui::TextColored(TEXT_DIM, "Skeleton color");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
        ImGui::ColorEdit4("##pe_skc", PlayerEspSettings::skeletonColor,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
    }

    PhantomSliderFloat("Display scale", "##pe_sc", PlayerEspSettings::displayScale, 0.5f, 3.f, "%.2f");
    PhantomSliderFloat("Max distance", "##pe_dist", PlayerEspSettings::maxRenderDistance, 16.f, 128.f, "%.0f");

    SectionHeader("Bind");
    DrawBindButton("pesp_bind", MenuBinds::pesp_bind, MenuBinds::pesp_listening);
    ImGui::Spacing();
}

void ClientMenu::RenderStorageEspTab() {
    StorageEsp* se = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((se = dynamic_cast<StorageEsp*>(m))) break;
    if (!se) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("Storage ESP", se->enabled, MenuBinds::sesp_bind, MenuBinds::sesp_listening, W, "Highlights chests and other storage.");
    if (!expanded) return;

    static const char* renderModes[] = { "2D", "3D", "Both" };
    static const char* boxModes[] = { "Outline", "Fill", "Both" };
    PhantomCombo("Render", "##sesp_rm", StorageEspSettings::renderMode, renderModes, 3);

    if (StorageEspSettings::renderMode == 1 || StorageEspSettings::renderMode == 2) {
        SectionHeader("3D");
        PhantomCombo("Mode 3D", "##sesp_m3d", StorageEspSettings::mode3d, boxModes, 3);
        if (StorageEspSettings::mode3d == 0 || StorageEspSettings::mode3d == 2)
            PhantomSliderFloat("Outline width", "##sesp_o3dw", StorageEspSettings::outline3dWidth, 0.5f, 5.f, "%.1f");
        if (StorageEspSettings::mode3d == 1 || StorageEspSettings::mode3d == 2)
            PhantomSliderFloat("Fill opacity", "##sesp_f3d", StorageEspSettings::fillAlpha3d, 0.f, 1.f, "%.2f");
    }
    if (StorageEspSettings::renderMode == 0 || StorageEspSettings::renderMode == 2) {
        SectionHeader("2D");
        PhantomCombo("Mode 2D", "##sesp_m2d", StorageEspSettings::mode2d, boxModes, 3);
        if (StorageEspSettings::mode2d == 0 || StorageEspSettings::mode2d == 2)
            PhantomSliderFloat("Outline width", "##sesp_o2dw", StorageEspSettings::outline2dWidth, 0.5f, 3.f, "%.1f");
        if (StorageEspSettings::mode2d == 1 || StorageEspSettings::mode2d == 2)
            PhantomSliderFloat("Fill opacity", "##sesp_f2d", StorageEspSettings::fillAlpha2d, 0.f, 1.f, "%.2f");
    }

    PhantomSliderFloat("Max distance", "##sesp_dist", StorageEspSettings::maxDistance, 16.f, 256.f, "%.0f");
    PhantomToggleRow("##sesp_lb", "Labels", StorageEspSettings::showLabels);
    if (StorageEspSettings::showLabels) {
        PhantomSliderFloat("Label scale", "##sesp_ls", StorageEspSettings::labelScale, 0.5f, 2.f, "%.2f");
        ImGui::TextColored(TEXT_DIM, "Label color");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
        ImGui::ColorEdit4("##sesp_lc", StorageEspSettings::labelColor,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
    }

    SectionHeader("Types");
    auto typeRow = [](const char* id, const char* name, bool& on, float* col) {
        PhantomToggleRow(id, name, on);
        if (on) {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
            ImGui::ColorEdit4((std::string("##c") + id).c_str(), col,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
        }
    };
    typeRow("##sesp_ch", "Chest", StorageEspSettings::chest, StorageEspSettings::chestColor);
    typeRow("##sesp_ec", "Ender chest", StorageEspSettings::enderChest, StorageEspSettings::enderChestColor);
    typeRow("##sesp_fu", "Furnace", StorageEspSettings::furnace, StorageEspSettings::furnaceColor);
    typeRow("##sesp_di", "Dispenser", StorageEspSettings::dispenser, StorageEspSettings::dispenserColor);
    typeRow("##sesp_dr", "Dropper", StorageEspSettings::dropper, StorageEspSettings::dropperColor);
    typeRow("##sesp_ho", "Hopper", StorageEspSettings::hopper, StorageEspSettings::hopperColor);

    SectionHeader("Bind");
    DrawBindButton("sesp_bind", MenuBinds::sesp_bind, MenuBinds::sesp_listening);
    ImGui::Spacing();
}

void ClientMenu::RenderNametagTab() {
    Nametag* nt = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((nt = dynamic_cast<Nametag*>(m))) break;
    if (!nt) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("Nametags", nt->enabled, MenuBinds::ntag_bind, MenuBinds::ntag_listening, W, "Custom nametags with health and distance.");
    if (!expanded) return;

    PhantomToggleRow("##nt_nm", "Names", NametagSettings::showNames);
    PhantomToggleRow("##nt_hp", "Health", NametagSettings::showHealth);
    PhantomToggleRow("##nt_ds", "Distance", NametagSettings::showDistance);
    PhantomToggleRow("##nt_bg", "Background", NametagSettings::showBackground);
    PhantomToggleRow("##nt_hb", "Health bar", NametagSettings::showHealthBar);
    PhantomToggleRow("##nt_ol", "Outline", NametagSettings::showOutline);
    PhantomToggleRow("##nt_hf", "Hide friends", NametagSettings::hideFriends);
    PhantomToggleRow("##nt_eo", "Enemies only", NametagSettings::enemiesOnly);

    PhantomSliderFloat("Scale", "##nt_sc", NametagSettings::nametagScale, 0.1f, 3.f, "%.2f");
    PhantomSliderFloat("Text size", "##nt_ts", NametagSettings::textSize, 8.f, 40.f, "%.0f");
    if (NametagSettings::showOutline)
        PhantomSliderFloat("Outline thickness", "##nt_ot", NametagSettings::outlineThickness, 0.5f, 3.f, "%.1f");
    PhantomSliderFloat("Max distance", "##nt_dist", NametagSettings::maxRenderDistance, 16.f, 128.f, "%.0f");
    if (NametagSettings::showHealthBar) {
        PhantomSliderFloat("Bar width", "##nt_bw", NametagSettings::healthBarWidth, 20.f, 120.f, "%.0f");
        PhantomSliderFloat("Bar height", "##nt_bh", NametagSettings::healthBarHeight, 1.f, 12.f, "%.0f");
    }

    auto colorRow = [](const char* label, const char* id, float* col) {
        ImGui::TextColored(TEXT_DIM, label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
        ImGui::ColorEdit4(id, col,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
    };
    if (NametagSettings::showNames)
        colorRow("Name", "##nt_nc", NametagSettings::nameColor);
    colorRow("Friend", "##nt_fc", NametagSettings::friendColor);
    colorRow("Enemy", "##nt_ec", NametagSettings::enemyColor);
    colorRow("Neutral", "##nt_neu", NametagSettings::neutralColor);
    if (NametagSettings::showDistance)
        colorRow("Distance", "##nt_dc", NametagSettings::distanceColor);
    if (NametagSettings::showBackground)
        colorRow("Background", "##nt_bgc", NametagSettings::backgroundColor);
    if (NametagSettings::showHealth || NametagSettings::showHealthBar) {
        colorRow("Health full", "##nt_hpf", NametagSettings::healthBarFull);
        colorRow("Health low", "##nt_hpl", NametagSettings::healthBarLow);
    }
    if (NametagSettings::showHealthBar)
        colorRow("Bar background", "##nt_hbb", NametagSettings::healthBarBg);
    if (NametagSettings::showOutline)
        colorRow("Outline", "##nt_olc", NametagSettings::outlineColor);

    SectionHeader("Bind");
    DrawBindButton("ntag_bind", MenuBinds::ntag_bind, MenuBinds::ntag_listening);
    ImGui::Spacing();
}

void ClientMenu::RenderTracerTab() {
    Tracer* tr = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((tr = dynamic_cast<Tracer*>(m))) break;
    if (!tr) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("Tracer", tr->enabled, MenuBinds::tr_bind, MenuBinds::tr_listening, W, "Draws lines from you to other players.");
    if (!expanded) return;

    PhantomToggleRow("##tr_hf", "Hide friends", TracerSettings::hideFriends);
    PhantomToggleRow("##tr_eo", "Enemies only", TracerSettings::enemiesOnly);
    PhantomSliderFloat("Width", "##tr_w", TracerSettings::tracerWidth, 0.5f, 8.f, "%.1f");
    PhantomSliderFloat("Max distance", "##tr_dist", TracerSettings::maxRenderDistance, 16.f, 128.f, "%.0f");

    ImGui::TextColored(TEXT_DIM, "Tracer");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
    ImGui::ColorEdit4("##tr_col", TracerSettings::tracerColor,
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
    ImGui::TextColored(TEXT_DIM, "Friend");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
    ImGui::ColorEdit4("##tr_fcol", TracerSettings::friendColor,
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
    ImGui::TextColored(TEXT_DIM, "Enemy");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
    ImGui::ColorEdit4("##tr_ecol", TracerSettings::enemyColor,
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);

    SectionHeader("Bind");
    DrawBindButton("tr_bind", MenuBinds::tr_bind, MenuBinds::tr_listening);
    ImGui::Spacing();
}

void ClientMenu::RenderTrajectoriesTab() {
    Trajectories* tj = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((tj = dynamic_cast<Trajectories*>(m))) break;
    if (!tj) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("Trajectories", tj->enabled, MenuBinds::tj_bind, MenuBinds::tj_listening, W, "Predicts projectile paths.");
    if (!expanded) return;

    PhantomToggleRow("##tj_bow", "Bow", TrajectoriesSettings::bow);
    PhantomToggleRow("##tj_pot", "Potion", TrajectoriesSettings::potion);
    PhantomToggleRow("##tj_prl", "Pearl", TrajectoriesSettings::pearl);
    PhantomToggleRow("##tj_sn", "Snowball", TrajectoriesSettings::snowball);
    PhantomToggleRow("##tj_egg", "Egg", TrajectoriesSettings::egg);
    PhantomToggleRow("##tj_rod", "Rod", TrajectoriesSettings::rod);
    PhantomSliderFloat("Width", "##tj_w", TrajectoriesSettings::lineWidth, 1.f, 8.f, "%.1f");
    ImGui::TextColored(TEXT_DIM, "Arc");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.f);
    ImGui::ColorEdit4("##tj_col", TrajectoriesSettings::arcColor,
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);

    SectionHeader("Bind");
    DrawBindButton("tj_bind", MenuBinds::tj_bind, MenuBinds::tj_listening);
    ImGui::Spacing();
}

// ------------------------------------------------------------------
//  SCROLL TAB  (desormais dans Utility)
// ------------------------------------------------------------------
void ClientMenu::RenderScrollTab() {
    bool scrollExpanded = SnapCard("Scroll", "Hotkey-scrolls to whitelisted items.",
        &Scroll::enabled, &Scroll::scroll_bind, &Scroll::scroll_listen);

    if (scrollExpanded) {
        SectionHeader("Whitelist items");
        for (int i = 0; i < Scroll::ITEM_COUNT; i++) {
            std::string tid = "##si_" + std::to_string(i);
            PhantomToggleRow(tid.c_str(), Scroll::g_itemNames[i], Scroll::g_whitelistEnabled[i]);
        }
        SectionHeader("Configuration");
        PhantomSliderInt("Delai entre slots (ms)", "##scroll_delay", Scroll::scrollDelay, 10, 300);
        ImGui::Spacing();
        if (ImGui::SmallButton("Reload Keybinds##scroll")) Scroll::ReloadKeybinds();
    }
}

// ------------------------------------------------------------------
//  FASTPLACE TAB  (Utility)
// ------------------------------------------------------------------
void ClientMenu::RenderFastPlaceTab() {
    FastPlace* fp = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((fp = dynamic_cast<FastPlace*>(m))) break;
    if (!fp) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("FastPlace", fp->enabled, MenuBinds::fp_bind, MenuBinds::fp_listening, W, "Places blocks faster than vanilla.");
    if (!expanded) return;

    static const char* modes[] = { "Delay", "Click" };
    PhantomCombo("Mode", "##fp_mode", FastPlaceSettings::mode, modes, 2);
    PhantomToggleRow("##fp_blk", "Only blocks", FastPlaceSettings::onlyBlock);

    if (FastPlaceSettings::mode == 0) {
        PhantomSliderInt("Tick delay", "##fp_td", FastPlaceSettings::tickDelay, 0, 3);
        ImGui::TextColored(TEXT_DIM, "0 = place chaque tick (vanilla = 4).");
    } else {
        PhantomSliderFloat("Average CPS", "##fp_avg", FastPlaceSettings::average, 1.f, 25.f, "%.1f");
        PhantomToggleRow("##fp_hold", "Hold to click", FastPlaceSettings::holdToClick);
        PhantomToggleRow("##fp_ex", "Exhaust", FastPlaceSettings::exhaust);
    }

    SectionHeader("Bind");
    DrawBindButton("fp_bind", MenuBinds::fp_bind, MenuBinds::fp_listening);
    ImGui::Spacing();
}

// ------------------------------------------------------------------
//  FASTBREAK TAB  (Utility)
// ------------------------------------------------------------------
void ClientMenu::RenderFastBreakTab() {
    FastBreak* fb = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((fb = dynamic_cast<FastBreak*>(m))) break;
    if (!fb) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("FastBreak", fb->enabled, MenuBinds::fb_bind, MenuBinds::fb_listening, W, "Breaks blocks faster than vanilla.");
    if (!expanded) return;

    static const char* modes[] = { "Normal", "Timer" };
    PhantomCombo("Mode", "##fb_mode", FastBreakSettings::mode, modes, 2);
    if (FastBreakSettings::mode == 0)
        PhantomSliderFloat("Power", "##fb_pow", FastBreakSettings::power, 0.f, 100.f, "%.0f%%");
    else
        PhantomSliderFloat("Multiplier", "##fb_mul", FastBreakSettings::multiplier, 0.f, 100.f, "%.1fx");

    SectionHeader("Bind");
    DrawBindButton("fb_bind", MenuBinds::fb_bind, MenuBinds::fb_listening);
    ImGui::Spacing();
}

void ClientMenu::RenderTickLockerTab() {
    if (g_GameVersion != LUNAR_1_8_9) return;
    TickLocker* tl = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((tl = dynamic_cast<TickLocker*>(m))) break;
    if (!tl) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("TickLocker", tl->enabled, MenuBinds::tl_bind, MenuBinds::tl_listening, W,
        "Locks mining to a selected block. 1.8 only.");
    if (!expanded) return;

    ImGui::TextColored(TEXT_DIM, "Look at a block and press the target key.");
    ImGui::Spacing();
    DrawBindButton("tl_target", TickLockerSettings::targetKey, MenuBinds::tl_target_listening);
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "Target block");
    ImGui::Spacing();
    if (ImGui::Button("Clear target##tl_clear", { ImGui::GetContentRegionAvail().x, 26.f }))
        TickLocker_ClearTarget();
    ImGui::Spacing();

    PhantomToggleRow("##tl_vis", "Render selected block", TickLockerSettings::renderSelectedBlock);
    if (TickLockerSettings::renderSelectedBlock) {
        PhantomToggleRow("##tl_ol", "Outline", TickLockerSettings::showOutline);
        if (TickLockerSettings::showOutline) {
            PhantomSliderFloat("Outline width", "##tl_olw", TickLockerSettings::outlineWidth, 0.5f, 5.f, "%.1f");
            ImGui::ColorEdit4("##tl_olc", TickLockerSettings::outlineColor,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        }
        PhantomToggleRow("##tl_fill", "Fill", TickLockerSettings::showFill);
        if (TickLockerSettings::showFill) {
            ImGui::ColorEdit4("##tl_fc", TickLockerSettings::fillColor,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        }
    }

    SectionHeader("Bind");
    DrawBindButton("tl_bind", MenuBinds::tl_bind, MenuBinds::tl_listening);
    ImGui::Spacing();
}

// ------------------------------------------------------------------
//  INVWALK TAB  (Utility)
// ------------------------------------------------------------------
void ClientMenu::RenderInvWalkTab() {
    InvWalk* iw = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((iw = dynamic_cast<InvWalk*>(m))) break;
    if (!iw) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("InvWalk", iw->enabled, MenuBinds::iw_bind, MenuBinds::iw_listening, W, "Walk while a GUI is open.");
    if (!expanded) return;

    static const char* modes[] = { "Legit", "Blatant" };
    PhantomCombo("Mode", "##iw_mode", InvWalkSettings::mode, modes, 2);
    ImGui::TextColored(TEXT_DIM, "Marche dans les GUIs (pas le chat). Blatant force le sprint.");

    SectionHeader("Bind");
    DrawBindButton("iw_bind", MenuBinds::iw_bind, MenuBinds::iw_listening);
    ImGui::Spacing();
}

// ------------------------------------------------------------------
//  FASTSTOP TAB  (Utility)
// ------------------------------------------------------------------
void ClientMenu::RenderFastStopTab() {
    FastStop* fs = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((fs = dynamic_cast<FastStop*>(m))) break;
    if (!fs) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("FastStop", fs->enabled, MenuBinds::fs_bind, MenuBinds::fs_listening, W, "Counter-strafes on key release.");
    if (!expanded) return;

    static const char* axes[] = { "Both", "Strafe", "Fwd/Back" };
    PhantomCombo("Axis", "##fs_axis", FastStopSettings::axis, axes, 3);
    PhantomToggleRow("##fs_sneak", "Disable on sneak", FastStopSettings::disableOnSneak);
    ImGui::TextColored(TEXT_DIM, "Contre-strafe 1 tick a la relache (Whip).");

    SectionHeader("Bind");
    DrawBindButton("fs_bind", MenuBinds::fs_bind, MenuBinds::fs_listening);
    ImGui::Spacing();
}

// ------------------------------------------------------------------
//  NOJUMPDELAY TAB  (Utility)
// ------------------------------------------------------------------
void ClientMenu::RenderNoJumpDelayTab() {
    NoJumpDelay* njd = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((njd = dynamic_cast<NoJumpDelay*>(m))) break;
    if (!njd) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("NoJumpDelay", njd->enabled, MenuBinds::njd_bind, MenuBinds::njd_listening, W, "Removes vanilla jump cooldown.");
    if (!expanded) return;

    ImGui::TextColored(TEXT_DIM, "Reset jumpTicks a 0 (sauts enchaines).");

    SectionHeader("Bind");
    DrawBindButton("njd_bind", MenuBinds::njd_bind, MenuBinds::njd_listening);
    ImGui::Spacing();
}

// ------------------------------------------------------------------
//  QUICKACCEL TAB  (Utility)
// ------------------------------------------------------------------
void ClientMenu::RenderQuickAccelTab() {
    QuickAccel* qa = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((qa = dynamic_cast<QuickAccel*>(m))) break;
    if (!qa) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("QuickAccel", qa->enabled, MenuBinds::qa_bind, MenuBinds::qa_listening, W, "Accelerates from standstill faster.");
    if (!expanded) return;

    PhantomToggleRow("##qa_sneak", "Disable on sneak", QuickAccelSettings::disableOnSneak);
    ImGui::TextColored(TEXT_DIM, "Boost input x5 (Whip onLivingUpdate).");

    SectionHeader("Bind");
    DrawBindButton("qa_bind", MenuBinds::qa_bind, MenuBinds::qa_listening);
    ImGui::Spacing();
}

// ------------------------------------------------------------------
//  SNAPTAP TAB  (Utility)
// ------------------------------------------------------------------
void ClientMenu::RenderSnapTapTab() {
    SnapTap* st = nullptr;
    for (auto* m : Modules::GetRegisteredModules())
        if ((st = dynamic_cast<SnapTap*>(m))) break;
    if (!st) { ImGui::TextDisabled("Module introuvable."); return; }

    float W = ImGui::GetContentRegionAvail().x;
    bool expanded = ModuleHeader("SnapTap", st->enabled, MenuBinds::st_bind, MenuBinds::st_listening, W, "Last opposite key wins while both are held.");
    if (!expanded) return;

    static const char* axes[] = { "Both", "Strafe", "Fwd/Back" };
    PhantomCombo("Axis", "##st_axis", SnapTapSettings::axis, axes, 3);
    PhantomToggleRow("##st_og", "Only on ground", SnapTapSettings::onlyOnGround);
    PhantomToggleRow("##st_sneak", "Disable on sneak", SnapTapSettings::disableOnSneak);
    ImGui::TextColored(TEXT_DIM, "La derniere touche opposee gagne (A/D, Z/S).");

    SectionHeader("Bind");
    DrawBindButton("st_bind", MenuBinds::st_bind, MenuBinds::st_listening);
    ImGui::Spacing();
}

// ------------------------------------------------------------------
//  ARMOR TAB  (Utility)
// ------------------------------------------------------------------
void ClientMenu::RenderArmorTab() {
    float W = ImGui::GetContentRegionAvail().x;
    float sw = W - 2.f;

    bool armorExpanded = SnapCard("Armor Switcher", "Equips the selected armor set.",
        &Armor::enabled, &Armor::bind, &Armor::listening);

    if (armorExpanded) {
        SectionHeader("Configuration");
        PhantomSliderInt("Vitesse", "##armor_spd", Armor::speed, 1, 20);
        SectionHeader("Armure a equiper");
        ImGui::TextColored(TEXT_DIM, "ID Minecraft (ex: 310=Diamond Helm)");
        ImGui::Spacing();

        static const char* matNames[] = { "Leather","Gold","Chain","Iron","Diamond" };
        static const int   matIds[5][4] = {
            {298,299,300,301},{314,315,316,317},
            {302,303,304,305},{306,307,308,309},{310,311,312,313}
        };
        for (int i = 0; i < 4; i++) {
            ImGui::TextColored(AC, "%s", Armor::pieceNames[i]);
            for (int m2 = 0; m2 < 5; m2++) {
                if (m2 > 0) ImGui::SameLine(0.f, 3.f);
                bool active = (Armor::armorIds[i][0] == matIds[m2][i]);
                ImGui::PushStyleColor(ImGuiCol_Button, active ? AC_DIM : BG2);
                ImGui::PushStyleColor(ImGuiCol_Text, active ? TEXT : TEXT_DIM);
                std::string lbl = std::string(matNames[m2]) + "##m" + std::to_string(i) + "_" + std::to_string(m2);
                if (ImGui::SmallButton(lbl.c_str())) {
                    Armor::armorIds[i][0] = matIds[m2][i];
                    for (int k = 1; k < 8; k++) Armor::armorIds[i][k] = 0;
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::SetNextItemWidth(sw * 0.45f);
            ImGui::InputInt((std::string("##armor_id_") + std::to_string(i)).c_str(), &Armor::armorIds[i][0], 1, 10);
            ImGui::SameLine();
            ImGui::TextColored(TEXT_DIM, Armor::armorIds[i][0] > 0 ? "ID: %d" : "OFF", Armor::armorIds[i][0]);
            ImGui::Spacing();
        }
    }
}

// ------------------------------------------------------------------
//  FRIENDS TAB  (Utility)
// ------------------------------------------------------------------
void ClientMenu::RenderFriendsTab() {
    bool frExpanded = SnapCard("Friends", "Skip friends in combat and visuals.",
        &FriendsSettings::enabled, &MenuBinds::fr_bind, &MenuBinds::fr_listening);

    if (!frExpanded) return;

    SectionHeader("Settings");
    {
        float rowW = ImGui::GetContentRegionAvail().x;
        ImGui::TextColored(TEXT_DIM, "Add Friends Key");
        ImGui::SameLine(rowW - 110.f);
        DrawBindButton("fr_add_key", FriendsSettings::addFriendKey, FriendsSettings::addFriendListening);
        ImGui::Spacing();

        ImGui::TextColored(TEXT_DIM, "Add Nearby Friends Key");
        ImGui::SameLine(rowW - 110.f);
        DrawBindButton("fr_nearby_key", FriendsSettings::addNearbyKey, FriendsSettings::addNearbyListening);
        ImGui::Spacing();

        ImGui::TextColored(TEXT_DIM, "Clear Friends Key");
        ImGui::SameLine(rowW - 110.f);
        DrawBindButton("fr_clear_key", FriendsSettings::clearFriendsKey, FriendsSettings::clearListening);
        ImGui::Spacing();

        PhantomSliderFloat("Nearby radius", "##fr_rad", FriendsSettings::nearbyRadius, 4.f, 32.f, "%.0f");
    }

    SectionHeader("Actions");
    {
        float rowW = ImGui::GetContentRegionAvail().x;
        float btnW = (rowW - 6.f) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_Button, BG2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG3);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AC_DIM);

        if (ImGui::Button("Clear  All  Friends##fr_clear_btn", { btnW, 28.f }))
            FriendsSettings::ClearAll();

        ImGui::SameLine(0.f, 6.f);

        if (ImGui::Button("Remove Last Friend##fr_remove_btn", { btnW, 28.f }))
            FriendsSettings::RemoveLast();

        ImGui::PopStyleColor(3);
        ImGui::Spacing();
    }

    SectionHeader("Friend List");
    {
        auto list = FriendsSettings::DisplayCopy();
        if (list.empty()) {
            ImGui::TextColored(TEXT_DIM, "  Aucun ami ajoute.");
            ImGui::Spacing();
        } else {
            for (int i = 0; i < (int)list.size(); i++) {
                ImGui::PushID(i);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.f, 0.f, 0.f, 0.f });
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG3);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, AC_DIM);
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
                if (ImGui::Button("x##fr_rm", { 20.f, 0.f }))
                    FriendsSettings::RemoveAt(i);
                ImGui::PopStyleColor(4);
                ImGui::SameLine(0.f, 6.f);
                ImGui::TextColored(TEXT, "%s", list[i].c_str());
                ImGui::PopID();
            }
            ImGui::Spacing();
        }
    }
}

void ClientMenu::RenderEnemiesTab() {
    bool enExpanded = SnapCard("Enemies", "Focus combat and visuals on marked enemies.",
        &EnemiesSettings::enabled, &MenuBinds::en_bind, &MenuBinds::en_listening);

    if (!enExpanded) return;

    SectionHeader("Settings");
    {
        float rowW = ImGui::GetContentRegionAvail().x;
        ImGui::TextColored(TEXT_DIM, "Add Enemies Key");
        ImGui::SameLine(rowW - 110.f);
        DrawBindButton("en_add_key", EnemiesSettings::addEnemyKey, EnemiesSettings::addEnemyListening);
        ImGui::Spacing();

        ImGui::TextColored(TEXT_DIM, "Add Nearby Enemies Key");
        ImGui::SameLine(rowW - 110.f);
        DrawBindButton("en_nearby_key", EnemiesSettings::addNearbyKey, EnemiesSettings::addNearbyListening);
        ImGui::Spacing();

        ImGui::TextColored(TEXT_DIM, "Clear Enemies Key");
        ImGui::SameLine(rowW - 110.f);
        DrawBindButton("en_clear_key", EnemiesSettings::clearEnemiesKey, EnemiesSettings::clearListening);
        ImGui::Spacing();

        PhantomSliderFloat("Nearby radius", "##en_rad", EnemiesSettings::nearbyRadius, 4.f, 32.f, "%.0f");
    }

    SectionHeader("Add by name");
    {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.f);
        bool enter = ImGui::InputText("##en_name", EnemiesSettings::nameInput, sizeof(EnemiesSettings::nameInput),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Add##en_add_btn", { 60.f, 0.f }) || enter) {
            if (EnemiesSettings::nameInput[0]) {
                EnemiesSettings::AddByName(nullptr, EnemiesSettings::nameInput);
                EnemiesSettings::nameInput[0] = 0;
            }
        }
        ImGui::TextColored(TEXT_DIM, "Si le joueur est en ligne, l'UUID est pris. Sinon le nom suffit.");
    }

    SectionHeader("Actions");
    {
        float rowW = ImGui::GetContentRegionAvail().x;
        float btnW = (rowW - 6.f) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_Button, BG2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG3);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AC_DIM);

        if (ImGui::Button("Clear  All  Enemies##en_clear_btn", { btnW, 28.f }))
            EnemiesSettings::ClearAll();

        ImGui::SameLine(0.f, 6.f);

        if (ImGui::Button("Remove Last Enemy##en_remove_btn", { btnW, 28.f }))
            EnemiesSettings::RemoveLast();

        ImGui::PopStyleColor(3);
        ImGui::Spacing();
    }

    SectionHeader("Enemy List");
    {
        auto list = EnemiesSettings::DisplayCopy();
        if (list.empty()) {
            ImGui::TextColored(TEXT_DIM, "  Aucun ennemi ajoute.");
            ImGui::Spacing();
        } else {
            for (int i = 0; i < (int)list.size(); i++) {
                ImGui::PushID(i);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.f, 0.f, 0.f, 0.f });
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG3);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, AC_DIM);
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
                if (ImGui::Button("x##en_rm", { 20.f, 0.f }))
                    EnemiesSettings::RemoveAt(i);
                ImGui::PopStyleColor(4);
                ImGui::SameLine(0.f, 6.f);
                ImGui::TextColored(TEXT, "%s", list[i].c_str());
                ImGui::PopID();
            }
            ImGui::Spacing();
        }
    }
}
// ------------------------------------------------------------------
void ClientMenu::RenderNotificationsTab() {
    bool notifExpanded = SnapCard("Notifications", "Shows enable/disable toasts.",
        &NotificationSettings::enabled, &MenuBinds::notif_bind, &MenuBinds::notif_listening);

    if (!notifExpanded) return;

    // ── Hide If ────────────────────────────────────────────────────
    SectionHeader("Hide If");
    {
        PhantomToggleRow("##notif_hide_ingame", "In-game", NotificationSettings::hideIfInGame);
        PhantomToggleRow("##notif_hide_holdbind", "Hold Bind", NotificationSettings::hideIfHoldBind);
    }

    // ── Visible Mod Categories ─────────────────────────────────────
    SectionHeader("Visible Mod Categories");
    {
        PhantomToggleRow("##notif_cat_combat", "Combat", NotificationSettings::catCombat);
        PhantomToggleRow("##notif_cat_visual", "Visual", NotificationSettings::catVisual);
        PhantomToggleRow("##notif_cat_utility", "Utility", NotificationSettings::catUtility);
    }

    // ── Apparence ──────────────────────────────────────────────────
    SectionHeader("Apparence");
    {
        float rowW = ImGui::GetContentRegionAvail().x;
        ImGui::TextColored(TEXT_DIM, "Duree (secondes)");
        ImGui::SetNextItemWidth(rowW - 2.f);
        ImGui::SliderFloat("##notif_dur", &NotificationSettings::duration, 0.5f, 8.f, "%.1f s");
        ImGui::Spacing();

        ImGui::TextColored(TEXT_DIM, "Vitesse animation");
        ImGui::SetNextItemWidth(rowW - 2.f);
        ImGui::SliderFloat("##notif_spd", &NotificationSettings::animSpeed, 2.f, 20.f, "%.0f");
        ImGui::Spacing();
    }

    // ── Test ───────────────────────────────────────────────────────
    SectionHeader("Test");
    {
        float rowW = ImGui::GetContentRegionAvail().x;
        float btnW = (rowW - 6.f) * 0.5f;
        ImGui::PushStyleColor(ImGuiCol_Button, BG2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG3);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AC_DIM);
        if (ImGui::Button("Test ON##notif_teston", { btnW, 28.f }))
            NotificationSettings::Push("TestModule", "Combat", true);
        ImGui::SameLine(0.f, 6.f);
        if (ImGui::Button("Test OFF##notif_testoff", { btnW, 28.f }))
            NotificationSettings::Push("TestModule", "Visual", false);
        ImGui::PopStyleColor(3);
        ImGui::Spacing();
    }
}

void ClientMenu::RenderSettingsTab() {
    float W = ImGui::GetContentRegionAvail().x;

    static char nameBuf[64] = "";
    static char descBuf[128] = "";
    static char searchBuf[64] = "";

    SectionHeader("Create");
    ImGui::TextColored(TEXT, "Name");
    ImGui::SetNextItemWidth(W);
    ImGui::InputText("##cfg_name", nameBuf, sizeof(nameBuf));
    ImGui::TextColored(TEXT, "Description (optional)");
    ImGui::SetNextItemWidth(W);
    ImGui::InputText("##cfg_desc", descBuf, sizeof(descBuf));
    ImGui::Spacing();
    if (ImGui::Button("Create config", { 140.f, 28.f })) {
        if (nameBuf[0]) {
            ConfigManager::SaveNew(nameBuf, descBuf);
            nameBuf[0] = 0;
            descBuf[0] = 0;
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Saved in %APPDATA%\\lolxd\\configs");

    SectionHeader("Configs");
    ImGui::SetNextItemWidth(W);
    ImGui::InputTextWithHint("##cfg_search", "Search...", searchBuf, sizeof(searchBuf));
    ImGui::Spacing();

    char searchLower[64];
    strncpy_s(searchLower, searchBuf, _TRUNCATE);
    for (char* p = searchLower; *p; ++p) *p = (char)tolower((unsigned char)*p);

    auto list = ConfigManager::List();
    if (list.empty()) {
        ImGui::TextColored(TEXT_DIM, "Aucune config. Cree-en une ci-dessus.");
        return;
    }

    for (auto& cfg : list) {
        if (searchLower[0]) {
            std::string nm = cfg.name;
            for (char& c : nm) c = (char)tolower((unsigned char)c);
            if (nm.find(searchLower) == std::string::npos) continue;
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float h = 72.f;
        dl->AddRectFilled(p, { p.x + W, p.y + h }, ImGui::ColorConvertFloat4ToU32(BG_CARD), 12.f);
        if (cfg.loaded)
            dl->AddRect(p, { p.x + W, p.y + h }, ImGui::ColorConvertFloat4ToU32(AC), 12.f, 0, 1.5f);

        ImGui::Dummy({ W, h });
        ImVec2 after = ImGui::GetCursorPos();

        ImGui::SetCursorScreenPos({ p.x + 14.f, p.y + 10.f });
        ImGui::TextColored(TEXT, "%s", cfg.name.c_str());
        ImGui::SetCursorScreenPos({ p.x + 14.f, p.y + 28.f });
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::TextUnformatted(cfg.description.empty() ? "No description" : cfg.description.c_str());
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({ p.x + 14.f, p.y + 46.f });
        ImGui::TextColored(TEXT_DIM, "%s", cfg.created.c_str());

        const float bw = 72.f, bh = 22.f;
        float bx = p.x + W - 14.f - bw;
        auto btn = [&](const char* id, const char* label, ImU32 col) {
            ImGui::SetCursorScreenPos({ bx, p.y + 12.f });
            bool hit = ImGui::InvisibleButton(id, { bw, bh });
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            dl->AddRectFilled({ bx, p.y + 12.f }, { bx + bw, p.y + 12.f + bh }, col, 10.f);
            ImVec2 ts = ImGui::CalcTextSize(label);
            dl->AddText({ bx + (bw - ts.x) * 0.5f, p.y + 12.f + (bh - ts.y) * 0.5f },
                IM_COL32(240, 240, 240, 255), label);
            bx -= bw + 8.f;
            return hit;
        };

        ImGui::PushID(cfg.id.c_str());
        if (btn("##cfg_del", "Delete", IM_COL32(90, 40, 40, 255)))
            ConfigManager::Delete(cfg.id);
        if (btn("##cfg_upd", "Update", IM_COL32(55, 55, 55, 255)))
            ConfigManager::Update(cfg.id);
        ImU32 loadCol = cfg.loaded ? ImGui::ColorConvertFloat4ToU32(AC) : IM_COL32(55, 55, 55, 255);
        if (btn("##cfg_load", cfg.loaded ? "Loaded" : "Load", loadCol) && !cfg.loaded)
            ConfigManager::Load(cfg.id);
        ImGui::PopID();

        ImGui::SetCursorPos(after);
        ImGui::Dummy({ 0.f, 8.f });
    }
}

void ClientMenu::RenderUnloadTab() {
    float W = ImGui::GetContentRegionAvail().x;

    SectionHeader("Keybinds");
    ImGui::TextColored(TEXT_DIM, "Ouvrir / Fermer");
    ImGui::SameLine(W - 108.f);
    DrawBindButton("open_bind", MenuBinds::open_bind, MenuBinds::open_listening);

    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "Destruct ejecte le client de la memoire.");
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.14f, 0.05f, 0.05f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_Border, { 0.55f, 0.10f, 0.10f, 0.8f });
    ImGui::BeginChild("##destruct_box", { W, 72.f }, true, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
    ImGui::TextColored(RED, "DESTRUCT");
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "Bind :"); ImGui::SameLine();
    DrawBindButton("destruct_bind", MenuBinds::destruct_bind, MenuBinds::destruct_listening);
    ImGui::SameLine(0.f, 12.f);
    ImGui::PushStyleColor(ImGuiCol_Button, RED);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RED_H);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 1.f, 0.f, 0.f, 1.f });
    if (ImGui::Button("Ejecter maintenant", { 155.f, 22.f }))
        Communication::GetSettings()->m_Destruct = true;
    ImGui::PopStyleColor(3);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ====================================================================
//  ON IMGUI RENDER
// ====================================================================
void ClientMenu::OnImGuiRender(JNIEnv* env) {
    ApplyTheme();

    bool bindGracePassed = (GetTickCount64() - g_bindReleasedAt) > 500;

    if (!AnyListening() && bindGracePassed) {
        // Aim Assist bind
        if (MenuBinds::aa_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::aa_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* aa = dynamic_cast<AimAssist*>(m);
                if (aa) {
                    aa->enabled = !aa->enabled;
                    NotificationSettings::Push("Aim Assist", "Combat", aa->enabled);
                    break;
                }
            }
            p = n;
        }
        // Clicker bind
        if (MenuBinds::lc_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::lc_bind) & 0x8000) != 0;
            if (n && !p) {
                Clicker::enabled = !Clicker::enabled;
                NotificationSettings::Push("Auto Clicker", "Combat", Clicker::enabled);
            }
            p = n;
        }
        // Velocity bind
        if (MenuBinds::vel_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::vel_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* v = dynamic_cast<Velocity*>(m);
                if (v) {
                    v->enabled = !v->enabled;
                    NotificationSettings::Push("Velocity", "Combat", v->enabled);
                    break;
                }
            }
            p = n;
        }
        // Piercing bind
        if (MenuBinds::prc_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::prc_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* pr = dynamic_cast<Piercing*>(m);
                if (pr) {
                    pr->enabled = !pr->enabled;
                    NotificationSettings::Push("Piercing", "Combat", pr->enabled);
                    break;
                }
            }
            p = n;
        }
        // KeepSprint bind
        if (MenuBinds::ks_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::ks_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* ks = dynamic_cast<KeepSprint*>(m);
                if (ks) {
                    ks->enabled = !ks->enabled;
                    NotificationSettings::Push("KeepSprint", "Combat", ks->enabled);
                    break;
                }
            }
            p = n;
        }
        // Criticals bind
        if (MenuBinds::cr_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::cr_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* cr = dynamic_cast<Criticals*>(m);
                if (cr) {
                    cr->enabled = !cr->enabled;
                    NotificationSettings::Push("Criticals", "Combat", cr->enabled);
                    break;
                }
            }
            p = n;
        }
        // AutoRod bind
        if (MenuBinds::rod_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::rod_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* rod = dynamic_cast<AutoRod*>(m);
                if (rod) {
                    rod->enabled = !rod->enabled;
                    NotificationSettings::Push("AutoRod", "Combat", rod->enabled);
                    break;
                }
            }
            p = n;
        }
        // AntiBot bind
        if (MenuBinds::ab_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::ab_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* ab = dynamic_cast<AntiBot*>(m);
                if (ab) {
                    ab->enabled = !ab->enabled;
                    NotificationSettings::Push("AntiBot", "Combat", ab->enabled);
                    break;
                }
            }
            p = n;
        }
        // ArrayList bind
        if (MenuBinds::al_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::al_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* al = dynamic_cast<ArrayList*>(m);
                if (al) {
                    al->enabled = !al->enabled;
                    NotificationSettings::Push("ArrayList", "Visual", al->enabled);
                    break;
                }
            }
            p = n;
        }
        // Chams bind
        if (MenuBinds::ch_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::ch_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* ch = dynamic_cast<Chams*>(m);
                if (ch) {
                    ch->enabled = !ch->enabled;
                    NotificationSettings::Push("Chams", "Visual", ch->enabled);
                    break;
                }
            }
            p = n;
        }
        // ESP bind
        if (MenuBinds::esp_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::esp_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* esp = dynamic_cast<Esp*>(m);
                if (esp) {
                    esp->enabled = !esp->enabled;
                    NotificationSettings::Push("ESP", "Visual", esp->enabled);
                    break;
                }
            }
            p = n;
        }
        // Item ESP bind
        if (MenuBinds::itemesp_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::itemesp_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* ie = dynamic_cast<ItemEsp*>(m);
                if (ie) {
                    ie->enabled = !ie->enabled;
                    NotificationSettings::Push("Item ESP", "Visual", ie->enabled);
                    break;
                }
            }
            p = n;
        }
        // Player ESP bind
        if (MenuBinds::pesp_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::pesp_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* pe = dynamic_cast<PlayerEsp*>(m);
                if (pe) {
                    pe->enabled = !pe->enabled;
                    NotificationSettings::Push("Player ESP", "Visual", pe->enabled);
                    break;
                }
            }
            p = n;
        }
        // Storage ESP bind
        if (MenuBinds::sesp_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::sesp_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* se = dynamic_cast<StorageEsp*>(m);
                if (se) {
                    se->enabled = !se->enabled;
                    NotificationSettings::Push("Storage ESP", "Visual", se->enabled);
                    break;
                }
            }
            p = n;
        }
        // Nametags bind
        if (MenuBinds::ntag_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::ntag_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* nt = dynamic_cast<Nametag*>(m);
                if (nt) {
                    nt->enabled = !nt->enabled;
                    NotificationSettings::Push("Nametags", "Visual", nt->enabled);
                    break;
                }
            }
            p = n;
        }
        // Tracer bind
        if (MenuBinds::tr_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::tr_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* tr = dynamic_cast<Tracer*>(m);
                if (tr) {
                    tr->enabled = !tr->enabled;
                    NotificationSettings::Push("Tracer", "Visual", tr->enabled);
                    break;
                }
            }
            p = n;
        }
        // Trajectories bind
        if (MenuBinds::tj_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::tj_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* tj = dynamic_cast<Trajectories*>(m);
                if (tj) {
                    tj->enabled = !tj->enabled;
                    NotificationSettings::Push("Trajectories", "Visual", tj->enabled);
                    break;
                }
            }
            p = n;
        }
        // Scroll bind
        if (Scroll::scroll_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(Scroll::scroll_bind) & 0x8000) != 0;
            if (n && !p) {
                Scroll::triggerRequested = true;
                NotificationSettings::Push("Scroll", "Utility", true);
            }
            p = n;
        }
        // Armor bind
        if (Armor::bind && Armor::enabled) {
            static bool p = false;
            bool n = (GetAsyncKeyState(Armor::bind) & 0x8000) != 0;
            if (n && !p) {
                Armor_Request_Scan();
                NotificationSettings::Push("Armor Switcher", "Utility", true);
            }
            p = n;
        }
        // FastPlace bind
        if (MenuBinds::fp_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::fp_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* fp = dynamic_cast<FastPlace*>(m);
                if (fp) {
                    fp->enabled = !fp->enabled;
                    NotificationSettings::Push("FastPlace", "Utility", fp->enabled);
                    break;
                }
            }
            p = n;
        }
        // FastBreak bind
        if (MenuBinds::fb_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::fb_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* fb = dynamic_cast<FastBreak*>(m);
                if (fb) {
                    fb->enabled = !fb->enabled;
                    NotificationSettings::Push("FastBreak", "Utility", fb->enabled);
                    break;
                }
            }
            p = n;
        }
        // TickLocker bind
        if (g_GameVersion == LUNAR_1_8_9 && MenuBinds::tl_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::tl_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* tl = dynamic_cast<TickLocker*>(m);
                if (tl) {
                    tl->enabled = !tl->enabled;
                    NotificationSettings::Push("TickLocker", "Utility", tl->enabled);
                    break;
                }
            }
            p = n;
        }
        // InvWalk bind
        if (MenuBinds::iw_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::iw_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* iw = dynamic_cast<InvWalk*>(m);
                if (iw) {
                    iw->enabled = !iw->enabled;
                    NotificationSettings::Push("InvWalk", "Utility", iw->enabled);
                    break;
                }
            }
            p = n;
        }
        // FastStop bind
        if (MenuBinds::fs_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::fs_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* fs = dynamic_cast<FastStop*>(m);
                if (fs) {
                    fs->enabled = !fs->enabled;
                    NotificationSettings::Push("FastStop", "Utility", fs->enabled);
                    break;
                }
            }
            p = n;
        }
        // NoJumpDelay bind
        if (MenuBinds::njd_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::njd_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* njd = dynamic_cast<NoJumpDelay*>(m);
                if (njd) {
                    njd->enabled = !njd->enabled;
                    NotificationSettings::Push("NoJumpDelay", "Utility", njd->enabled);
                    break;
                }
            }
            p = n;
        }
        // QuickAccel bind
        if (MenuBinds::qa_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::qa_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* qa = dynamic_cast<QuickAccel*>(m);
                if (qa) {
                    qa->enabled = !qa->enabled;
                    NotificationSettings::Push("QuickAccel", "Utility", qa->enabled);
                    break;
                }
            }
            p = n;
        }
        // SnapTap bind
        if (MenuBinds::st_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::st_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* st = dynamic_cast<SnapTap*>(m);
                if (st) {
                    st->enabled = !st->enabled;
                    NotificationSettings::Push("SnapTap", "Utility", st->enabled);
                    break;
                }
            }
            p = n;
        }
        // Friends bind
        if (MenuBinds::fr_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::fr_bind) & 0x8000) != 0;
            if (n && !p) FriendsSettings::enabled = !FriendsSettings::enabled;
            p = n;
        }
        // Enemies bind
        if (MenuBinds::en_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::en_bind) & 0x8000) != 0;
            if (n && !p) {
                EnemiesSettings::enabled = !EnemiesSettings::enabled;
                NotificationSettings::Push("Enemies", "Utility", EnemiesSettings::enabled);
            }
            p = n;
        }
        // Notifications bind
        if (MenuBinds::notif_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::notif_bind) & 0x8000) != 0;
            if (n && !p) NotificationSettings::enabled = !NotificationSettings::enabled;
            p = n;
        }
        // AutoRefill bind
        if (MenuBinds::ar_bind) {
            static bool p = false;
            bool n = (GetAsyncKeyState(MenuBinds::ar_bind) & 0x8000) != 0;
            if (n && !p) for (auto* m : Modules::GetRegisteredModules()) {
                auto* ar = dynamic_cast<AutoRefill*>(m);
                if (ar && ar->enabled) { AutoRefill_Trigger(); break; }
            }
            p = n;
        }
        // Destruct bind
        if (MenuBinds::destruct_bind) {
            bool n = (GetAsyncKeyState(MenuBinds::destruct_bind) & 0x8000) != 0;
            if (n && !m_prevDestructKey)
                Communication::GetSettings()->m_Destruct = true;
            m_prevDestructKey = n;
        }
        else { m_prevDestructKey = false; }
    }

    // Toggle menu — uniquement si Lunar est au premier plan
    if (!AnyListening() && bindGracePassed) {
        bool lunarFocused = IsGameWindowFocused();

        if (lunarFocused) {
            bool now = MenuBinds::open_bind ? (GetAsyncKeyState(MenuBinds::open_bind) & 0x8000) != 0 : false;
            if (now && !m_prevToggleKey) Overlay::isOpen = !Overlay::isOpen;
            m_prevToggleKey = now;
        }
        else {
            // Pas au premier plan → forcer la fermeture du menu
            if (Overlay::isOpen) Overlay::isOpen = false;
            m_prevToggleKey = false;
        }
    }
    else { m_prevToggleKey = false; }

    if (!Overlay::isOpen) return;

    // ── Fenetre principale ────────────────────────────────────────
    const float WIN_W = 900.f;
    const float WIN_H = 640.f;
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowSize({ WIN_W, WIN_H }, ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        { io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f },
        ImGuiCond_Once, { 0.5f, 0.5f });

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG0);
    ImGui::PushStyleColor(ImGuiCol_Border, { 0.f, 0.f, 0.f, 0.f });
    ImGui::Begin("##phantom_main", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar);
    ImGui::PopStyleColor(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    dl->AddRect(wp, { wp.x + WIN_W, wp.y + WIN_H }, IM_COL32(40, 40, 40, 80), 16.f);

    static const char* navLabels[] = { "Combat","Move","Visual","Block","Utility","Profiles","Unload" };
    const int NAV_N = 7;
    const float pillH = 28.f;
    const float padX = 22.f;
    float totalNav = 0.f;
    for (int i = 0; i < NAV_N; i++)
        totalNav += ImGui::CalcTextSize(navLabels[i]).x + padX + (i ? 4.f : 0.f);

    float navX = (WIN_W - totalNav) * 0.5f;
    ImGui::SetCursorPos({ navX, 12.f });
    for (int i = 0; i < NAV_N; i++) {
        if (i > 0) ImGui::SameLine(0.f, 4.f);
        bool isUnload = (i == NAV_N - 1);
        bool active = (m_activeTab == i);
        ImVec2 ts = ImGui::CalcTextSize(navLabels[i]);
        float pw = ts.x + padX;
        ImVec2 p = ImGui::GetCursorScreenPos();
        std::string nid = std::string("##nav_") + navLabels[i];
        if (ImGui::InvisibleButton(nid.c_str(), { pw, pillH }))
            m_activeTab = i;
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (active)
            dl->AddRectFilled(p, { p.x + pw, p.y + pillH }, IM_COL32(52, 52, 52, 255), 14.f);
        ImU32 col = isUnload ? ImGui::ColorConvertFloat4ToU32(RED)
            : (active ? IM_COL32(230, 230, 230, 255) : IM_COL32(150, 150, 150, 255));
        dl->AddText({ p.x + (pw - ts.x) * 0.5f, p.y + (pillH - ts.y) * 0.5f }, col, navLabels[i]);
    }

    ImGui::Dummy({ 0.f, 8.f });

    // ── Contenu ───────────────────────────────── ──────────────────
    float contentH = WIN_H - ImGui::GetCursorPosY() - 12.f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.f, 0.f, 0.f, 0.f });
    ImGui::BeginChild("##content", { WIN_W - 28.f, contentH }, false);

    auto twoCol = [](auto&& leftFn, auto&& rightFn) {
        float avail = ImGui::GetContentRegionAvail().x;
        float colW = (avail - 10.f) * 0.5f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.f, 0.f, 0.f, 0.f });
        ImGui::BeginChild("##col_a", { colW, 0.f }, false);
        leftFn();
        ImGui::EndChild();
        ImGui::SameLine(0.f, 10.f);
        ImGui::BeginChild("##col_b", { colW, 0.f }, false);
        rightFn();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    };

    switch (m_activeTab) {
    case 0: RenderCombatTab(); break;
    case 1:
        twoCol([&] {
            RenderInvWalkTab();
            RenderFastStopTab();
            RenderNoJumpDelayTab();
        }, [&] {
            RenderQuickAccelTab();
            RenderSnapTapTab();
        });
        break;
    case 2:
        twoCol([&] {
            RenderArrayListTab();
            RenderEspTab();
            RenderItemEspTab();
            RenderPlayerEspTab();
            RenderStorageEspTab();
        }, [&] {
            RenderNametagTab();
            RenderTracerTab();
            RenderTrajectoriesTab();
            RenderChamsTab();
            RenderNotificationsTab();
        });
        break;
    case 3:
        twoCol([&] {
            RenderFastPlaceTab();
            if (g_GameVersion == LUNAR_1_8_9)
                RenderTickLockerTab();
        }, [&] { RenderFastBreakTab(); });
        break;
    case 4:
        twoCol([&] {
            RenderScrollTab();
            RenderArmorTab();
        }, [&] {
            RenderFriendsTab();
            RenderEnemiesTab();
        });
        break;
    case 5: RenderSettingsTab(); break;
    case 6: RenderUnloadTab(); break;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
}
