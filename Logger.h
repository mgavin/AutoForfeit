/*
 * reference:
 * https://github.com/Martinii89/OrganizeMyGarageOpenSource/blob/master/OrganizeMyGarageV2/logging.h
 *
 * TODO:
 *   - get a handle on enums with states / options
 *   - clean up the namespace? make it more intuitive to use?
 *
 *
 * I seen't a log.INFO() notation...
 * instead of LOGGER(type, msg);
 *
 * unfortunately this has a lot of unnecessary cost when you don't need certain debug
 * levels :\ (although it is kind of nice to have when changing logging levels at runtime)
 */

#ifndef _LOGGER_HPP_
#define _LOGGER_HPP_

#include <format>
#include <source_location>
#include <string_view>

#include <codecvt>
#include <locale>
#include <string>

// #include "flagpp/flags.hpp"

#include "bakkesmod/wrappers/cvarmanagerwrapper.h"
// import scoped_enum_bitmask;

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

namespace details {
      struct FormatString {
            std::string_view     str;
            std::source_location loc {};

            FormatString(const char * str, const std::source_location & loc = std::source_location::current()) :
                  str(str), loc(loc) {}
            FormatString(const std::string & str, const std::source_location & loc = std::source_location::current()) :
                  str(str), loc(loc) {}
            FormatString(const std::string && str, const std::source_location & loc = std::source_location::current()) :
                  str(str), loc(loc) {}

            [[nodiscard]] std::string GetLocation() const {
                  return std::format("[{} ({}:{})]", loc.function_name(), loc.file_name(), loc.line());
            }
      };

      struct FormatWString {
            std::wstring_view    str;
            std::source_location loc {};

            FormatWString(const wchar_t * str, const std::source_location & loc = std::source_location::current()) :
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
                  auto basic_string = std::format("[{} ({}:{})]", loc.function_name(), loc.file_name(), loc.line());
                  return std::wstring(basic_string.begin(), basic_string.end());
            }
      };
}  // namespace details

enum class LOGLEVEL { INFO, DEBUG, WARNING, ERROR, OFF };

enum class LOGOPTIONS {
      NONE      = 0,
      PERSIST   = 1 << 0,  // write out to a file
      CONSOLE   = 1 << 1,  // write out to the bakkesmod console
      SOURCELOC = 1 << 2,  // include the source location
};
}  // namespace LOGGER

constexpr inline bool operator&(const LOGGER::LOGOPTIONS & lhs, const LOGGER::LOGOPTIONS & rhs) {
      return false;
}

namespace LOGGER {
// NOLINTBEGIN
static LOGOPTIONS g_options  = LOGOPTIONS::NONE;  // default level of options...
static LOGLEVEL   g_loglevel = LOGLEVEL::OFF;     // default log level

// NOLINTEND

static std::shared_ptr<CVarManagerWrapper> g_cvarmanager;

__forceinline void set_cvarmanager(std::shared_ptr<CVarManagerWrapper> cmw) {
      g_cvarmanager = cmw;
}

constexpr inline void set_loglevel(const LOGGER::LOGLEVEL & nl) {
      g_loglevel = nl;
}

using details::FormatString;
using details::FormatWString;

//static ::std::queue<::std::pair<::std::wstring, int>> rolling_log;
//static ::std::map<::std::wstring, int>                roll_log_db;
//// static int roll_log_sz;
//
//constexpr inline void roll_log(std::string & str) {
//      ::std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
//      ::std::wstring                                           wide = converter.from_bytes(str);
//      roll_log(wide);
//}
//
//constexpr inline void roll_log(std::wstring & str) {
//      //
//      if (rolling_log.empty()) {
//            rolling_log.emplace(std::pair {str, 1});
//      } else {
//            if (roll_log_db[str] != 0) {
//                  // pre-existing string.
//            }
//      }
//}

template <typename... Args>
constexpr inline void LOG(const FormatString & format_str, Args &&... args) {
      auto str = std::format(
            "{}{}{}",
            g_options & LOGOPTIONS::SOURCELOC ? format_str.GetLocation() : "",
            g_options & LOGOPTIONS::SOURCELOC ? " " : "",
            sizeof...(args) ? std::vformat(format_str.str, std::make_format_args(args...)).c_str() : format_str.str);

      // roll_log(std::move(str));
      g_cvarmanager->log(std::move(str));
}

template <typename... Args>
constexpr inline void LOG(const FormatWString & wformat_str, Args &&... args) {
      auto str = std::format(
            L"{}{}{}",
            g_options & LOGOPTIONS::SOURCELOC ? wformat_str.GetLocation() : L"",
            g_options & LOGOPTIONS::SOURCELOC ? L" " : L"",
            sizeof...(args) ? std::vformat(wformat_str.str, std::make_wformat_args(args...)) : wformat_str.str);

      // roll_log(std::move(str));
      g_cvarmanager->log(std::move(str));
}

// USING LOGLEVEL
template <typename... Args>
constexpr inline void LOG(const LOGLEVEL & log_level, const FormatString & format_str, Args &&... args) {
      if (log_level < g_loglevel) { return; }
      LOG(format_str, args...);
}

template <typename... Args>
constexpr inline void LOG(const LOGLEVEL & log_level, const FormatWString & wformat_str, Args &&... args) {
      if (log_level < g_loglevel) { return; }
      LOG(wformat_str, args...);
}

template <typename... Args>
constexpr inline void log_info(const FormatString & format_str, Args &&... args) {
      LOG(LOGLEVEL::INFO, format_str, args...);
}

template <typename... Args>
constexpr inline void log_info(const FormatWString & wformat_str, Args &&... args) {
      LOG(LOGLEVEL::INFO, wformat_str, args...);
}

template <typename... Args>
constexpr inline void log_debug(const FormatString & format_str, Args &&... args) {
      LOG(LOGLEVEL::DEBUG, format_str, args...);
}

template <typename... Args>
constexpr inline void log_debug(const FormatWString & wformat_str, Args &&... args) {
      LOG(LOGLEVEL::DEBUG, wformat_str, args...);
}

template <typename... Args>
constexpr inline void log_warning(const FormatString & format_str, Args &&... args) {
      LOG(LOGLEVEL::WARNING, format_str, args...);
}

template <typename... Args>
constexpr inline void log_warning(const FormatWString & wformat_str, Args &&... args) {
      LOG(LOGLEVEL::WARNING, wformat_str, args...);
}

template <typename... Args>
constexpr inline void log_error(const FormatString & format_str, Args &&... args) {
      LOG(LOGLEVEL::ERROR, format_str, args...);
}

template <typename... Args>
constexpr inline void log_error(const FormatWString & wformat_str, Args &&... args) {
      LOG(LOGLEVEL::ERROR, wformat_str, args...);
}

};  // namespace LOGGER

#ifdef _WIN32
// We restore the ERROR Windows macro
#pragma pop_macro("ERROR")
#pragma pop_macro("DEBUG")
#endif

#endif
