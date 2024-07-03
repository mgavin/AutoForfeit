#ifndef __AUTOFORFEIT_H_
#define __AUTOFORFEIT_H_

#include <map>
#include <set>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginsettingswindow.h"

#include "imgui.h"

#include "bm_helper.h"
// #include "imgui_helper.h"
#include "PersistentManagedCVarStorage.h"

// registerCvar([req] name,[req] default_value,[req] description, searchable, has_min, min, has_max, max, save_to_cfg)
#define LIST_OF_PLUGIN_CVARS                                                                                    \
      X(enabled, "1", "Governs whether the AutoForfeit BakkesMod plugin is enabled.", true)                     \
      X(autoff_tm8, "0", "Forfeit whenever a teammate forfeits?", false)                                        \
      X(autoff_tm8_timeout,                                                                                     \
        "0",                                                                                                    \
        "How much time to wait until after tm8 forfeits to forfeit.",                                           \
        false,                                                                                                  \
        true,                                                                                                   \
        0,                                                                                                      \
        true,                                                                                                   \
        19)                                                                                                     \
      X(autoff_match, "0", "Forfeit a match automatically?", false)                                             \
      X(autoff_match_time, "240", "At what time in match to forfeit.", false)                                   \
      X(party_disable, "1", "Should this be disabled while in a party?", false)                                 \
      X(autoff_my_goals, "1", "Auto-forfeit when my team hits X goals.", false)                                 \
      X(autoff_my_goals_num, "1", "Auto-forfeit when my team hits num goals.", false, true, 0, true, 100)       \
      X(autoff_other_goals, "1", "Auto-forfeit when other team hits X goals.", false)                           \
      X(autoff_other_goals_num, "1", "Auto-forfeit when other team hits num goals.", false, true, 0, true, 100) \
      X(autoff_diff_goals, "1", "Auto-forfeit when there's a goal differential.", false)                        \
      X(autoff_diff_goals_num, "1", "Auto-forfeit when the goal difference is this num.", false, true, -100, true, 100)

#include "CVarManager.h"

class AutoForfeit : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow {
private:
      const ImColor col_white = ImColor {
            ImVec4 {1.0f, 1.0f, 1.0f, 1.0f}
      };
      const std::vector<std::string> SHOWN_PLAYLIST_CATEGORIES = {"Casual", "Competitive", "Tournament"};
      const std::set<PlaylistId>     no_replay_playlists       = {
            PlaylistId::Unknown,
            PlaylistId::Casual,

            // Training
            PlaylistId::Training,
            PlaylistId::UGCTrainingEditor,
            PlaylistId::UGCTraining,

            // Offline
            PlaylistId::OfflineSplitscreen,
            PlaylistId::Season,
            PlaylistId::Workshop,

            // Private Match
            PlaylistId::PrivateMatch,
            PlaylistId::LANMatch};

      // saved cvars so I don't have to look them up over and over
      std::map<PlaylistId, CVarWrapper> cvs;

      std::map<PlaylistId, bool> plist_enabled = [this] {
            std::map<PlaylistId, bool> tmp;
            for (const auto & x : bm_helper::playlist_ids_str) {
                  if (no_replay_playlists.contains(x.first)) {
                        continue;
                  }
                  tmp[x.first] = false;
            }
            return tmp;
      }();

      std::unique_ptr<PersistentManagedCVarStorage> cvar_storage;

      // helpful values
      int autoff_tm8_timeout     = 0;
      int autoff_match_time      = 240;
      int autoff_my_goals_num    = 0;
      int autoff_other_goals_num = 0;
      int autoff_diff_goals_num  = -3;
      int vote_started_timer     = 0;
      int which_team_am_i        = 0;

      // flags for different settings
      bool plugin_enabled     = false;
      bool party_disabled     = false;
      bool autoff_tm8         = false;
      bool autoff_match       = false;
      bool autoff_my_goals    = false;
      bool autoff_other_goals = false;
      bool autoff_diff_goals  = false;
      bool in_party           = false;
      bool in_ff_vote         = false;

      std::unique_ptr<PriWrapper> p;

      // helper functions
      void init_cvars();
      void init_hooked_events();

      void hook_forfeit_conditions();
      void unhook_forfeit_conditions();

      void enable_plugin();
      void disable_plugin();

      bool can_forfeit();
      void forfeit_func();

      void clear_flags();

      void add_notifier(std::string cmd_name, std::function<void(std::vector<std::string>)> do_func, std::string desc)
            const;

public:
      // honestly, for the sake of inheritance,
      // the member access in this class doesn't matter.
      // (since it's not expected to be inherited from)

      void onLoad() override;
      void onUnload() override;

      void        RenderSettings() override;
      std::string GetPluginName() override;
      void        SetImGuiContext(uintptr_t ctx) override;

      //
      // inherit from
      //				public BakkesMod::Plugin::PluginWindow
      //	 for the following
      // void        OnOpen() override;
      // void        OnClose() override;
      // void        Render() override;
      // std::string GetMenuName() override;
      // std::string GetMenuTitle() override;
      // bool        IsActiveOverlay() override;
      // bool        ShouldBlockInput() override;
};

#endif
