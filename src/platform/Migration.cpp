#include "platform/Migration.hpp"

#include <system_error>

#include "core/Log.hpp"
#include "platform/Paths.hpp"

namespace cd::platform {

namespace fs = std::filesystem;

int migrateUserData(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    if (!fs::exists(from, ec) || ec || !fs::is_directory(from, ec) || ec) {
        return 0;  // no legacy data: a fresh install
    }
    if (fs::exists(to, ec) && !ec && !fs::is_empty(to, ec)) {
        return 0;  // the new home already carries data: it wins, touch nothing
    }
    fs::create_directories(to, ec);
    if (ec) {
        return 0;  // cannot create the target: play proceeds on a fresh dir
    }
    int copied = 0;
    for (auto it = fs::recursive_directory_iterator(
             from, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        const fs::path rel = fs::relative(it->path(), from, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        const fs::path target = to / rel;
        std::error_code entryEc;
        if (it->is_directory(entryEc)) {
            fs::create_directories(target, entryEc);
        } else if (it->is_regular_file(entryEc)) {
            fs::create_directories(target.parent_path(), entryEc);
            if (fs::copy_file(it->path(), target, fs::copy_options::skip_existing, entryEc)) {
                ++copied;
            }
        }
        // Any per-entry failure is skipped, never fatal: a partial migration
        // still leaves the untouched legacy dir as the recovery source.
    }
    return copied;
}

int migrateLegacyUserData() {
    const int copied = migrateUserData(paths::legacyUserDataDir(), paths::userDataDir());
    if (copied > 0) {
        log::info("Migrated " + std::to_string(copied) +
                  " save/config file(s) from the CrystalDungeons folder (the old "
                  "folder is untouched).");
    }
    return copied;
}

}  // namespace cd::platform
