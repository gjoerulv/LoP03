#pragma once

#include <filesystem>

namespace cd::platform {

// M108: the one-time save migration for the "Are P Geese" rebrand (owner
// decision: FULL rebrand including the save folder). If the new user-data
// directory is absent (or exists empty) and the legacy CrystalDungeons one
// exists, everything is copied across — recursively, skip-on-error, and the
// legacy directory is NEVER modified or deleted, so nothing can be lost even
// if the copy is interrupted. When both directories carry data, the new one
// wins and nothing is touched. Returns the number of files copied (0 =
// nothing to do). Pure filesystem work; the path-pair core is unit-tested on
// temp directories.

int migrateUserData(const std::filesystem::path& from, const std::filesystem::path& to);

// The production pair: legacy dir -> current dir (paths::userDataDir).
int migrateLegacyUserData();

}  // namespace cd::platform
