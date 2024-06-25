#ifndef __CVARMANAGERSINGLETON_H__
#define __CVARMANAGERSINGLETON_H__

#include "../cmap/include/cmap.hpp"

using cmap::make_lookup;
using cmap::make_map;
using cmap::map;

#include <mutex>

#include "bakkesmod/wrappers/cvarmanagerwrapper.h"

class CVarManagerSingleton {
public:
      static CVarManagerSingleton & getInstance() {
            static CVarManagerSingleton instance;
            return instance;
      }

      void SetCVarManager(std::shared_ptr<CVarManagerWrapper> cvar) {
            std::call_once(
                  setCvarFlag_,
                  [this](std::shared_ptr<CVarManagerWrapper> && cvarInternal) {
                        cvarManager_ = std::move(cvarInternal);
                  },
                  std::move(cvar));
      }

      CVarManagerWrapper * getCvar() const { return cvarManager_.get(); }

private:
      std::shared_ptr<CVarManagerWrapper> cvarManager_;
      std::once_flag                      setCvarFlag_;

      CVarManagerSingleton()                                         = default;
      ~CVarManagerSingleton()                                        = default;
      CVarManagerSingleton(const CVarManagerSingleton &)             = delete;
      CVarManagerSingleton & operator=(const CVarManagerSingleton &) = delete;
};

#endif
