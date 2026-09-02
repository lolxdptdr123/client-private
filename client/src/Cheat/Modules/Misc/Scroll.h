#pragma once
#include "pch.h"
#include "../Module.h"
#include <atomic>

namespace Scroll {

    enum WhitelistItem {
        ITEM_GHAST_TEAR = 0,
        ITEM_BLAZE_POWDER = 1,
        ITEM_MAGMA_CREAM = 2,
        ITEM_IRON_INGOT = 3,
        ITEM_SUGAR = 4,
        ITEM_FEATHER = 5,
        ITEM_COUNT = 6
    };

    inline const char* g_itemNames[ITEM_COUNT] = {
        "Ghast Tear", "Blaze Powder", "Magma Cream",
        "Iron Ingot", "Sugar", "Feather"
    };

    inline const int g_itemIds[ITEM_COUNT] = {
        370, // Ghast Tear
        377, // Blaze Powder
        378, // Magma Cream
        265, // Iron Ingot
        353, // Sugar
        288  // Feather
    };

    inline bool g_whitelistEnabled[ITEM_COUNT] = {
        false, false, false, false, false, false
    };

    inline bool enabled = false;
    inline int  scroll_bind = 0;
    inline bool scroll_listen = false;
    inline int  scrollDelay = 50;

    inline int  hotbarKeys[9] = { -1,-1,-1,-1,-1,-1,-1,-1,-1 };

    inline int  swordSlot = -1;

    inline std::atomic<bool> triggerRequested{ false };

    void Start();
    void Stop();
    void Trigger(JNIEnv* env);
    void ReloadKeybinds();
    void ScrollToSlot(int slot);
}

class ScrollModule : public Module {
public:
    const char* GetName()   override { return "Scroll"; }
    bool        IsEnabled() override { return Scroll::enabled; }
    const char* GetSuffix() override { return ""; }
    void Run(JNIEnv* env)   override {
        if (Scroll::triggerRequested.exchange(false))
            Scroll::Trigger(env);
    }
};