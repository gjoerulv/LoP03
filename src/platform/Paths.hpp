#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

// Path helpers with safety baked in. No raylib, no shell execution.

namespace cd::paths {

// Normalizes a relative path and REJECTS anything unsafe: empty input, absolute
// paths, drive/root names, or any ".." traversal component. Returns std::nullopt
// when the input is not a safe relative path. Used for all data/save file access
// so untrusted strings cannot escape the intended directory.
std::optional<std::filesystem::path> sanitizeRelative(std::string_view relative);

// Per-user writable directory for saves/config, resolved from environment
// variables only (Windows: %APPDATA%/CrystalDungeons; otherwise $XDG_DATA_HOME
// or ~/.local/share/CrystalDungeons). Falls back to "./CrystalDungeons" if no
// suitable variable is set. Does not create the directory.
std::filesystem::path userDataDir();

}  // namespace cd::paths
