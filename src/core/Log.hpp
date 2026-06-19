#pragma once

#include <string>

#include "raylib.h"

// Thin wrapper over raylib's TraceLog so the rest of the codebase has a single,
// replaceable logging entry point. raylib's logger works before/without a
// window, so these are always safe to call.

namespace cd::log {

inline void info(const std::string& msg) { TraceLog(LOG_INFO, "%s", msg.c_str()); }
inline void warn(const std::string& msg) { TraceLog(LOG_WARNING, "%s", msg.c_str()); }
inline void error(const std::string& msg) { TraceLog(LOG_ERROR, "%s", msg.c_str()); }

}  // namespace cd::log
