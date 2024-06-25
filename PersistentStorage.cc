#include "PersistentStorage.h"

#include <fstream>

#include "Logger.h"

namespace {
namespace log = LOGGER;
};  // namespace

PersistentCVarStorage::PersistentCVarStorage(
      BakkesMod::Plugin::BakkesModPlugin * plugin,
      const std::string &                  storage_file_name,
      const bool                           auto_write,
      bool                                 auto_load) :
      cv_(plugin->cvarManager),
      storage_file_(GetStorageFilePath(plugin->gameWrapper, storage_file_name)),
      auto_write_(auto_write)

{
      log::LOG(
            log::LOGLEVEL::INFO,
            "PersistentStorage: created and will store the data in {}",
            storage_file_.string());
      cv_->registerNotifier(
            "writeconfig",
            [this](...) { WritePersistentStorage(); },
            "",
            0);

      if (auto_load) {
            plugin->gameWrapper->SetTimeout([this](...) { Load(); }, 0.1f);
      }
}

PersistentCVarStorage::~PersistentCVarStorage() {
      WritePersistentStorage();
}

void PersistentCVarStorage::WritePersistentStorage() {
      std::ofstream out(storage_file_);
      log::LOG(log::LOGLEVEL::INFO, "PersistentStorage: Writing to file");
      for (const auto & [cvar, cvar_cache_item] : cvar_cache_) {
            out << std::format(
                  "{} \"{}\" //{}\n",
                  cvar,
                  cvar_cache_item.value,
                  cvar_cache_item.description);
      }
}

void PersistentCVarStorage::Load() {
      log::LOG(
            log::LOGLEVEL::INFO,
            "PersistentStorage: Loading the persistent storage cfg");
      cv_->loadCfg(storage_file_.string());
      loaded_ = true;
}

CVarWrapper PersistentCVarStorage::RegisterPersistentCVar(
      const std::string & cvar,
      const std::string & defaultValue,
      const std::string & desc,
      const bool          searchAble,
      const bool          hasMin,
      const float         min,
      const bool          hasMax,
      const float         max,
      const bool          saveToCfg) {
      auto registered_cvar = cv_->registerCvar(
            cvar,
            defaultValue,
            desc,
            searchAble,
            hasMin,
            min,
            hasMax,
            max,
            saveToCfg);
      if (!registered_cvar) {
            log::LOG(
                  log::LOGLEVEL::ERROR,
                  "PersistentStorage: Failed to register the cvar {}",
                  cvar);
            return registered_cvar;
      }
      AddCVar(cvar);
      return registered_cvar;
}

void PersistentCVarStorage::AddCVar(const std::string & s) {
      if (auto cvar = cv_->getCvar(s)) {
            cvar_cache_.insert_or_assign(s, CVarCacheItem {cvar});
            cvar.addOnValueChanged([this](auto old, auto new_cvar) {
                  OnPersistentCVarChanged(old, new_cvar);
            });
      } else {
            log::LOG(
                  log::LOGLEVEL::WARNING,
                  "PersistentStorage: Warning the cvar {} should be registered before "
                  "adding it to persistent storage",
                  s);
      }
}

void PersistentCVarStorage::AddCVars(const std::initializer_list<std::string> cvars) {
      for (const auto & cvar_name : cvars) {
            AddCVar(cvar_name);
      }
}

std::filesystem::path PersistentCVarStorage::GetStorageFilePath(
      const std::shared_ptr<GameWrapper> & gw,
      std::string                          file_name) {
      return gw->GetBakkesModPath() / "cfg" / file_name.append(".cfg");
}

void PersistentCVarStorage::OnPersistentCVarChanged(
      const std::string & old,
      CVarWrapper         changed_cvar) {
      const auto cvar_name = changed_cvar.getCVarName();
      if (auto it = cvar_cache_.find(cvar_name); it != cvar_cache_.end()) {
            it->second = CVarCacheItem {changed_cvar};
      }
      // If you write to file before the file has been loaded,
      // you will lose the data there.
      if (auto_write_ && loaded_) {
            WritePersistentStorage();
      }
}

PersistentCVarStorage::CVarCacheItem::CVarCacheItem(CVarWrapper cvar) :
      value(cvar.getStringValue()), description(cvar.getDescription()) {
}
