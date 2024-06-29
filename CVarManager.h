/*
 * TODO: Make a singleton for the class?
 *
 * I want to manage cvars here, and provide an interface to their existence
 *
 *
 */

#ifndef __CVARMANAGER_H__
#define __CVARMANAGER_H__

#ifndef LIST_OF_PLUGIN_CVARS
#error "Need a list of plugin CVar(s) first before this can be used!"
#endif

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

      std::string                         _prefix;
      std::shared_ptr<CVarManagerWrapper> _cvarManager;

public:
      CVarManager(std::string p, std::shared_ptr<CVarManagerWrapper> cvm) :
            _prefix(std::move(p)),
            _cvarManager(cvm) {
      // registerCvar([req] name,[req] default_value,[req] description, searchable, has_min, min, has_max, max,
      // save_to_cfg)
#define X(name, default_value, description, searchable, ...) \
      _cvarManager->registerCvar(_prefix + #name, default_value, description, searchable __VA_OPT__(, ) __VA_ARGS__);
                  LIST_OF_PLUGIN_CVARS
#undef X
            }

#define X(name, ...)                                                   \
      CVarWrapper getCVar_##name() {                                   \
            lookup[#name];                                             \
            return _cvarManager->getCvar(std::string("aff_") + #name); \
      }
            LIST_OF_PLUGIN_CVARS
#undef X
};

#endif
