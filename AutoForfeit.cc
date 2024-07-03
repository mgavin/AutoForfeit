/*
 *
 *
 * NEED TO CLEAN UP GIT COMMIT HISTORY!
 *
 * 1. clean up the cases for the initiator vs your ability to vote
 * 2. put in comparators for differnt fields.
 * 3. clean up git history and commit.
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

BAKKESMOD_PLUGIN(AutoForfeit, "AutoForfeit", "1.0.0", /*UNUSED*/ NULL);

/// <summary>
/// do the following when your plugin is loaded
/// </summary>
void AutoForfeit::onLoad() {
      // initialize things
      HookedEvents::gameWrapper = gameWrapper;

      // set up logging necessities
      log::set_cvarmanager(cvarManager);
      log::set_loglevel(log::LOGLEVEL::DEBUG);

      CVarManager::instance().set_cvar_prefix("aff_");
      CVarManager::instance().set_cvarmanager(cvarManager);

      cvar_storage = std::make_unique<PersistentManagedCVarStorage>(this, "autoff_cvars", false, true);

      init_cvars();

      if (plugin_enabled) {
            init_hooked_events();
      }
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
      cvarManager->registerNotifier(CVarManager::instance().get_cvar_prefix() + cmd_name, do_func, desc, NULL);
}

/// <summary>
/// group together the initialization of cvars
/// </summary>
void AutoForfeit::init_cvars() {
      CVarManager::instance().register_cvars();

      CVarManager::instance().get_cvar_enabled().addOnValueChanged([this](std::string oldValue, CVarWrapper newValue) {
            plugin_enabled = newValue.getBoolValue();
            plugin_enabled ? enable_plugin() : disable_plugin();
      });
      CVarManager::instance().get_cvar_autoff_tm8().addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_tm8 = newValue.getBoolValue(); });
      CVarManager::instance().get_cvar_autoff_tm8_timeout().addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_tm8_timeout = newValue.getIntValue(); });
      CVarManager::instance().get_cvar_autoff_match().addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_match = newValue.getBoolValue(); });
      CVarManager::instance().get_cvar_autoff_match_time().addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_match_time = newValue.getIntValue(); });
      CVarManager::instance().get_cvar_party_disable().addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { party_disabled = newValue.getBoolValue(); });
      CVarManager::instance().get_cvar_autoff_my_goals().addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_my_goals = newValue.getBoolValue(); });
      CVarManager::instance().get_cvar_autoff_my_goals_num().addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_my_goals_num = newValue.getIntValue(); });
      CVarManager::instance().get_cvar_autoff_other_goals().addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_other_goals = newValue.getBoolValue(); });
      CVarManager::instance().get_cvar_autoff_other_goals_num().addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_other_goals_num = newValue.getIntValue(); });
      CVarManager::instance().get_cvar_autoff_diff_goals().addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_diff_goals = newValue.getBoolValue(); });
      CVarManager::instance().get_cvar_autoff_diff_goals_num().addOnValueChanged(
            [this](std::string oldValue, CVarWrapper newValue) { autoff_diff_goals_num = newValue.getIntValue(); });

#define X(name, ...) cvar_storage->AddCVar(CVarManager::instance().get_cvar_prefix() + #name);
      LIST_OF_PLUGIN_CVARS
#undef X

      // cvars for enabled playlists
      for (const auto & playlist_pair : bm_helper::playlist_ids_str_spaced) {
            if (no_replay_playlists.contains(playlist_pair.first)) {
                  continue;
            }

            std::string gamemode_str = playlist_pair.second | std::views::filter([](const char c) { return c != ' '; })
                                       | std::views::transform([](unsigned char c) { return std::tolower(c); })
                                       | std::ranges::to<std::string>();

            // unmanaged cvars... the wild west.
            std::string cvar_name = CVarManager::instance().get_cvar_prefix() + "autoff_" + gamemode_str;
            cvs.emplace(std::make_pair(
                  playlist_pair.first,
                  cvarManager->registerCvar(cvar_name, "0", "auto forfeit " + gamemode_str + " game mode", false)));

            cvs.at(playlist_pair.first).addOnValueChanged([this, playlist_pair](std::string, CVarWrapper cvar) {
                  plist_enabled[playlist_pair.first] = cvar.getBoolValue();
            });

            cvar_storage->AddCVar(cvar_name);
      }
}

/// <summary>
/// group together the initialization of hooked events
/// </summary>
void AutoForfeit::init_hooked_events() {
      HookedEvents::AddHookedEvent("Function TAGame.GameEvent_TA.OnCanVoteForfeitChanged", [this](...) {
            LOG(log::LOGLEVEL::DEBUG, "MADE IT TO ON CAN VOTE FORFEIT CHANGED!");
            // good opportunity to get the team the player is on
            ServerWrapper sw = gameWrapper->GetCurrentGameState();
            if (!sw) {
                  LOG(log::LOGLEVEL::ERROR, "CANT GET CURRENT SERVER?");
                  return;
            }

            if (!sw.GetbCanVoteToForfeit()) {
                  // Can't forfeit no mo'
                  unhook_forfeit_conditions();
                  return;
            } else {
                  // You're allowed to forfeit
                  // I'm happy that bakkesmod makes sure to keep track of this
                  // because trying to do it myself was tedious.
                  hook_forfeit_conditions();
            }

            PlayerControllerWrapper pcw = sw.GetLocalPrimaryPlayer();

            if (!pcw) {
                  LOG(log::LOGLEVEL::ERROR, "CANT GET LOCAL PRIMARY PLAYER?");
                  return;
            }

            std::unique_ptr<PriWrapper> priw = std::make_unique<PriWrapper>(pcw.GetPRI());

            if (!priw) {
                  LOG(log::LOGLEVEL::ERROR, "CANT GET PLAYER PRI?");
                  return;
            }

            which_team_am_i = priw->GetTeamNum();
            p.swap(priw);
      });

      // general helper hooks.
      // BECAUSE PriWrapper::GetPartyLeaderID() MAY NOT WORK OUTSIDE OF A SERVER?
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

      HookedEvents::AddHookedEvent("Function Engine.GameInfo.PreExit", [this](std::string eventName) {
            // ASSURED CLEANUP
            onUnload();
      });
}

/**
 * \brief Hook the relevant functions when able to forfeit.
 */
void AutoForfeit::hook_forfeit_conditions() {
      log::LOG(log::LOGLEVEL::DEBUG, "forfeit conditions hooked");

      HookedEvents::AddHookedEvent("Function TAGame.GameEvent_Soccar_TA.EventGameTimeUpdated", [this](...) {
            ServerWrapper sw = gameWrapper->GetCurrentGameState();
            if (!sw) {
                  return;
            }

            // sw.GetSecondsElapsed();  // TIME SPENT (float) IN MATCH SO FAR

            // IF sw.GetbOverTime(); == 1, THIS IS TIME SPENT IN OVERTIME
            int  game_time_left = sw.GetSecondsRemaining();  // TIME REMAINING.
            bool in_overtime    = sw.GetbOverTime();
            if (in_overtime) {
                  game_time_left *= -1;
            }

            if (autoff_match && autoff_match_time >= game_time_left) {
                  log::LOG(
                        log::LOGLEVEL::DEBUG,
                        "FULFILLED CONDITIONS TO FORFEIT FROM GAME TIME: autoff_match: {}, autoff_match_time: {}, "
                        "in_overtime: {}, game_time_left: {}={}:{:02d}",
                        autoff_match,
                        autoff_match_time,
                        in_overtime,
                        game_time_left,
                        game_time_left / 60,
                        game_time_left % 60);
                  forfeit_func();
            }
      });

      HookedEvents::AddHookedEvent("Function TAGame.Team_TA.EventScoreUpdated", [this](...) {
            ServerWrapper sw = gameWrapper->GetCurrentGameState();
            if (sw) {
                  ArrayWrapper<TeamWrapper> awtw = sw.GetTeams();
                  if (!awtw.IsNull()) {
                        if (awtw.Count() < 2) {
                              return;
                        }

                        int other_team       = which_team_am_i ? 0 : 1;
                        int my_team_score    = sw.GetTeams().Get(which_team_am_i).GetScore();
                        int other_team_score = sw.GetTeams().Get(other_team).GetScore();
                        log::LOG(
                              log::LOGLEVEL::INFO,
                              "my team is {}. score : {}, other team's score : {} ",
                              which_team_am_i,
                              my_team_score,
                              other_team_score);

                        if ((autoff_my_goals && my_team_score >= autoff_my_goals_num)
                            || (autoff_other_goals && other_team_score >= autoff_other_goals_num)
                            || (autoff_diff_goals
                                && (autoff_diff_goals_num < 0
                                          ? ((my_team_score - other_team_score) <= autoff_diff_goals_num)
                                          : ((my_team_score - other_team_score) >= autoff_diff_goals_num)))) {
                              log::LOG(
                                    log::LOGLEVEL::DEBUG,
                                    "FULFILLED CONDITIONS TO FORFEIT FROM GOALS: autoff_my_goals: {}, my_team_score: "
                                    "{}, autoff_my_goals_num: {}, autoff_other_goals: {}, other_team_score: {}, "
                                    "autoff_other_goals_num: {}, autoff_diff_goals: {}, autoff_diff_goals_num: {}, "
                                    "diff_score: {}",
                                    autoff_my_goals,
                                    my_team_score,
                                    autoff_my_goals_num,
                                    autoff_other_goals,
                                    other_team_score,
                                    autoff_other_goals_num,
                                    autoff_diff_goals,
                                    autoff_diff_goals_num,
                                    abs(my_team_score - other_team_score));
                              forfeit_func();
                        }
                  }
            }
      });

      HookedEvents::AddHookedEvent("Function TAGame.VoteActor_TA.EventStarted", [this](...) {
            LOG(log::LOGLEVEL::DEBUG, "MADE IT TO VOTE ACTOR EVENT STARTED!");
            // SOMEONE ON YOUR TEAM STARTED A VOTE (to forfeit) MIGHT BE YOU!
            in_ff_vote = true;
            if (autoff_tm8 && autoff_tm8_timeout == 0) {
                  log::LOG(log::LOGLEVEL::DEBUG, "FULFILLED CONDITIONS TO FORFEIT FROM TEAMMATE");
                  forfeit_func();
            }
      });

      HookedEvents::AddHookedEvent("Function TAGame.VoteActor_TA.EventFinished", [this](...) {
            LOG(log::LOGLEVEL::DEBUG, "MADE IT TO VOTE ACTOR EVENT ENDED!");
            // THE VOTE IS OVER.
            in_ff_vote         = false;
            vote_started_timer = CVarManager::instance().get_cvar_autoff_tm8_timeout().getIntValue();
      });

      HookedEvents::AddHookedEvent("Function TAGame.VoteActor_TA.UpdateTimeRemaining", [this](...) {
            LOG(log::LOGLEVEL::DEBUG, "MADE IT TO VOTE ACTOR UPDATE TIME REMAINING!");
            if (autoff_tm8 && vote_started_timer == 0) {
                  log::LOG(log::LOGLEVEL::DEBUG, "FULFILLED CONDITIONS TO FORFEIT FROM TEAMMATE");
                  forfeit_func();
            }
            vote_started_timer--;
      });

      HookedEvents::AddHookedEvent("Function TAGame.PRI_TA.ServerVoteToForfeit", [this](...) {
            LOG(log::LOGLEVEL::DEBUG, "MADE IT TO PRI SERVER VOTE TO FORFEIT!");
      });
}

/**
 * \brief Unhook the relevant functions when unable to forfeit.
 */
void AutoForfeit::unhook_forfeit_conditions() {
      log::LOG(log::LOGLEVEL::DEBUG, "forfeit conditions unhooked");

      std::vector<std::string_view> forfeit_hooks {
            "Function TAGame.GameEvent_Soccar_TA.EventGameTimeUpdated",
            "Function TAGame.Team_TA.EventScoreUpdated",
            "Function TAGame.VoteActor_TA.EventStarted",
            "Function TAGame.VoteActor_TA.EventFinished",
            "Function TAGame.VoteActor_TA.UpdateTimeRemaining",
            "Function TAGame.PRI_TA.ServerVoteToForfeit"};

      for (const auto & h : forfeit_hooks) {
            HookedEvents::RemoveHook(h.data());
      }
}

/**
 * \brief Hook the relevant functions when the plugin is enabled.
 */
void AutoForfeit::enable_plugin() {
      log::LOG(log::LOGLEVEL::DEBUG, "plugin_enabled");
      plugin_enabled = true;
      clear_flags();
      init_hooked_events();
}

/**
 * \brief Unhook the relevant functions when the plugin is disabled.
 */
void AutoForfeit::disable_plugin() {
      log::LOG(log::LOGLEVEL::DEBUG, "plugin_disabled");
      plugin_enabled = false;
      clear_flags();
      HookedEvents::RemoveAllHooks();
}

/**
 * \brief Called to check if forfeitting should be done
 *
 * \return True if forfeitting is allowed. False otherwise.
 */
bool AutoForfeit::can_forfeit() {
      if (!plugin_enabled) {
            LOG(log::LOGLEVEL::DEBUG, "PLUGIN NOT ENABLED");
            return false;
      }

      if (!gameWrapper->IsInOnlineGame()) {
            log::LOG(log::LOGLEVEL::DEBUG, "NOT IN ONLINE GAME");
            return false;
      }

      if (p->GetbStartVoteToForfeitDisabled() && !someone_else_ff) {
            LOG(log::LOGLEVEL::DEBUG, "NOT ALLOWED TO FORFEIT");
            return false;
      }

      if (in_party && party_disabled) {
            LOG(log::LOGLEVEL::DEBUG, "DISABLED WHILE IN PARTY");
            return false;
      }

      // get the playlistid of the current match
      PlaylistId    playid = PlaylistId::Unknown;
      ServerWrapper sw     = gameWrapper->GetCurrentGameState();

      if (!sw) {
            // not in server
            LOG(log::LOGLEVEL::ERROR, "CANT GET PLAYLIST ID TO DETERMINE FORFEIT-ABILITY (no serverwrapper)");
            return false;
      }

      GameSettingPlaylistWrapper gspw = sw.GetPlaylist();

      if (!gspw) {
            // 🤷‍♂️
            LOG(log::LOGLEVEL::ERROR,
                "CANT GET PLAYLIST ID TO DETERMINE FORFEIT-ABILITY (no gamesettingsplaylistwrapper)");
            return false;
      }

      playid = static_cast<PlaylistId>(gspw.GetPlaylistId());

      if (!plist_enabled[playid]) {
            LOG(log::LOGLEVEL::DEBUG, "PLAYLIST NOT ENABLED");
            return false;
      }

      return true;
}

/**
 * \brief Calling this calls a vote to forfeit in game for the current player.
 */
void AutoForfeit::forfeit_func() {
      using log::LOGLEVEL::DEBUG;

      if (!can_forfeit()) {
            return;
      }

      ServerWrapper sw = gameWrapper->GetCurrentGameState();
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
}

/**
 * \brief If I needed to clear any flags, they would be put here.
 *
 */
void AutoForfeit::clear_flags() {
      someone_else_ff = false;
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
                  wmemset(URL, 0, static_cast<size_t>(nchar) + 1);  //...
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
                  CVarWrapper cv = CVarManager::instance().get_cvar_enabled();
                  if (cv) {
                        cv.setValue(plugin_enabled);
                  }
            });
      }

      ImGui::SameLine(0.0f, 50.0f);

      if (ImGui::Checkbox("Disable while in a party?", &party_disabled)) {
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = CVarManager::instance().get_cvar_party_disable();
                  if (cv) {
                        cv.setValue(party_disabled);
                  }
            });
      }

      // ImGui::NewLine();

      if (ImGui::Checkbox("Auto-forfeit when teammate forfeits?", &autoff_tm8)) {
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = CVarManager::instance().get_cvar_autoff_tm8();
                  if (cv) {
                        cv.setValue(autoff_tm8);
                  }
            });
      }

      ImGui::SameLine(0.0f, 50.0f);

      ImGui::SetNextItemWidth(200.0f);
      if (ImGui::SliderInt("Wait how long after teammate's vote?", &autoff_tm8_timeout, 0, 19, "%d seconds")) {
            autoff_tm8_timeout = std::clamp(autoff_tm8_timeout, 0, 19);
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = CVarManager::instance().get_cvar_autoff_tm8_timeout();
                  if (cv) {
                        cv.setValue(autoff_tm8_timeout);
                  }
            });
      }

      if (ImGui::Checkbox("Auto-forfeit in a match?", &autoff_match)) {
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = CVarManager::instance().get_cvar_autoff_match();
                  if (cv) {
                        cv.setValue(autoff_match);
                  }
            });
      }

      ImGui::SameLine(0.0f, 50.0f);
      ImGui::SetNextItemWidth(200.0f);
      // EXPLAINED IN [INFORMATION]
      // IF IT'S COMP GAME AND ABOVE 3:30, THEN IT'S APPLIED ASAP AT 3:30.
      // IF IT'S NEGATIVE, THAT MEANS OVERTIME
      static char buf[32] = {0};
      snprintf(
            buf,
            32,
            "%s %d:%02d",
            autoff_match_time < 0 ? "OVERTIME:" : "MATCH TIME:",
            abs(autoff_match_time) / 60,
            abs(autoff_match_time) % 60);
      if (ImGui::SliderInt(
                "Forfeit at what time in match? (seconds displayed as minutes, negative = overtime)",
                &autoff_match_time,
                240,
                -6001,
                buf)) {
            // from 4:00 minutes in match or -100:00 in overtime
            autoff_match_time = std::clamp(autoff_match_time, -6000, 240);
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = CVarManager::instance().get_cvar_autoff_match_time();
                  if (cv) {
                        cv.setValue(autoff_match_time);
                  }
            });
      }  // COULD YOU IMAGINE DOING THIS _PER_ PLAYLIST? LOL!

      if (ImGui::Checkbox("Forfeit after YOUR team scores X goals?", &autoff_my_goals)) {
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = CVarManager::instance().get_cvar_autoff_my_goals();
                  if (cv) {
                        cv.setValue(autoff_my_goals);
                  }
            });
      }

      ImGui::SameLine(0.0f, 50.0f);

      ImGui::SetNextItemWidth(200.0f);
      if (ImGui::SliderInt("# of goals for your team", &autoff_my_goals_num, 0, 100, "%d")) {
            autoff_my_goals_num = std::clamp(autoff_my_goals_num, 0, 100);
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = CVarManager::instance().get_cvar_autoff_my_goals_num();
                  if (cv) {
                        cv.setValue(autoff_my_goals_num);
                  }
            });
      }

      if (ImGui::Checkbox("Forfeit after OPPONENT team scores X goals?", &autoff_other_goals)) {
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = CVarManager::instance().get_cvar_autoff_other_goals();
                  if (cv) {
                        cv.setValue(autoff_other_goals);
                  }
            });
      }

      ImGui::SameLine(0.0f, 50.0f);

      ImGui::SetNextItemWidth(200.0f);
      if (ImGui::SliderInt("# of goals for opponent team", &autoff_other_goals_num, 0, 100, "%d")) {
            autoff_other_goals_num = std::clamp(autoff_other_goals_num, 0, 100);
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = CVarManager::instance().get_cvar_autoff_other_goals_num();
                  if (cv) {
                        cv.setValue(autoff_other_goals_num);
                  }
            });
      }

      if (ImGui::Checkbox("Forfeit after GOAL DIFFERENTIAL reaches this amount?", &autoff_diff_goals)) {
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = CVarManager::instance().get_cvar_autoff_diff_goals();
                  if (cv) {
                        cv.setValue(autoff_diff_goals);
                  }
            });
      }

      ImGui::SameLine(0.0f, 50.0f);

      ImGui::SetNextItemWidth(200.0f);
      if (ImGui::SliderInt(
                "# of goals (positive = up by that many, negative = down)",
                &autoff_diff_goals_num,
                -100,
                100,
                "%d")) {
            autoff_diff_goals_num = std::clamp(autoff_diff_goals_num, -100, 100);
            gameWrapper->Execute([this](...) {
                  CVarWrapper cv = CVarManager::instance().get_cvar_autoff_diff_goals_num();
                  if (cv) {
                        cv.setValue(autoff_diff_goals_num);
                  }
            });
      }

      ImGui::NewLine();

      ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.2f, 0.8f, 1.0f));
      if (ImGui::CollapsingHeader("Enable automatic forfeiting for certain playlists:")) {
            ImGui::TextUnformatted("Click a playlist to enable automatic forfeiting.");
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
