// ============================================================
//  ArrayList.cpp — Phantom style
//  - Background configurable (couleur + alpha)
//  - Suffix des settings affiché (ex: "17 CPS", "Regular")
//  - Couleur du nom configurable
//  - Couleur du suffix configurable
//  - Barre droite configurable
//  - Scroll effect : lerp Y + slide-in/out X keyed par nom
//  - Option "Show Title" : affiche un titre fixe en haut
// ============================================================

#include "pch.h"
#include "ArrayList.h"
#include "../Module.h"
#include "../../../../vendors/imgui/imgui.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

// ── Etat d'animation par module (keyed par nom) ────────────────────
struct EntryAnim {
    float currentY = 0.f;   // position Y animée
    float targetY = 0.f;   // position Y cible
    float slideX = 0.f;   // offset X slide : 0=en place, >0=décalé à droite
    bool  alive = false;  // module encore actif ce frame
    bool  removing = false;  // en slide-out (module désactivé)
};

static std::unordered_map<std::string, EntryAnim> g_anims;
static float g_lastTime = 0.f;

void ArrayList::OnImGuiRender(JNIEnv* env) {
    if (!enabled) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const float fSize = ImGui::GetFontSize() * ArrayListSettings::fontSize;
    const float padding = 5.f;
    const float barW = ArrayListSettings::barEnabled ? 3.f : 0.f;
    const float gap = 4.f;
    const float screenW = io.DisplaySize.x;
    const float entryH = fSize + padding * 2.f;

    // ── Hauteur du titre ────────────────────────────────────────────
    const float titleH = ArrayListSettings::showTitle ? (entryH + padding) : 0.f;

    // ── Delta time ──────────────────────────────────────────────────
    float now = (float)ImGui::GetTime();
    float dt = now - g_lastTime;
    g_lastTime = now;
    if (dt <= 0.f || dt > 0.1f) dt = 0.016f;

    const float lerpSpeed = ArrayListSettings::scrollSpeed;
    const float slideSpeed = lerpSpeed * 1.5f;

    // ── Collecter modules actifs ────────────────────────────────────
    std::vector<Module*> active;
    for (auto* mod : Modules::GetRegisteredModules()) {
        if (mod == this)                  continue;
        if (!mod->IsEnabled())            continue;
        if (strlen(mod->GetName()) == 0)  continue;
        active.push_back(mod);
    }

    // ── Trier par largeur pixel décroissante (cascade propre) ──────────────
    // CalcTextSize donne la vraie largeur avec la font proportionnelle.
    // strlen est insuffisant : "Clicker" et "Velocity" peuvent avoir
    // des longueurs proches en chars mais des widths très différentes.
    std::sort(active.begin(), active.end(), [&](Module* a, Module* b) {
        float wa = ImGui::CalcTextSize(a->GetName()).x
            + (a->GetSuffix() && a->GetSuffix()[0] ? ImGui::CalcTextSize(a->GetSuffix()).x + gap : 0.f);
        float wb = ImGui::CalcTextSize(b->GetName()).x
            + (b->GetSuffix() && b->GetSuffix()[0] ? ImGui::CalcTextSize(b->GetSuffix()).x + gap : 0.f);
        return wa > wb;
        });

    // ── Reset alive flags ───────────────────────────────────────────
    for (auto& kv : g_anims)
        kv.second.alive = false;

    // ── Assigner targetY + créer nouvelles entrées ──────────────────
    float y = padding + titleH;
    for (auto* mod : active) {
        std::string key = mod->GetName();
        auto it = g_anims.find(key);
        if (it == g_anims.end()) {
            // Nouvelle entrée : démarre hors écran à droite
            EntryAnim ea;
            ea.currentY = y;
            ea.targetY = y;
            ea.slideX = screenW;   // commence complètement à droite
            ea.alive = true;
            ea.removing = false;
            g_anims[key] = ea;
        }
        else {
            it->second.targetY = y;
            it->second.alive = true;
            it->second.removing = false;
        }
        y += entryH;
    }

    // ── Marquer les disparus en removing ───────────────────────────
    for (auto& kv : g_anims)
        if (!kv.second.alive && !kv.second.removing)
            kv.second.removing = true;

    // ── Lerp Y + slide X ───────────────────────────────────────────
    const float alphaY = 1.f - expf(-lerpSpeed * dt);
    const float alphaX = 1.f - expf(-slideSpeed * dt);

    std::vector<std::string> toErase;
    for (auto& kv : g_anims) {
        EntryAnim& ea = kv.second;
        ea.currentY += (ea.targetY - ea.currentY) * alphaY;

        if (ea.removing) {
            // Slide OUT : slideX remonte vers screenW
            ea.slideX += (screenW - ea.slideX) * alphaX;
            if (ea.slideX >= screenW - 1.f)
                toErase.push_back(kv.first);
        }
        else {
            // Slide IN : slideX descend vers 0
            ea.slideX += (0.f - ea.slideX) * alphaX;
            if (ea.slideX < 0.5f) ea.slideX = 0.f;
        }
    }
    for (auto& k : toErase) g_anims.erase(k);

    // ── Couleurs ────────────────────────────────────────────────────
    ImU32 colName = ImGui::ColorConvertFloat4ToU32(ImVec4(
        ArrayListSettings::nameColor[0], ArrayListSettings::nameColor[1],
        ArrayListSettings::nameColor[2], ArrayListSettings::nameColor[3]));

    ImU32 colSuffix = ImGui::ColorConvertFloat4ToU32(ImVec4(
        ArrayListSettings::suffixColor[0], ArrayListSettings::suffixColor[1],
        ArrayListSettings::suffixColor[2], ArrayListSettings::suffixColor[3]));

    ImU32 colBar = ImGui::ColorConvertFloat4ToU32(ImVec4(
        ArrayListSettings::barColor[0], ArrayListSettings::barColor[1],
        ArrayListSettings::barColor[2], ArrayListSettings::barColor[3]));

    ImU32 colBg = ImGui::ColorConvertFloat4ToU32(ImVec4(
        ArrayListSettings::bgColor[0], ArrayListSettings::bgColor[1],
        ArrayListSettings::bgColor[2], ArrayListSettings::bgColor[3]));

    ImU32 colShadow = IM_COL32(0, 0, 0, 120);

    // ── Titre fixe en haut ──────────────────────────────────────────
    if (ArrayListSettings::showTitle) {
        const char* title = ArrayListSettings::titleText;
        float titleW = ImGui::CalcTextSize(title).x * ArrayListSettings::fontSize;
        float tx = screenW - titleW - padding * 2.f - barW;
        float ty = padding;

        ImU32 colTitle = ImGui::ColorConvertFloat4ToU32(ImVec4(
            ArrayListSettings::titleColor[0], ArrayListSettings::titleColor[1],
            ArrayListSettings::titleColor[2], ArrayListSettings::titleColor[3]));

        // Background titre
        if (ArrayListSettings::bgEnabled)
            dl->AddRectFilled(
                ImVec2(tx - padding, ty),
                ImVec2(screenW, ty + entryH),
                colBg, 2.f);

        // Barre droite titre
        if (ArrayListSettings::barEnabled)
            dl->AddRectFilled(
                ImVec2(screenW - barW, ty),
                ImVec2(screenW, ty + entryH),
                colBar);

        // Texte titre (ombre + couleur)
        dl->AddText(ImGui::GetFont(), fSize, ImVec2(tx + 1.f, ty + padding + 1.f), colShadow, title);
        dl->AddText(ImGui::GetFont(), fSize, ImVec2(tx, ty + padding), colTitle, title);
    }

    if (g_anims.empty()) return;

    // ── Helper lambda de rendu ──────────────────────────────────────
    auto renderEntry = [&](const char* name, const char* suffix, EntryAnim& ea) {
        float cy = ea.currentY;
        float slideOff = ea.slideX;

        float nameW = ImGui::CalcTextSize(name).x * ArrayListSettings::fontSize;
        float suffixW = (suffix && suffix[0] != '\0')
            ? ImGui::CalcTextSize(suffix).x * ArrayListSettings::fontSize + gap
            : 0.f;
        float totalW = nameW + suffixW;

        // x de base (position finale) + offset slide
        float x = screenW - totalW - padding * 2.f - barW + slideOff;

        if (x >= screenW) return;   // hors écran, skip

        // Background
        if (ArrayListSettings::bgEnabled)
            dl->AddRectFilled(
                ImVec2(x - padding, cy),
                ImVec2(screenW + slideOff, cy + entryH),
                colBg, 2.f);

        // Barre droite
        if (ArrayListSettings::barEnabled)
            dl->AddRectFilled(
                ImVec2(screenW - barW + slideOff, cy),
                ImVec2(screenW + slideOff, cy + entryH),
                colBar);

        // Nom
        float textY = cy + padding;
        dl->AddText(ImGui::GetFont(), fSize, ImVec2(x + 1.f, textY + 1.f), colShadow, name);
        dl->AddText(ImGui::GetFont(), fSize, ImVec2(x, textY), colName, name);

        // Suffix
        if (suffix && suffix[0] != '\0') {
            float sx = x + nameW + gap;
            dl->AddText(ImGui::GetFont(), fSize, ImVec2(sx + 1.f, textY + 1.f), colShadow, suffix);
            dl->AddText(ImGui::GetFont(), fSize, ImVec2(sx, textY), colSuffix, suffix);
        }
        };

    // ── Render modules actifs (ordre trié) ──────────────────────────
    for (auto* mod : active) {
        std::string key = mod->GetName();
        auto it = g_anims.find(key);
        if (it == g_anims.end()) continue;
        renderEntry(mod->GetName(), mod->GetSuffix(), it->second);
    }

    // ── Render entrées en slide-out ──────────────────────────────────
    for (auto& kv : g_anims) {
        if (!kv.second.removing) continue;
        static const char* empty = "";
        renderEntry(kv.first.c_str(), empty, kv.second);
    }
}