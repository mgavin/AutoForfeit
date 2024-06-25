/*
 * reference:
 * https://github.com/Martinii89/OrganizeMyGarageOpenSource/blob/master/OrganizeMyGarageV2/logging.h
 *
 * TODO:
 *   - get a handle on enums with states / options
 *   - clean up the namespace? make it more intuitive to use?
 */

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <format>
#include <source_location>
#include <string_view>

#include "flagpp/flags.hpp"

#include "bakkesmod/wrappers/cvarmanagerwrapper.h"

extern std::shared_ptr<CVarManagerWrapper> _globalCVarManager;

namespace LOGGER {
#ifdef _WIN32
// ERROR macro is defined in Windows header
// To avoid conflict between these macro and declaration of ERROR / DEBUG in SEVERITY enum
// We save macro and undef it
#pragma push_macro("ERROR")
#pragma push_macro("DEBUG")
#undef ERROR
#undef DEBUG
#endif

namespace {
      struct FormatString {
            std::string_view     str;
            std::source_location loc {};

            FormatString(
                  const char *                 str,
                  const std::source_location & loc = std::source_location::current()) :
                  str(str), loc(loc) {}
            FormatString(
                  const std::string &          str,
                  const std::source_location & loc = std::source_location::current()) :
                  str(str), loc(loc) {}
            FormatString(
                  const std::string &&         str,
                  const std::source_location & loc = std::source_location::current()) :
                  str(str), loc(loc) {}

            [[nodiscard]] std::string GetLocation() const {
                  return std::format(
                        "[{} ({}:{})]",
                        loc.function_name(),
                        loc.file_name(),
                        loc.line());
            }
      };

      struct FormatWString {
            std::wstring_view    str;
            std::source_location loc {};

            FormatWString(
                  const wchar_t *              str,
                  const std::source_location & loc = std::source_location::current()) :
                  str(str), loc(loc) {}
            FormatWString(
                  const std::wstring &         str,
                  const std::source_location & loc = std::source_location::current()) :
                  str(str), loc(loc) {}
            FormatWString(
                  const std::wstring &&        str,
                  const std::source_location & loc = std::source_location::current()) :
                  str(str), loc(loc) {}

            [[nodiscard]] std::wstring GetLocation() const {
                  auto basic_string = std::format(
                        "[{} ({}:{})]",
                        loc.function_name(),
                        loc.file_name(),
                        loc.line());
                  return std::wstring(basic_string.begin(), basic_string.end());
            }
      };
}  // namespace

enum class LOGLEVEL {
      INFO,
      DEBUG,
      WARNING,
      ERROR,
};

enum class LOGOPTIONS {
      NONE      = 0,
      PERSIST   = 1 << 0,  // write out to a file
      CONSOLE   = 1 << 1,  // write out to the bakkesmod console
      SOURCELOC = 1 << 2,  // include the source location
};
}  // namespace LOGGER

template<>
// NOLINTNEXTLINE
constexpr bool flagpp::enabled<LOGGER::LOGOPTIONS> = true;

namespace LOGGER {
// NOLINTBEGIN
LOGOPTIONS g_options  = LOGOPTIONS::NONE;  // default level of options...
LOGLEVEL   g_loglevel = LOGLEVEL::ERROR;   // default error level

// NOLINTEND

inline void set_loglevel(const LOGGER::LOGLEVEL & nl) {
      g_loglevel = nl;
}

template<typename... Args>
inline void LOG(const FormatString & format_str, Args &&... args) {
      auto str = std::format(
            "{}{}{}",
            g_options & LOGOPTIONS::SOURCELOC ? format_str.GetLocation() : "",
            g_options & LOGOPTIONS::SOURCELOC ? " " : "",
            std::vformat(
                  format_str.str,
                  std::make_format_args(std::forward<Args>(args)...)));
      _globalCVarManager->log(std::move(str));
}

template<typename... Args>
inline void LOG(const FormatWString & wformat_str, Args &&... args) {
      auto str = std::format(
            L"{}{}{}",
            g_options & LOGOPTIONS::SOURCELOC ? wformat_str.GetLocation() : L"",
            g_options & LOGOPTIONS::SOURCELOC ? " " : "",
            std::vformat(
                  wformat_str.str,
                  std::make_wformat_args(std::forward<Args>(args)...)));
      _globalCVarManager->log(std::move(str));
}

// USING LOGLEVEL
template<typename... Args>
inline void
      LOG(const LOGLEVEL log_level, const FormatString & format_str, Args &&... args) {
      if (log_level < g_loglevel) {
            return;
      }
      LOG(format_str, std::forward(args...));
}

template<typename... Args>
inline void LOG(
      const LOGLEVEL &      log_level,
      const FormatWString & wformat_str,
      Args &&... args) {
      if (log_level < g_loglevel) {
            return;
      }
      LOG(wformat_str, std::forward(args...));
}

};  // namespace LOGGER

#ifdef _WIN32
// We restore the ERROR Windows macro
#pragma pop_macro("ERROR")
#pragma pop_macro("DEBUG")
#endif

#endif
