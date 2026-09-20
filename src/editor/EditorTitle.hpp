#pragma once

#include <string>

#include "core/Version.hpp"

// M125 - CrystalForge's window title carries the project version it was built
// from (packaging/Version.hpp.in -> version::kString, the same stamp the
// game's title screen shows), so a screenshot or a bug report about the
// editor names its build. Pure, so the headless suite can pin it.

namespace cd::editor {

inline std::string windowTitle() {
    return std::string("CrystalForge - Are P Geese v") + version::kString;
}

}  // namespace cd::editor
