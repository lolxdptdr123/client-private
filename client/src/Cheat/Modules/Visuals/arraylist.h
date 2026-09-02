#pragma once
#include "../Module.h"
#include <jni.h>

// ============================================================
//  ArrayListSettings — couleurs & style configurables via menu
// ============================================================
namespace ArrayListSettings {
    // Background de chaque entrée
    inline bool  bgEnabled = true;
    inline float bgColor[4] = { 0.f, 0.f, 0.f, 0.40f };  // RGBA

    // Couleur du nom du module  (ex: "Clicker")
    inline float nameColor[4] = { 1.f, 1.f, 1.f, 1.f };

    // Couleur du suffix/settings (ex: "17 CPS")
    inline float suffixColor[4] = { 0.15f, 0.93f, 0.88f, 0.60f };

    // Barre décorative côté droit
    inline bool  barEnabled = true;
    inline float barColor[4] = { 0.15f, 0.93f, 0.88f, 0.60f };

    // Taille de police (multiplicateur)
    inline float fontSize = 1.3f;

    // Vitesse du scroll effect (lerp)
    inline float scrollSpeed = 12.f;  // 1 = très lent, 30 = quasi instantané

    // Titre affiché en haut de l'ArrayList
    inline bool  showTitle = false;
    inline char  titleText[64] = "lolxd private";
    inline float titleColor[4] = { 1.f, 1.f, 1.f, 1.f };
}

class ArrayList : public Module
{
public:
    bool enabled = false;
    const char* GetName()   override { return "ArrayList"; }
    bool        IsEnabled() override { return enabled; }
    virtual void OnImGuiRender(JNIEnv* env) override;
};