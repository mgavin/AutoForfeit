/*
 * plug: https://github.com/Martinii89/OrganizeMyGarageOpenSource/blob/master/OrganizeMyGarageV2/CVarManagerSingleton.h
 */

#ifndef LIST_OF_PLUGIN_CVARS
// I don't think I would care for people to use this without managed cvars.
// holy shit, imagining a "ManagedCVarWrapper" class... FUCK
// ... time just needs to be spent on the basics sometimes :(
#error "Need a list of plugin CVar(s) first before this can be used!"
#else

#ifndef __CVARMANAGER_H__
#define __CVARMANAGER_H__

#include <mutex>

// https://www.scs.stanford.edu/~dm/blog/va-opt.html
#define PARENS       ()
#define EXPAND(...)  EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_EACH(macro, ...)            __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(, ) __VA_OPT__(FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))
#define FOR_EACH_AGAIN()                FOR_EACH_HELPER

#include "bakkesmod/wrappers/cvarmanagerwrapper.h"
#include "bakkesmod/wrappers/cvarwrapper.h"

#include "cmap.hpp"

using cmap::make_lookup;
using cmap::map;

class CVarManager {
private:
      static inline constexpr auto lookup = make_lookup(
#define Y(a)         a
#define Z(...)       Y(__VA_ARGS__)
#define X(name, ...) map(std::string_view(#name), 1),
            FOR_EACH(Z, LIST_OF_PLUGIN_CVARS)
#undef X

      );

      std::once_flag                      sngl_f;
      std::string                         _prefix;
      std::shared_ptr<CVarManagerWrapper> _cvarManager;

public:
      static CVarManager & instance() {
            static CVarManager instance;
            return instance;
      }

      void set_cvarmanager(std::shared_ptr<CVarManagerWrapper> cvar) {
            std::call_once(
                  sngl_f,
                  [this](std::shared_ptr<CVarManagerWrapper> && cvarInternal) {
                        _cvarManager = std::move(cvarInternal);
                  },
                  std::move(cvar));
      }

      void        set_cvar_prefix(const std::string & p) { _prefix = p; }
      void        set_cvar_prefix(std::string && p) { _prefix = p; }
      std::string get_cvar_prefix() { return _prefix; }

      ~CVarManager() = default;
      CVarManager() {
      // registerCvar([req] name,[req] default_value,[req] description, searchable, has_min, min, has_max, max,
      // save_to_cfg)
#define X(name, default_value, description, searchable, ...) \
      _cvarManager->registerCvar(_prefix + #name, default_value, description, searchable __VA_OPT__(, ) __VA_ARGS__);
            LIST_OF_PLUGIN_CVARS
#undef X
      }

#define X(name, ...)                                                 \
      CVarWrapper get_cvar_##name() {                                \
            lookup[#name];                                           \
            return _cvarManager->getCvar(get_cvar_prefix() + #name); \
      }
      LIST_OF_PLUGIN_CVARS
#undef X
};

#endif
#endif
