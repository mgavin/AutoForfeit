#ifndef __AUTOFORFEIT_H_
#define __AUTOFORFEIT_H_

#include <map>
#include <set>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginsettingswindow.h"

#include "imgui.h"

#include "bm_helper.h"
// #include "imgui_helper.h"

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

      // set a prefix to attach in front of all cvars
      // to avoid name clashes
      static const inline std::string cmd_prefix = "aff_";

      // timers
      int autoff_tm8_timeout = 0;
      int autoff_match_time  = 240;
      int game_time          = 300;
      int vote_started_timer = 0;

      // flags for different settings
      bool plugin_enabled = false;
      bool autoff_tm8     = false;
      bool autoff_match   = false;
      bool party_disable  = false;
      bool in_party       = false;

      // calling once flag
      std::once_flag f;

      // helper functions
      void init_cvars();
      void init_hooked_events();

      bool can_forfeit();
      void forfeit_func();

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
