#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

// #include "bm_helper.h"
// #include "imgui_helper.h"

class AutoForfeit : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow {
private:
      // data members

      // set a prefix to attach in front ofall cvars
      // to avoid name clashes
      static const inline std::string cmd_prefix = "aff_";

      // helper functions
      void init_cvars();
      void init_hooked_events();

      // flags for different points in the program
      bool plugin_enabled = false;

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
