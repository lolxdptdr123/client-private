#pragma once
#include "Module.h"
#include <jni.h>
#include <string>
#include <unordered_map>

namespace MenuBinds {
    inline int  lc_bind = 0; inline bool lc_listening = false;
    inline int  aa_bind = 0; inline bool aa_listening = false;
    inline int  vel_bind = 0; inline bool vel_listening = false;
    inline int  ks_bind = 0; inline bool ks_listening = false;
    inline int  cr_bind = 0; inline bool cr_listening = false;
    inline int  sr_bind = 0; inline bool sr_listening = false;
    inline int  lr_bind = 0; inline bool lr_listening = false;
    inline int  reach_bind = 0; inline bool reach_listening = false;
    inline int  blink_bind = 0; inline bool blink_listening = false;
    inline int  rod_bind = 0; inline bool rod_listening = false;
    inline int  ab_bind = 0; inline bool ab_listening = false;
    inline int  ablock_bind = 0; inline bool ablock_listening = false;
    inline int  bt_bind = 0; inline bool bt_listening = false;
    inline int  aw_bind = 0; inline bool aw_listening = false;
    inline int  ar_bind = 0; inline bool ar_listening = false;
    inline int  al_bind = 0; inline bool al_listening = false;
    inline int  ch_bind = 0; inline bool ch_listening = false;
    inline int  esp_bind = 0; inline bool esp_listening = false;
    inline int  itemesp_bind = 0; inline bool itemesp_listening = false;
    inline int  pesp_bind = 0; inline bool pesp_listening = false;
    inline int  sesp_bind = 0; inline bool sesp_listening = false;
    inline int  besp_bind = 0; inline bool besp_listening = false;
    inline int  ntag_bind = 0; inline bool ntag_listening = false;
    inline int  tr_bind = 0; inline bool tr_listening = false;
    inline int  tj_bind = 0; inline bool tj_listening = false;
    inline int  fr_bind = 0; inline bool fr_listening = false;
    inline int  en_bind = 0; inline bool en_listening = false;
    inline int  nir_bind = 0; inline bool nir_listening = false;
    inline int  ad_bind = 0; inline bool ad_listening = false;
    inline int  pf_bind = 0; inline bool pf_listening = false;
    inline int  rc_bind = 0; inline bool rc_listening = false;
    inline int  bboost_bind = 0; inline bool bboost_listening = false;
    inline int  fp_bind = 0; inline bool fp_listening = false;
    inline int  fb_bind = 0; inline bool fb_listening = false;
    inline int  at_bind = 0; inline bool at_listening = false;
    inline int  cs_bind = 0; inline bool cs_listening = false;
    inline int  im_bind = 0; inline bool im_listening = false;
    inline int  ba_bind = 0; inline bool ba_listening = false;
    inline int  bi_bind = 0; inline bool bi_listening = false;
    inline int  clutch_bind = 0; inline bool clutch_listening = false;
    inline int  tl_bind = 0; inline bool tl_listening = false;
    inline bool tl_target_listening = false;
    inline int  iw_bind = 0; inline bool iw_listening = false;
    inline int  fs_bind = 0; inline bool fs_listening = false;
    inline int  njd_bind = 0; inline bool njd_listening = false;
    inline int  qa_bind = 0; inline bool qa_listening = false;
    inline int  st_bind = 0; inline bool st_listening = false;
    inline int  sp_bind = 0; inline bool sp_listening = false;
    inline int  ns_bind = 0; inline bool ns_listening = false;
    inline int  strf_bind = 0; inline bool strf_listening = false;
    inline int  notif_bind = 0; inline bool notif_listening = false;
    inline int  ptr_bind = 0; inline bool ptr_listening = false;
    inline int  ind_bind = 0; inline bool ind_listening = false;
    inline int  nhc_bind = 0; inline bool nhc_listening = false;
    inline int  destruct_bind = VK_END; inline bool destruct_listening = false;
    inline int  open_bind = VK_RSHIFT; inline bool open_listening = false;

    inline std::unordered_map<int*, bool> holdFlags;
    inline bool& Hold(int& key) { return holdFlags[&key]; }

    inline std::string VKToString(int vk) {
        if (vk == 0)           return "None";
        if (vk == VK_MBUTTON)  return "Mouse 3";
        if (vk == VK_XBUTTON1) return "Mouse 4";
        if (vk == VK_XBUTTON2) return "Mouse 5";
        if (vk == VK_F1)       return "F1";   if (vk == VK_F2)  return "F2";
        if (vk == VK_F3)       return "F3";   if (vk == VK_F4)  return "F4";
        if (vk == VK_F5)       return "F5";   if (vk == VK_F6)  return "F6";
        if (vk == VK_F7)       return "F7";   if (vk == VK_F8)  return "F8";
        if (vk == VK_F9)       return "F9";   if (vk == VK_F10) return "F10";
        if (vk == VK_F11)      return "F11";  if (vk == VK_F12) return "F12";
        if (vk == VK_INSERT)   return "Insert";
        if (vk == VK_DELETE)   return "Delete";
        if (vk == VK_HOME)     return "Home";
        if (vk == VK_END)      return "End";
        if (vk == VK_PRIOR)    return "PageUp";
        if (vk == VK_NEXT)     return "PageDown";
        if (vk == VK_LSHIFT)   return "LShift";
        if (vk == VK_RSHIFT)   return "RShift"; 
        if (vk == VK_RCONTROL) return "RCtrl";
        if (vk == VK_LCONTROL) return "LCtrl";
        if (vk >= 'A' && vk <= 'Z') return std::string(1, (char)vk);
        if (vk >= '0' && vk <= '9') return std::string(1, (char)vk);
        return "Key " + std::to_string(vk);
    }
}

namespace GuiSettings {
    inline float scale = 1.f;
    inline float accent[4] = { 0.32f, 0.62f, 0.98f, 1.00f };
    inline bool  allowInput = false;
    inline bool  compact = false;
    inline bool  wide = true;
    void Load();
    void Save();
}

class ClientMenu : public Module {
public:
    const char* GetName() override { return ""; }
    bool IsEnabled() override { return false; }
    virtual void OnImGuiRender(JNIEnv* env) override;

private:
    bool      m_prevToggleKey = false;
    bool      m_prevDestructKey = false;
    ULONGLONG m_bindReleasedAt = 0;
    int  m_activeTab = 0;

    std::unordered_map<std::string, bool> m_expanded;

    void RenderCombatTab();
    void RenderClickerTab();
    void RenderArrayListTab();
    void RenderChamsTab();
    void RenderEspTab();
    void RenderItemEspTab();
    void RenderPlayerEspTab();
    void RenderStorageEspTab();
    void RenderBlockEspTab();
    void RenderNametagTab();
    void RenderTracerTab();
    void RenderTrajectoriesTab();
    void RenderScrollTab();
    void RenderFastPlaceTab();
    void RenderFastBreakTab();
    void RenderAutoToolTab();
    void RenderChestStealerTab();
    void RenderInvManagerTab();
    void RenderTickLockerTab();
    void RenderInvWalkTab();
    void RenderFastStopTab();
    void RenderNoJumpDelayTab();
    void RenderKeepSprintTab();
    void RenderStrafeTab();
    void RenderQuickAccelTab();
    void RenderSnapTapTab();
    void RenderSprintTab();
    void RenderNoSlowTab();
    void RenderArmorTab();
    void RenderWeaponsTab();
    void RenderFriendsTab();
    void RenderEnemiesTab();
    void RenderNoItemReleaseTab();
    void RenderBlinkTab();
    void RenderAntiDebuffTab();
    void RenderPingFixTab();
    void RenderRightClickerTab();
    void RenderBowBoostTab();
    void RenderBridgeAssistTab();
    void RenderBlockInTab();
    void RenderClutchTab();
    void RenderNotificationsTab();
    void RenderPointersTab();
    void RenderIndicatorsTab();
    void RenderNoHurtCamTab();
    void RenderGuiModule();
    void RenderSettingsTab();
    void RenderUnloadTab();

    void DrawBindButton(const char* id, int& key, bool& listening);
    void SectionHeader(const char* label);
    bool ModuleHeader(const char* label, bool& enabled, int& bind, bool& listening, float W, const char* desc = nullptr);
    bool SnapCard(const char* name, const char* desc, bool* enabled, int* bind, bool* listening);
    bool ExpandButton(const char* label);
};