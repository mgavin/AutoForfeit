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
#define LIST_OF_PLUGIN_CVARS                                                                                          \
      X(enabled, "1", "Governs whether the AutoForfeit BakkesMod plugin is enabled.", true)                           \
      X(party_disable, "1", "Should this be disabled while in a party?", false)                                       \
      /* you-initiated grouping */                                                                                    \
      X(autoff_match, "0", "Forfeit a match automatically?", false)                                                   \
      X(autoff_match_time, "240", "At what time in match to forfeit.", false)                                         \
      X(autoff_my_goals, "0", "Auto-forfeit when my team hits X goals.", false)                                       \
      X(autoff_my_goals_num, "10", "Auto-forfeit when my team hits num goals.", false, true, 0, true, 100)            \
      X(autoff_other_goals, "0", "Auto-forfeit when other team hits X goals.", false)                                 \
      X(autoff_other_goals_num, "10", "Auto-forfeit when other team hits num goals.", false, true, 0, true, 100)      \
      X(autoff_diff_goals, "0", "Auto-forfeit when there's a goal differential.", false)                              \
      X(autoff_diff_goals_num,                                                                                        \
        "-3",                                                                                                         \
        "Auto-forfeit when the goal difference is this num.",                                                         \
        false,                                                                                                        \
        true,                                                                                                         \
        -100,                                                                                                         \
        true,                                                                                                         \
        100)                                                                                                          \
      /* tm8-initiated grouping */                                                                                    \
      X(autoff_tm8, "0", "Enable forfeiting when tm8 forfeits?", false)                                               \
      X(autoff_tm8_any, "0", "Forfeit when tm8 forfeits for any reason.", false)                                      \
      X(autoff_tm8_timeout,                                                                                           \
        "0",                                                                                                          \
        "How much time to wait until after tm8 forfeits to forfeit.",                                                 \
        false,                                                                                                        \
        true,                                                                                                         \
        0,                                                                                                            \
        true,                                                                                                         \
        19)                                                                                                           \
      X(autoff_tm8_match, "0", "Forfeit with a tm8 after a certain time?", false)                                     \
      X(autoff_tm8_match_time, "240", "At what time in match to forfeit with tm8.", false)                            \
      X(autoff_tm8_my_goals, "0", "Auto-forfeit when tm8 asks and my team hits X goals.", false)                      \
      X(autoff_tm8_my_goals_num,                                                                                      \
        "10",                                                                                                         \
        "Auto-forfeit when tm8 asks and my team hits num goals.",                                                     \
        false,                                                                                                        \
        true,                                                                                                         \
        0,                                                                                                            \
        true,                                                                                                         \
        100)                                                                                                          \
      X(autoff_tm8_other_goals, "0", "Auto-forfeit when tm8 asks and other team hits X goals.", false)                \
      X(autoff_tm8_other_goals_num,                                                                                   \
        "10",                                                                                                         \
        "Auto-forfeit when tm8 asks and other team hits num goals.",                                                  \
        false,                                                                                                        \
        true,                                                                                                         \
        0,                                                                                                            \
        true,                                                                                                         \
        100)                                                                                                          \
      X(autoff_tm8_diff_goals, "0", "Auto-forfeit when tm8 asks and there's a goal differential.", false)             \
      X(autoff_tm8_diff_goals_num,                                                                                    \
        "-3",                                                                                                         \
        "Auto-forfeit when the goal difference is this num.",                                                         \
        false,                                                                                                        \
        true,                                                                                                         \
        -100,                                                                                                         \
        true,                                                                                                         \
        100)                                                                                                          \
      X(autoff_match_time_comparator, "3", "Compare function to use for match time.", false, true, 0, true, 4);       \
      X(autoff_my_goals_comparator, "1", "Compare function to use for my goals.", false, true, 0, true, 4);           \
      X(autoff_other_goals_comparator, "1", "Compare function to use for oppo goals.", false, true, 0, true, 4);      \
      X(autoff_diff_goals_comparator, "3", "Compare function to use for diff goals.", false, true, 0, true, 4);       \
      X(autoff_tm8_match_time_comparator, "3", "Compare function for tm8 for match time.", false, true, 0, true, 4);  \
      X(autoff_tm8_my_goals_comparator, "1", "Compare function for tm8 for my goals.", false, true, 0, true, 4);      \
      X(autoff_tm8_other_goals_comparator, "1", "Compare function for tm8 for oppo goals.", false, true, 0, true, 4); \
      X(autoff_tm8_diff_goals_comparator, "3", "Compare function for tm8 for diff goals.", false, true, 0, true, 4);

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
                  tmp[x.first] = true;
            }
            return tmp;
      }();

      std::unique_ptr<PersistentManagedCVarStorage> cvar_storage;

      // helpful values

      // "you initiate" - grouping
      int autoff_match_time      = 240;
      int autoff_my_goals_num    = 10;
      int autoff_other_goals_num = 10;
      int autoff_diff_goals_num  = -3;

      // "after tm8" - grouping
      int autoff_tm8_timeout         = 0;
      int autoff_tm8_match_time      = 240;
      int autoff_tm8_my_goals_num    = 10;
      int autoff_tm8_other_goals_num = 10;
      int autoff_tm8_diff_goals_num  = -3;
      int vote_started_timer         = 0;

      int which_team_am_i = 0;

      // flags for different settings
      bool plugin_enabled = false;
      bool party_disabled = false;
      bool in_party       = false;
      bool ff_vote_added  = false;
      bool team_did_vote  = false;

      // "you initiate" - grouping
      bool autoff_match       = false;
      bool autoff_my_goals    = false;
      bool autoff_other_goals = false;
      bool autoff_diff_goals  = false;

      // "after tm8" - grouping
      bool autoff_tm8             = false;
      bool autoff_tm8_any         = false;
      bool autoff_tm8_match       = false;
      bool autoff_tm8_my_goals    = false;
      bool autoff_tm8_other_goals = false;
      bool autoff_tm8_diff_goals  = false;

      std::unique_ptr<PriWrapper> p;

      // comparators!
      template<typename T>
      bool comp(const char * op, T left, T right) {
            LOGGER::LOG("COMPARING {} AND {} WITH {}", left, right, op);
            bool rslt = false;
            if (std::strstr(op, "=")) {
                  LOGGER::LOG(LOGGER::LOGLEVEL::INFO, "IS EQUAL?");
                  rslt |= std::equal_to<T> {}(left, right);
            }
            if (std::strstr(op, ">")) {
                  LOGGER::LOG(LOGGER::LOGLEVEL::INFO, "IS GREATER?");
                  rslt |= std::greater<T> {}(left, right);
            }
            if (std::strstr(op, "<")) {
                  LOGGER::LOG(LOGGER::LOGLEVEL::INFO, "IS LESS?");
                  rslt |= std::less<T> {}(left, right);
            }
            return rslt;
      };

      // you-initiate
      const char * compares[5]            = {">", ">=", "=", "<=", "<"};
      const char * match_time_comparator  = compares[3];
      const char * my_goals_comparator    = compares[1];
      const char * other_goals_comparator = compares[1];
      const char * diff_goals_comparator  = compares[3];

      // tm8-initiate
      const char * tm8_match_time_comparator  = compares[3];
      const char * tm8_my_goals_comparator    = compares[1];
      const char * tm8_other_goals_comparator = compares[1];
      const char * tm8_diff_goals_comparator  = compares[3];

      // debug levels.. for the luls
      const char * debug_levels[5] = {"INFO", "DEBUG", "WARNING", "ERROR", "OFF"};
      const char * debug_level     = debug_levels[4];  // INFO=0, DEBUG=1, WARNING=2, ERROR=3, OFF=4}LOGGER::LOG_LEVEL

      // helper functions
      void init_cvars();
      void init_hooked_events();

      void hook_forfeit_conditions();
      void unhook_forfeit_conditions();

      void enable_plugin();
      void disable_plugin();

      bool can_forfeit();
      void forfeit_func();
      bool check_tm8_forfeit_conditions();

      void get_player_pri();

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
