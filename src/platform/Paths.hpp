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
// variables only (Windows: %APPDATA%/ArePGeese; otherwise $XDG_DATA_HOME
// or ~/.local/share/ArePGeese). Falls back to "./ArePGeese" if no
// suitable variable is set. Does not create the directory.
// (M108: renamed with the "Are P Geese" rebrand; platform/Migration copies a
// legacy CrystalDungeons folder across exactly once.)
std::filesystem::path userDataDir();

// M108: the pre-rebrand folder (same derivation, the old name) — the
// migration's source. Never written to.
std::filesystem::path legacyUserDataDir();

}  // namespace cd::paths
