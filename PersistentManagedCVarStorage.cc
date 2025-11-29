#include "PersistentManagedCVarStorage.h"

#include <fstream>

// #include "CVarManager.hpp"  // UNABLE TO WORK RIGHT NOW
#include "Logger.h"

namespace {
namespace log = LOGGER;
};  // namespace

PersistentManagedCVarStorage::PersistentManagedCVarStorage(
      BakkesMod::Plugin::BakkesModPlugin * plugin,
      const std::string &                  storage_file_name,
      const bool                           auto_write,
      bool                                 auto_load) :
      _cv(plugin->cvarManager),
      _storage_file(GetStorageFilePath(plugin->gameWrapper, storage_file_name)),
      _auto_write(auto_write)

{
      log::set_cvarmanager(_cv);
      // log::set_loglevel(log::LOGLEVEL::INFO);

      log::log_info("PersistentStorage: created and will store the data in {}", _storage_file.string());
      if (auto_load) {
            plugin->gameWrapper->SetTimeout([this](...) { Load(); }, 0.1f);
      }
}

PersistentManagedCVarStorage::CVarCacheItem::CVarCacheItem(CVarWrapper cvar) :
      value(cvar.getStringValue()), description(cvar.getDescription()) {
}

std::filesystem::path PersistentManagedCVarStorage::GetStorageFilePath(
      const std::shared_ptr<GameWrapper> & gw,
      std::string                          file_name) {
      return gw->GetBakkesModPath() / "cfg" / file_name.append(".cfg");
}

void PersistentManagedCVarStorage::OnPersistentCVarChanged(const std::string & old, CVarWrapper changed_cvar) {
      const auto cvar_name = changed_cvar.getCVarName();
      if (auto it = _cvar_cache.find(cvar_name); it != _cvar_cache.end()) { it->second = CVarCacheItem {changed_cvar}; }
      // If you write to file before the file has been loaded,
      // you will lose the data there.
      if (_auto_write && _loaded) { WritePersistentStorage(); }
}

void PersistentManagedCVarStorage::WritePersistentStorage() {
      std::ofstream out(_storage_file);
      log::log_info("PersistentStorage: Writing to file");
      for (const auto & [cvar, cvar_cache_item] : _cvar_cache) {
            out << std::format("{} ", cvar);
            out << std::quoted(cvar_cache_item.value);
            out << std::format(" //{}\n", cvar_cache_item.description);
      }
}

void PersistentManagedCVarStorage::Load() {
      if (!std::filesystem::exists(_storage_file)) {
            log::log_info("PersistentStorage: {} does not exist yet.", _storage_file.string());
      }
      // log::LOG(log::LOGLEVEL::INFO, "PersistentStorage: Loading the persistent storage cfg.");
      log::log_info("PersistentStorage: Loading the persistent storage cfg from {}.", _storage_file.string());
      _cv->loadCfg(_storage_file.string());
      _loaded = true;
}

void PersistentManagedCVarStorage::AddCVar(const std::string & s) {
      if (auto cvar = _cv->getCvar(s)) {
            _cvar_cache.insert_or_assign(s, CVarCacheItem {cvar});
            cvar.addOnValueChanged([this](auto old, auto new_cvar) { OnPersistentCVarChanged(old, new_cvar); });
      } else {
            log::log_warning(
                  "PersistentStorage: the cvar {} should be registered before "
                  "adding it to persistent storage",
                  s);
      }
}

void PersistentManagedCVarStorage::AddCVars(const std::initializer_list<std::string> cvars) {
      for (const auto & cvar_name : cvars) {
            AddCVar(cvar_name);
      }
}

PersistentManagedCVarStorage::~PersistentManagedCVarStorage() {
      WritePersistentStorage();
}
