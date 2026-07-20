#include "platform/AtomicFile.hpp"

#include <fstream>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace cd::platform {
namespace fs = std::filesystem;

namespace {

void removeTemporary(const fs::path& path) {
    std::error_code ignored;
    fs::remove(path, ignored);
}

bool replaceDestination(const fs::path& temporary, const fs::path& destination,
                        std::string& error) {
#ifdef _WIN32
    if (MoveFileExW(temporary.c_str(), destination.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        error = "could not replace destination (Windows error " +
                std::to_string(GetLastError()) + ")";
        return false;
    }
    return true;
#else
    std::error_code ec;
    fs::rename(temporary, destination, ec);
    if (ec) {
        error = "could not replace destination: " + ec.message();
        return false;
    }
    return true;
#endif
}

} // namespace

bool writeTextFileAtomically(const fs::path& destination, std::string_view contents,
                             std::string& error, AtomicWriteOptions options) {
    error.clear();
    if (destination.empty()) {
        error = "destination path is empty";
        return false;
    }

    const fs::path parent = destination.parent_path();
    std::error_code ec;
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            error = "could not create directory '" + parent.string() + "': " + ec.message();
            return false;
        }
    }

    fs::path temporary = destination;
    temporary += ".tmp";
    fs::path backup = destination;
    backup += ".bak";

    if (fs::exists(temporary, ec)) {
        ec.clear();
        fs::remove(temporary, ec);
        if (ec || fs::exists(temporary)) {
            error = "could not remove stale temporary file '" + temporary.string() + "'";
            return false;
        }
    }

    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "could not open temporary file '" + temporary.string() + "'";
            return false;
        }
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        out.flush();
        if (!out) {
            error = "failed while writing temporary file '" + temporary.string() + "'";
            out.close();
            removeTemporary(temporary);
            return false;
        }
        out.close();
        if (!out) {
            error = "failed while closing temporary file '" + temporary.string() + "'";
            removeTemporary(temporary);
            return false;
        }
    }

    if (options.keepBackup && fs::exists(destination, ec) && !ec) {
        ec.clear();
        fs::copy_file(destination, backup, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            error = "could not create backup '" + backup.string() + "': " + ec.message();
            removeTemporary(temporary);
            return false;
        }
    }

    if (!replaceDestination(temporary, destination, error)) {
        removeTemporary(temporary);
        return false;
    }
    return true;
}

} // namespace cd::platform
