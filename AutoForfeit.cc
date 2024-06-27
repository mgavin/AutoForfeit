/*
 * Needs cleanup, then this'll be fine.
 *
 * I wish I had snacks :(
 *
 *
 *
 *
 *
 */

#include "AutoForfeit.h"

#include <mutex>

#include "Windows.h"  // IWYU pragma: keep

#include "shellapi.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "HookedEvents.h"
#include "Logger.h"

namespace {
#ifdef _WIN32
// ERROR macro is defined in Windows header
// To avoid conflict between these macro and declaration of ERROR / DEBUG in SEVERITY enum
// We save macro and undef it
#pragma push_macro("ERROR")
#pragma push_macro("DEBUG")
#undef ERROR
#undef DEBUG
#endif

namespace log = LOGGER;
};  // namespace

BAKKESMOD_PLUGIN(AutoForfeit, "AutoForfeit", "0.0.0", /*UNUSED*/ NULL);
std::shared_ptr<CVarManagerWrapper> _globalCVarManager;

/// <summary>
/// do the following when your plugin is loaded
/// </summary>
void AutoForfeit::onLoad() {
      // initialize things
      _globalCVarManager        = cvarManager;
      HookedEvents::gameWrapper = gameWrapper;

      log::set_loglevel(log::LOGLEVEL::INFO);

      init_cvars();
      init_hooked_events();
}

/// <summary>
/// HELPER FOR REGISTERING A (SETTER) NOTIFIER IN CVARMANAGER  *
/// </summary>
/// <param name="cmd_name"></param>
/// <param name="do_func"></param>
/// <param name="desc"></param>
void AutoForfeit::add_notifier(
      std::string                                   cmd_name,
      std::function<void(std::vector<std::string>)> do_func,
      std::string                                   desc) const {
      cvarManager->registerNotifier(cmd_prefix + cmd_name, do_func, desc, NULL);
}

/// <summary>
/// group together the initialization of cvars
///
/// I hate that I have to memoize cvar strings and then get no guarantee of if they're
/// valid until something goes wrong at runtime.
/// </summary>
void AutoForfeit::init_cvars() {
      CVarWrapper enabled_cv = cvarManager->registerCvar(
            cmd_prefix + "enabled",
            "1",
            "Governs whether the AutoForfeit BakkesMod plugin is enabled.",
            true);
      enabled_cv.addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { plugin_enabled = newValue.getBoolValue(); });

      CVarWrapper autoff_tm8_cv =
            cvarManager->registerCvar(cmd_prefix + "autoff_tm8", "0", "Forfeit whenever a teammate forfeits?", false);
      autoff_tm8_cv.addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_tm8 = newValue.getBoolValue(); });

      CVarWrapper autoff_tm8_timeout_cv = cvarManager->registerCvar(
            cmd_prefix + "autoff_tm8_timeout",
            "0",
            "How much time to wait until after tm8 forfeits to forfeit.",
            false,
            true,
            0,
            true,
            19);
      autoff_tm8_timeout_cv.addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_tm8_timeout = newValue.getIntValue(); });

      CVarWrapper autoff_match_cv =
            cvarManager->registerCvar(cmd_prefix + "autoff_match", "0", "Forfeit a match automatically?", false);
      autoff_match_cv.addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_match = newValue.getBoolValue(); });

      CVarWrapper autoff_match_time_cv = cvarManager->registerCvar(
            cmd_prefix + "autoff_match_time",
            "240",
            "At what time in match to forfeit.",
            false);
      autoff_match_time_cv.addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_match_time = newValue.getIntValue(); });

      CVarWrapper party_disable_cv = cvarManager->registerCvar(
            cmd_prefix + "party_disable",
            "1",
            "Should this be disabled while in a party?",
            false);
      party_disable_cv.addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { party_disable = newValue.getBoolValue(); });

      // cvars for enabled playlists
      for (auto playlist_pair : bm_helper::playlist_ids_str_spaced) {
            if (no_replay_playlists.contains(playlist_pair.first)) {
                  continue;
            }

            std::string gamemode_str = playlist_pair.second | std::views::filter([](const char c) { return c != ' '; })
                                       | std::views::transform([](unsigned char c) { return std::tolower(c); })
                                       | std::ranges::to<std::string>();

            cvs.emplace(std::make_pair(
                  playlist_pair.first,
                  cvarManager->registerCvar(
                        cmd_prefix + "autoff_" + gamemode_str,
                        "0",
                        "auto forfeit " + gamemode_str + " game mode",
                        false)));

            cvs.at(playlist_pair.first).addOnValueChanged([this, playlist_pair](std::string, CVarWrapper cvar) {
                  plist_enabled[playlist_pair.first] = cvar.getBoolValue();
            });
      }
}

/// <summary>
/// group together the initialization of hooked events
/// </summary>
void AutoForfeit::init_hooked_events() {
      // BECAUSE PriWrapper::GetPartyLeaderID() IS BROKEN.
      HookedEvents::AddHookedEventWithCaller<ActorWrapper>(
            "Function ProjectX.PartyMetrics_X.PartyChanged",
            [this](ActorWrapper caller, void * params, std::string eventName) {
                  bm_helper::PartyChangeParams party_changed_params =
                        *reinterpret_cast<bm_helper::PartyChangeParams *>(params);
                  if (party_changed_params.party_size > 1) {
                        in_party = true;
                  } else {
                        in_party = false;
                  }
            });

      HookedEvents::AddHookedEvent("Function TAGame.VoteActor_TA.EventStarted", [this](...) {
            gameWrapper->Execute([this](...) {
                  LOG(log::LOGLEVEL::INFO, "MADE IT TO VOTE ACTOR EVENT STARTED!");
                  // TEAMMATE FORFEITED
                  forfeit_func();
            });
      });

      HookedEvents::AddHookedEvent("Function TAGame.VoteActor_TA.UpdateTimeRemaining", [this](...) {
            if (vote_started_timer <= autoff_tm8_timeout) {
                  forfeit_func();
            }
            vote_started_timer--;
      });

      HookedEvents::AddHookedEvent("Function TAGame.GameEvent_TA.OnCanVoteForfeitChanged", [this](...) {
            LOG(log::LOGLEVEL::INFO, "MADE IT TO ON CAN VOTE FORFEIT CHANGED!");
            // "Function TAGame.VoteActor_TA.UpdateTimeRemaining"
            // Function TAGame.GameEvent_Soccar_TA.EventGameTimeUpdated
            // YOU CAN FORFEIT NOW!
            std::call_once(f, &AutoForfeit::forfeit_func, this);
      });

      HookedEvents::AddHookedEvent("Function TAGame.GameEvent_Soccar_TA.EventGameTimeUpdated", [this](...) {
            if (game_time <= autoff_match_time) {
                  forfeit_func();
            }
            game_time--;
      });

      // This function has been suggested before as a function that runs
      // while going back to the menu
      HookedEvents::AddHookedEvent(
            "Function TAGame.LoadingScreen_TA.HandlePreLoadMap",
            [this](std::string eventName) {
                  log::LOG(log::LOGLEVEL::INFO, "OUT OF A GAME? MAYBE? PRELOADMAP?");
                  vote_started_timer = autoff_tm8_timeout;
                  game_time          = 300;
            },
            true);

      HookedEvents::AddHookedEvent("Function Engine.GameInfo.PreExit", [this](std::string eventName) {
            // ASSURED CLEANUP
            onUnload();
      });
}

/**
 * \brief Called to check if forfeitting should be done
 *
 *
 * \return True if forfeitting is allowed. False otherwise.
 */
bool AutoForfeit::can_forfeit() {
      PlaylistId playid = PlaylistId::Unknown;
      gameWrapper->Execute([this, &playid](GameWrapper * gw) {
            ServerWrapper sw = gw->GetCurrentGameState();
            if (!sw) {
                  LOG(log::LOGLEVEL::ERROR, "CANT GET PLAYLIST ID TO DETERMINE FORFEIT-ABILITY (no serverwrapper)");
                  return;
            }
            GameSettingPlaylistWrapper gspw = sw.GetPlaylist();
            if (!gspw) {
                  LOG(log::LOGLEVEL::ERROR,
                      "CANT GET PLAYLIST ID TO DETERMINE FORFEIT-ABILITY (no gamesettingsplaylistwrapper)");
                  return;
            }

            playid = static_cast<PlaylistId>(gspw.GetPlaylistId());
      });

      if (plugin_enabled && plist_enabled[playid] && !(in_party && party_disable)) {
            return true;
      }
      return false;
}

/**
 * \brief Calling this calls a vote to forfeit in game for the current player.
 */
void AutoForfeit::forfeit_func() {
      using log::LOGLEVEL::DEBUG;

      if (!can_forfeit()) {
            LOG(DEBUG, "CAN'T EVEN FORFEIT!");
            return;
      }

      gameWrapper->Execute([this](GameWrapper * gw) {
            if (!gw->IsInOnlineGame()) {
                  log::LOG(DEBUG, "NOT IN ONLINE GAME");
                  return;
            }

            ServerWrapper sw = gw->GetCurrentGameState();
            if (!sw) {
                  log::LOG(DEBUG, "NO SERVER WRAPPER");
                  return;
            }

            PlayerControllerWrapper pcw = sw.GetLocalPrimaryPlayer();
            if (!pcw) {
                  log::LOG(DEBUG, "NO LOCAL CAR");
                  return;
            }

            PriWrapper pw = pcw.GetPRI();
            if (!pw) {
                  log::LOG(DEBUG, "NO PRI WRAPPER");
                  return;
            }
            pw.ServerVoteToForfeit();
      });
}

/// <summary>
/// https://mastodon.gamedev.place/@dougbinks/99009293355650878
/// </summary>
static inline void AddUnderline(ImColor col_) {
      ImVec2 min = ImGui::GetItemRectMin();
      ImVec2 max = ImGui::GetItemRectMax();
      min.y      = max.y;
      ImGui::GetWindowDrawList()->AddLine(min, max, col_, 1.0f);
}

/// <summary>
/// taken from https://gist.github.com/dougbinks/ef0962ef6ebe2cadae76c4e9f0586c69
/// "hyperlink urls"
/// </summary>
static inline void TextURL(  // NOLINT
      const char * name_,
      const char * URL_,
      uint8_t      SameLineBefore_,
      uint8_t      SameLineAfter_) {
      if (1 == SameLineBefore_) {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
      }
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 165, 255, 255));
      ImGui::Text("%s", name_);
      ImGui::PopStyleColor();
      if (ImGui::IsItemHovered()) {
            if (ImGui::IsMouseClicked(0)) {
                  // What if the URL length is greater than int but less than size_t?
                  // well then the program should crash, but this is fine.
                  const int nchar =
                        std::clamp(static_cast<int>(std::strlen(URL_)), 0, (std::numeric_limits<int>::max)() - 1);
                  wchar_t * URL = new wchar_t[nchar + 1];
                  wmemset(URL, 0, nchar + 1);
                  MultiByteToWideChar(CP_UTF8, 0, URL_, nchar, URL, nchar);
                  ShellExecuteW(NULL, L"open", URL, NULL, NULL, SW_SHOWNORMAL);

                  delete[] URL;
            }
            AddUnderline(ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
            ImGui::SetTooltip("  Open in browser\n%s", URL_);
      } else {
            AddUnderline(ImGui::GetStyle().Colors[ImGuiCol_Button]);
      }
      if (1 == SameLineAfter_) {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
      }
}

/// <summary>
/// This call usually includes ImGui code that is shown and rendered (repeatedly,
/// on every frame rendered) when your plugin is selected in the plugin
/// manager. AFAIK, if your plugin doesn't have an associated *.set file for its
/// settings, this will be used instead.
/// </summary>
void AutoForfeit::RenderSettings() {
      // for imgui plugin window
      if (ImGui::Checkbox("Enable Plugin", &plugin_enabled)) {
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = cvarManager->getCvar(cmd_prefix + "enabled");
                  if (cv) {
                        cv.setValue(plugin_enabled);
                  }
            });
      }

      ImGui::SameLine(0.0f, 50.0f);

      if (ImGui::Checkbox("Disable while in a party?", &party_disable)) {
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = cvarManager->getCvar(cmd_prefix + "party_disable");
                  if (cv) {
                        cv.setValue(party_disable);
                  }
            });
      }

      // ImGui::NewLine();

      if (ImGui::Checkbox("Auto-forfeit when teammate forfeits?", &autoff_tm8)) {
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = cvarManager->getCvar(cmd_prefix + "autoff_tm8");
                  if (cv) {
                        cv.setValue(autoff_tm8);
                  }
            });
      }

      ImGui::SameLine(0.0f, 50.0f);

      ImGui::SetNextItemWidth(200.0f);
      if (ImGui::SliderInt("Wait how long after teammate's vote? (seconds)", &autoff_tm8_timeout, 0, 19)) {
            autoff_tm8_timeout = std::clamp(autoff_tm8_timeout, 0, 19);
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = cvarManager->getCvar(cmd_prefix + "autoff_tm8_timeout");
                  if (cv) {
                        cv.setValue(autoff_tm8_timeout);
                  }
            });
      }

      // ImGui::NewLine();

      if (ImGui::Checkbox("Auto-forfeit in a match?", &autoff_match)) {
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = cvarManager->getCvar(cmd_prefix + "autoff_match");
                  if (cv) {
                        cv.setValue(autoff_match);
                  }
            });
      }

      ImGui::SameLine(0.0f, 50.0f);
      ImGui::SetNextItemWidth(200.0f);
      // EXPLAINED IN [INFORMATION]
      // EVEN THOUGH IT'S 4 MINUTES, IF YOU'RE IN A COMP GAME (can only wait 210 seconds (after 3:30 in match))
      // AND YOU SET IT TO ANYTHING GREATER, THIS WILL HAVE NO EFFECT!
      static char buf[32] = {0};
      snprintf(buf, 32, "%d:%02d", autoff_match_time / 60, autoff_match_time % 60);
      if (ImGui::SliderInt("Forfeit at what time in match?", &autoff_match_time, 240, -1, buf)) {
            autoff_match_time = std::clamp(autoff_match_time, 0, 240);
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = cvarManager->getCvar(cmd_prefix + "autoff_match_time");
                  if (cv) {
                        cv.setValue(autoff_match_time);
                  }
            });
      }

      ImGui::NewLine();

      ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.2f, 0.8f, 1.0f));
      if (ImGui::CollapsingHeader("Enable automatic forfeiting for certain playlists:")) {
            ImGui::TextUnformatted("Click a playlist to enable forfeiting.");
            ImGui::NewLine();
            ImGui::PushStyleColor(
                  ImGuiCol_Header,
                  ImVec4(
                        0.17f,
                        0.51f,
                        0.16f,
                        0.7f));  // the color the selectable becomes when selected.
            ImGui::BeginColumns(
                  "##playlistselectables",
                  static_cast<int>(std::size(SHOWN_PLAYLIST_CATEGORIES)),
                  ImGuiColumnsFlags_NoResize);
            size_t mxlines = 0;
            for (const std::string & header : SHOWN_PLAYLIST_CATEGORIES) {
                  ImGui::TextUnformatted(header.c_str());
                  AddUnderline(col_white);
                  ImGui::NextColumn();
                  mxlines = (std::max)(mxlines, bm_helper::playlist_categories[header].size());
            }
            for (int line = 0; line < mxlines; ++line) {
                  for (const std::string & category : SHOWN_PLAYLIST_CATEGORIES) {
                        if (line < bm_helper::playlist_categories[category].size()) {
                              PlaylistId playid = bm_helper::playlist_categories[category][line];
                              if (!no_replay_playlists.contains(playid)) {
                                    bool b = plist_enabled[playid];
                                    if (ImGui::Selectable(
                                              std::vformat(
                                                    "[{:c}] {}",
                                                    std::make_format_args(
                                                          b ? 'X' : ' ',
                                                          bm_helper::playlist_ids_str_spaced[playid]))
                                                    .c_str(),
                                              &plist_enabled[playid])) {
                                          cvs.at(playid).setValue(plist_enabled[playid]);
                                    }
                              }
                        };
                        ImGui::NextColumn();
                  }
            }
            ImGui::EndColumns();
            ImGui::PopStyleColor();
      }

      if (ImGui::CollapsingHeader("INFORMATION")) {
            // SOME TEXT EXPLAINING SOME QUESTIONS
            static const float INDENT_OFFSET = 40.0f;

            // Question 1
            ImGui::Indent(INDENT_OFFSET);

            ImGui::TextUnformatted("HOW DOES THIS WORK?");
            AddUnderline(col_white);

            ImGui::Unindent(INDENT_OFFSET);

            ImGui::TextUnformatted("乁( ◔ ౪◔)ㄏ");

            ImGui::NewLine();

            // Question 2
            ImGui::Indent(INDENT_OFFSET);

            ImGui::TextUnformatted("WHAT IF I CRASH OR HAVE A PROBLEM?");
            AddUnderline(col_white);
            ImGui::TextUnformatted("WHAT IF I HAVE A SUGGESTION?");
            AddUnderline(col_white);
            ImGui::TextUnformatted("WHAT IF I NEED TO INFORM YOU OF A CORRECTION?");
            AddUnderline(col_white);

            ImGui::Unindent(INDENT_OFFSET);

            ImGui::TextUnformatted("Raise an issue on the github page: ");
            TextURL("HERE", "https://github.com/mgavin/AutoForfeit/issues", true, false);

            ImGui::NewLine();

            ImGui::Indent(INDENT_OFFSET);

            ImGui::TextUnformatted("SPECIAL THANKS");
            AddUnderline(col_white);

            ImGui::Unindent(INDENT_OFFSET);

            ImGui::TextUnformatted(
                  "JediMaster2G -- for helping me test by saving "
                  "unsuspecting victims from enduring forfeit votes.");
      }
      ImGui::PopStyleColor();
}

/// <summary>
/// "SetImGuiContext happens when the plugin's ImGui is initialized."
/// https://wiki.bakkesplugins.com/imgui/custom_fonts/
///
/// also:
/// "Don't call this yourself, BM will call this function with a pointer
/// to the current ImGui context" -- pluginsettingswindow.h
/// ...
///
/// so ¯\(°_o)/¯
/// </summary>
/// <param name="ctx">AFAIK The pointer to the ImGui context</param>
void AutoForfeit::SetImGuiContext(uintptr_t ctx) {
      ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext *>(ctx));
}

/// <summary>
/// Get the name of the plugin for the plugins tab in bakkesmod
/// </summary>
std::string AutoForfeit::GetPluginName() {
      return "Auto Forfeit";
}

/*
 * for when you've inherited from BakkesMod::Plugin::PluginWindow.
 * this lets  you do "togglemenu (GetMenuName())" in BakkesMod's console...
 * ie
 * if the following GetMenuName() returns "xyz", then you can refer to your
 * plugin's window in game through "togglemenu xyz"
 */

/*
/// <summary>
/// do the following on togglemenu open
/// </summary>
void AutoForfeit::OnOpen() {};

/// <summary>
/// do the following on menu close
/// </summary>
void AutoForfeit::OnClose() {};

/// <summary>
/// (ImGui) Code called while rendering your menu window
/// </summary>
void AutoForfeit::Render() {};

/// <summary>
/// Returns the name of the menu to refer to it by
/// </summary>
/// <returns>The name used refered to by togglemenu</returns>
std::string AutoForfeit::GetMenuName() {
        return "$safeprojectname";
};

/// <summary>
/// Returns a std::string to show as the title
/// </summary>
/// <returns>The title of the menu</returns>
std::string AutoForfeit::GetMenuTitle() {
        return "AutoForfeit";
};

/// <summary>
/// Is it the active overlay(window)?
/// </summary>
/// <returns>True/False for being the active overlay</returns>
bool AutoForfeit::IsActiveOverlay() {
        return true;
};

/// <summary>
/// Should this block input from the rest of the program?
/// (aka RocketLeague and BakkesMod windows)
/// </summary>
/// <returns>True/False for if bakkesmod should block input</returns>
bool AutoForfeit::ShouldBlockInput() {
        return false;
};
*/

/// <summary>
///  do the following when your plugin is unloaded
///
///  destroy things here, don't throw
///  don't rely on this to assuredly run when RL is closed
/// </summary>
void AutoForfeit::onUnload() {
}

#ifdef _WIN32
// We restore the ERROR Windows macro
#pragma pop_macro("ERROR")
#pragma pop_macro("DEBUG")
#endif
