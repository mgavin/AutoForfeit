#ifndef __PERSISTENTMANAGEDCVARSTORAGE_H__
#define __PERSISTENTMANAGEDCVARSTORAGE_H__

#include <filesystem>
#include <map>

#include "bakkesmod/wrappers/cvarmanagerwrapper.h"
#include "bakkesmod/wrappers/cvarwrapper.h"
#include "bakkesmod/wrappers/gamewrapper.h"

class PersistentManagedCVarStorage {
private:
      std::shared_ptr<CVarManagerWrapper> _cv;
      std::string                         _prefix;
      std::filesystem::path               _storage_file {""};
      bool                                _auto_write = false;
      bool                                _loaded     = false;

      struct CVarCacheItem {
            std::string value;
            std::string description;
            explicit CVarCacheItem(CVarWrapper cvar);
      };

      std::map<std::string, CVarCacheItem> _cvar_cache;

      static std::filesystem::path GetStorageFilePath(const std::shared_ptr<GameWrapper> & gw, std::string file_name);

      void OnPersistentCVarChanged(const std::string & old, CVarWrapper changed_cvar);

public:
      /**
       * \brief
       * PersistentStorage is a convenient class for making sure your cvar values are
       * always loaded, and never lost if the main config is rewritten while your plugin
       * is unloaded.
       *
       * \param plugin Pointer to the main plugin
       * \param storage_file_name The file of the config file excluding the file extension
       * \param auto_write Automatically write cvar value to persistent storage once changed
       * \param auto_load Automatically load the cfg after 0.1s
       */
      explicit PersistentManagedCVarStorage(
            BakkesMod::Plugin::BakkesModPlugin * plugin,
            const std::string &                  prefix,
            const std::string &                  storage_file_name,
            bool                                 auto_write = false,
            bool                                 auto_load  = false);

      PersistentManagedCVarStorage(const PersistentManagedCVarStorage & other)                 = delete;
      PersistentManagedCVarStorage(PersistentManagedCVarStorage && other) noexcept             = delete;
      PersistentManagedCVarStorage & operator=(const PersistentManagedCVarStorage & other)     = delete;
      PersistentManagedCVarStorage & operator=(PersistentManagedCVarStorage && other) noexcept = delete;
      ~PersistentManagedCVarStorage();

      /**
       * \brief
       * Writes the cvar values to disk
       */
      void WritePersistentStorage();

      /**
       * \brief
       * Loads the cvar values from disk
       */
      void Load();

      /**
       * \brief
       * Add a single cvar to be persistent
       * \param s The cvar name
       */
      void AddCVar(const std::string & s);

      /**
       * \brief
       * Adds cvars to be persistent. The cvars have to be registered first.
       * \param cvars List of cvar names
       */
      void AddCVars(std::initializer_list<std::string> cvars);

      /**
       * \brief
       * Adds cvars to be persistent. The cvars have to be registered first.
       * \param cvars List of cvar names
       */
      template<class Iterable>
      void AddCVars(const Iterable cvars) {
            for (const auto & cvar_name : cvars) {
                  AddCVar(cvar_name);
            }
      }
};

#endif
