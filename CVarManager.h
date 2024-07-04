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

#include <exception>
#include <mutex>

// https://www.scs.stanford.edu/~dm/blog/va-opt.html
// #define PARENS       ()
// #define EXPAND(...)  EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
// #define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
// #define EXPAND1(...) __VA_ARGS__
//
// #define FOR_EACH(macro, ...)            __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
// #define FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(, ) __VA_OPT__(FOR_EACH_AGAIN PARENS(macro,
// __VA_ARGS__)) #define FOR_EACH_AGAIN()                FOR_EACH_HELPER

#include "bakkesmod/wrappers/cvarmanagerwrapper.h"
#include "bakkesmod/wrappers/cvarwrapper.h"

// #include "cmap.hpp"
//
// using cmap::make_lookup;
// using cmap::map;

#include "Logger.h"

namespace {
namespace log = LOGGER;
}

class CVarManager {
private:
      // CMAP MADE "COMPILER OUT OF HEAP SPACE" HAPPEN, /Zm2000 or /MP- HAD NO EFFECT BIG FUCKING SAD!
      //      static inline constexpr auto lookup = make_lookup(
      // #define Y(a)         a
      // #define Z(...)       Y(__VA_ARGS__)
      // #define X(name, ...) map(#name, 1),
      //            FOR_EACH(Z, LIST_OF_PLUGIN_CVARS)
      // #undef X
      //
      //      );

      std::once_flag                      sngl_f;
      std::string                         _prefix;
      std::shared_ptr<CVarManagerWrapper> _cvarManager;

      std::shared_ptr<CVarManagerWrapper> getCVM() { return _cvarManager; }

public:
      static CVarManager & instance() {
            static CVarManager instance;
            return instance;
      }

      void register_cvars() {
            // registerCvar([req] name,[req] default_value,[req] description, searchable, has_min, min, has_max, max,
            // save_to_cfg)
            std::string cvar_name;
#define X(name, default_value, description, searchable, ...) \
      cvar_name = instance().get_cvar_prefix() + #name;      \
      instance().getCVM()->registerCvar(cvar_name, default_value, description, searchable __VA_OPT__(, ) __VA_ARGS__);
            LIST_OF_PLUGIN_CVARS
#undef X
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

      ~CVarManager()                               = default;
      CVarManager()                                = default;
      CVarManager(const CVarManager &)             = delete;
      CVarManager & operator=(const CVarManager &) = delete;

#define X(name, ...)                                                                                              \
      CVarWrapper get_cvar_##name() {                                                                             \
            using std::runtime_error;                                                                             \
            /* lookup[#name]; */                                                                                  \
            std::string cvar_name = instance().get_cvar_prefix() + #name;                                         \
            CVarWrapper cv        = instance().getCVM()->getCvar(cvar_name);                                      \
            if (!cv) {                                                                                            \
                  throw runtime_error {std::vformat("cvar {} doesn't exist.", std::make_format_args(cvar_name))}; \
            }                                                                                                     \
            return cv;                                                                                            \
      }
      LIST_OF_PLUGIN_CVARS
#undef X
};

#endif
#endif
