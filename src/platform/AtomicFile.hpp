#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace cd::platform {

struct AtomicWriteOptions {
    bool keepBackup = false;
};

// Writes complete text to a sibling temporary file and replaces the destination
// only after the temporary write has been flushed and closed successfully.
// On failure, the existing destination is left untouched.
bool writeTextFileAtomically(const std::filesystem::path& destination,
                             std::string_view contents,
                             std::string& error,
                             AtomicWriteOptions options = {});

} // namespace cd::platform
