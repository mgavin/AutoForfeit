#ifndef _HOOKEDEVENTS_H_
#define _HOOKEDEVENTS_H_

#include <regex>
#include <string>
#include <unordered_set>

#include "bakkesmod/wrappers/gamewrapper.h"

#include "Logger.h"

namespace {
namespace log = LOGGER;
}

class HookedEvents {
private:
      struct HookedEvent {
            std::string eventName;
            bool        isPost;

            HookedEvent(std::string eN, bool iP) : eventName(std::move(eN)), isPost(iP) {}
            ~HookedEvent() { isPost ? gameWrapper->UnhookEventPost(eventName) : gameWrapper->UnhookEvent(eventName); }

            static void UnhookEvent(std::string eventName, bool isPost) {
                  // just because it might already exist.
                  if (isPost) {
                        gameWrapper->UnhookEventPost(eventName);
                  } else {
                        gameWrapper->UnhookEvent(eventName);
                  }
            }
      };

      struct hook_cmp {
            bool operator()(const std::shared_ptr<HookedEvent> & rhe, const std::shared_ptr<HookedEvent> & lhe) const {
                  return ((lhe->eventName == rhe->eventName) && (lhe->isPost == rhe->isPost));
            }
      };

      struct HookedEventHash {
            std::size_t operator()(const std::shared_ptr<HookedEvent> & hooked_event) const {
                  return std::hash<std::string> {}(hooked_event->eventName + std::to_string(hooked_event->isPost));
            }
      };

      static std::unordered_set<std::shared_ptr<HookedEvent>, HookedEventHash, hook_cmp> hooked_events;

public:
      HookedEvents()                = delete;
      HookedEvents(HookedEvents &)  = delete;
      HookedEvents(HookedEvents &&) = delete;

      static std::shared_ptr<GameWrapper> gameWrapper;

      template<typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type * = nullptr>
      static void AddHookedEventWithCaller(
            const std::string &                                                 eventName,
            std::function<void(T caller, void * params, std::string eventName)> func,
            bool                                                                isPost = false);
      static void AddHookedEvent(
            const std::string &                        eventName,
            std::function<void(std::string eventName)> func,
            bool                                       isPost = false);

      static void RemoveHook(std::string);
      static void RemoveHook(std::regex);
      static void RemoveAllHooks();
};

inline std::
      unordered_set<std::shared_ptr<HookedEvents::HookedEvent>, HookedEvents::HookedEventHash, HookedEvents::hook_cmp>
                                    HookedEvents::hooked_events;
inline std::shared_ptr<GameWrapper> HookedEvents::gameWrapper;

template<typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type *>
void HookedEvents::AddHookedEventWithCaller(
      const std::string &                                                 eventName,
      std::function<void(T caller, void * params, std::string eventName)> func,
      bool                                                                isPost) {
      if (gameWrapper == nullptr) {
            throw std::exception {
                  "gameWrapper not set for hooking functions (put "
                  "HookedEvents::gameWrapper = gameWrapper in "
                  "your OnLoad() plugin member function)"};
      }
      if (std::find_if(
                begin(hooked_events),
                end(hooked_events),
                [eventName, isPost](const std::shared_ptr<HookedEvent> & he) {
                      return ((he->eventName == eventName) && (he->isPost == isPost));
                })
          != end(hooked_events)) {
            log::LOG("Hooked event (with caller) \"{}\" already exists. Cannot overwrite.", eventName);
            return;
      }
      // just because it might already exist before we manage it.
      HookedEvent::UnhookEvent(eventName, isPost);
      if (isPost) {
            gameWrapper->HookEventWithCallerPost<T>(eventName, func);
      } else {
            gameWrapper->HookEventWithCaller<T>(eventName, func);
      }

      hooked_events.insert(std::shared_ptr<HookedEvent> {
            new HookedEvent {eventName, isPost}
      });
}

inline void HookedEvents::AddHookedEvent(
      const std::string &                        eventName,
      std::function<void(std::string eventName)> func,
      bool                                       isPost) {
      if (gameWrapper == nullptr) {
            throw std::exception {
                  "gameWrapper not set for hooking functions (put "
                  "HookedEvents::gameWrapper = gameWrapper in "
                  "your OnLoad() plugin member function)"};
      }
      if (std::find_if(
                begin(hooked_events),
                end(hooked_events),
                [eventName, isPost](const std::shared_ptr<HookedEvent> & he) {
                      return ((he->eventName == eventName) && (he->isPost == isPost));
                })
          != end(hooked_events)) {
            log::LOG("Hooked event \"{}\" already exists. Cannot overwrite.", eventName);
            return;
      }
      // just because it might exist before we manage it.
      HookedEvent::UnhookEvent(eventName, isPost);
      void (GameWrapper::*hook)(std::string eventName, std::function<void(std::string eventName)> callback) =
            isPost ? &GameWrapper::HookEventPost : &GameWrapper::HookEvent;
      (gameWrapper.get()->*hook)(eventName, func);

      hooked_events.insert(std::shared_ptr<HookedEvent> {
            new HookedEvent {eventName, isPost}
      });
}

inline void HookedEvents::RemoveHook(std::string key) {
      auto it = std::find_if(begin(hooked_events), end(hooked_events), [key](const auto & he) {
            return he->eventName == key;
      });
      if (it != end(hooked_events)) {
            hooked_events.erase(it);
      }
}

inline void HookedEvents::RemoveHook(std::regex key) {
}

/**
 * @brief You shouldn't necessarily need to call this yourself, as all hooks unhook when the underlying structure gets
 * destroyed at the end of the program, but if you want to remove them all before then explicitly, then call this.
 */
inline void HookedEvents::RemoveAllHooks() {
      hooked_events.clear();
}

#endif
