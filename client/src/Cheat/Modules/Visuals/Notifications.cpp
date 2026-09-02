// ============================================================
//  Notifications.cpp
// ============================================================
#include "pch.h"
#include "Notifications.h"
#include "../Module.h"
#include "../../../../vendors/imgui/imgui.h"
#include <cmath>

static float g_lastTime = 0.f;

void NotificationsModule::OnImGuiRender(JNIEnv* env) {
    if (!NotificationSettings::enabled) return;
    if (NotificationSettings::queue.empty()) return;

    if (NotificationSettings::hideIfInGame &&
        g_playerInGame.load(std::memory_order_relaxed)) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const float toastW = 220.f;
    const float toastH = 48.f;
    const float padding = 8.f;
    const float marginR = 12.f;
    const float marginT = 12.f;

    float now = (float)ImGui::GetTime();
    float dt = now - g_lastTime;
    g_lastTime = now;
    if (dt <= 0.f || dt > 0.1f) dt = 0.016f;

    const float speed = NotificationSettings::animSpeed;
    const float alphaX = 1.f - expf(-speed * dt);

    const ImU32 colBg = IM_COL32(18, 18, 18, 220);
    const ImU32 colBorder = IM_COL32(30, 30, 30, 255);
    const ImU32 colAccent = IM_COL32(38, 237, 224, 255);
    const ImU32 colOn = IM_COL32(38, 237, 224, 255);
    const ImU32 colOff = IM_COL32(180, 60, 60, 255);
    const ImU32 colInfo = IM_COL32(255, 200, 60, 255); // jaune pour Info
    const ImU32 colName = IM_COL32(235, 235, 235, 255);
    const ImU32 colShadow = IM_COL32(0, 0, 0, 120);

    ImFont* font = ImGui::GetFont();
    const float fSize = ImGui::GetFontSize();

    auto A = [](ImU32 col, float a) -> ImU32 {
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
        c.w *= a;
        return ImGui::ColorConvertFloat4ToU32(c);
        };

    std::vector<int> toRemove;
    int i = 0;

    for (auto& n : NotificationSettings::queue) {
        // Slide IN / OUT
        if (n.slidingOut) {
            n.slideX += (300.f - n.slideX) * alphaX;
            if (n.slideX >= 299.f) { toRemove.push_back(i++); continue; }
        }
        else {
            n.slideX += (0.f - n.slideX) * alphaX;
            if (n.slideX < 0.5f) n.slideX = 0.f;
            if (n.slideX < 1.f) {
                n.timeLeft -= dt;
                if (n.timeLeft <= 0.f) n.slidingOut = true;
            }
        }

        float y = marginT + i * (toastH + padding);
        float x = io.DisplaySize.x - toastW - marginR + n.slideX;

        float alpha = 1.f;
        if (!n.slidingOut && n.timeLeft < 0.5f) alpha = n.timeLeft / 0.5f;
        if (alpha < 0.f) alpha = 0.f;

        // Background + bordure
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + toastW, y + toastH),
            A(colBg, alpha), 6.f);
        dl->AddRect(ImVec2(x, y), ImVec2(x + toastW, y + toastH),
            A(colBorder, alpha), 6.f, 0, 1.f);

        // Barre latérale
        ImU32 barCol;
        if (n.type == NotificationSettings::Type::Info) barCol = colInfo;
        else barCol = n.enabled ? colAccent : colOff;
        dl->AddRectFilled(ImVec2(x, y + 4.f), ImVec2(x + 3.f, y + toastH - 4.f),
            A(barCol, alpha), 2.f);

        float tx = x + 12.f;

        // Ligne 1 : titre (nom module ou titre info)
        float ty1 = y + 8.f;
        dl->AddText(font, fSize, ImVec2(tx + 1.f, ty1 + 1.f), A(colShadow, alpha), n.moduleName.c_str());
        dl->AddText(font, fSize, ImVec2(tx, ty1), A(colName, alpha), n.moduleName.c_str());

        // Ligne 2 : sous-texte (ON/OFF ou message custom)
        float ty2 = ty1 + fSize + 2.f;
        ImU32 subCol;
        if (n.type == NotificationSettings::Type::Info)
            subCol = colInfo;
        else
            subCol = n.enabled ? colOn : colOff;
        dl->AddText(font, fSize, ImVec2(tx + 1.f, ty2 + 1.f), A(colShadow, alpha), n.subText.c_str());
        dl->AddText(font, fSize, ImVec2(tx, ty2), A(subCol, alpha), n.subText.c_str());

        // Catégorie (coin haut droit)
        float catW = ImGui::CalcTextSize(n.category.c_str()).x;
        float catX = x + toastW - catW - 8.f;
        dl->AddText(font, fSize * 0.85f, ImVec2(catX, ty1 + 2.f),
            A(IM_COL32(100, 100, 100, 255), alpha), n.category.c_str());

        i++;
    }

    for (int j = (int)toRemove.size() - 1; j >= 0; j--)
        NotificationSettings::queue.erase(
            NotificationSettings::queue.begin() + toRemove[j]);
}