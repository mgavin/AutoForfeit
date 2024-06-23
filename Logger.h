#pragma once
#include <format>
#include "bakkesmod/plugin/bakkesmodplugin.h"

// TODO: create a class out of this someday
// TODO: take from martini's logger (garage organizer thingy)

extern std::shared_ptr<CVarManagerWrapper> _globalCVarManager;
template<typename S, typename... Args>
void LOG(const S & format_str, Args &&... args) {
        _globalCVarManager->log(std::vformat(format_str, std::make_format_args(args...)));
}
