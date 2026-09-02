#pragma once
#include "../Module.h"
#include <string>
#include <vector>
#include <deque>
#include <jni.h>

namespace NotificationSettings {

    inline bool enabled = false;

    inline bool hideIfInGame = false;
    inline bool hideIfHoldBind = false;

    inline bool catCombat = true;
    inline bool catVisual = true;
    inline bool catUtility = true;

    inline float duration = 3.0f;
    inline float animSpeed = 8.0f;

    // ── Type de notification ──────────────────────────────────────
    enum class Type { Toggle, Info };

    struct Notification {
        std::string moduleName; // titre (ligne 1)
        std::string subText;    // sous-titre (ligne 2) — ex: "2 friends added"
        std::string category;
        Type        type;
        bool        enabled;    // pour Type::Toggle : true=ON, false=OFF
        float       timeLeft;
        float       slideX;
        bool        slidingOut;
    };

    inline std::deque<Notification> queue;

    // ── Push Toggle (module ON/OFF) ───────────────────────────────
    inline void Push(const char* moduleName, const char* category, bool isEnabled) {
        if (!enabled) return;
        std::string cat = category ? category : "Utility";
        if (cat == "Combat" && !catCombat)  return;
        if (cat == "Visual" && !catVisual)  return;
        if (cat == "Utility" && !catUtility) return;

        for (auto& n : queue) {
            if (n.moduleName == moduleName && !n.slidingOut) {
                n.enabled = isEnabled;
                n.subText = isEnabled ? "ON" : "OFF";
                n.timeLeft = duration;
                return;
            }
        }
        Notification n;
        n.moduleName = moduleName;
        n.subText = isEnabled ? "ON" : "OFF";
        n.category = cat;
        n.type = Type::Toggle;
        n.enabled = isEnabled;
        n.timeLeft = duration;
        n.slideX = 300.f;
        n.slidingOut = false;
        queue.push_back(n);
        while (queue.size() > 5) queue.pop_front();
    }

    // ── Push Info (message custom, ex: "2 friends added") ─────────
    inline void PushInfo(const char* title, const char* message,
        const char* category = "Utility") {
        if (!enabled) return;
        std::string cat = category ? category : "Utility";
        if (cat == "Combat" && !catCombat)  return;
        if (cat == "Visual" && !catVisual)  return;
        if (cat == "Utility" && !catUtility) return;

        // Remplacer si même titre déjà dans la queue
        for (auto& n : queue) {
            if (n.moduleName == title && !n.slidingOut) {
                n.subText = message ? message : "";
                n.timeLeft = duration;
                return;
            }
        }
        Notification n;
        n.moduleName = title;
        n.subText = message ? message : "";
        n.category = cat;
        n.type = Type::Info;
        n.enabled = true;
        n.timeLeft = duration;
        n.slideX = 300.f;
        n.slidingOut = false;
        queue.push_back(n);
        while (queue.size() > 5) queue.pop_front();
    }
}

class NotificationsModule : public Module {
public:
    const char* GetName()   override { return "Notifications"; }
    bool        IsEnabled() override { return NotificationSettings::enabled; }
    const char* GetSuffix() override { return ""; }
    void OnImGuiRender(JNIEnv* env) override;
};