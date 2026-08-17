// M108 — the rebrand's one-time save migration: legacy CrystalDungeons data
// copies into the new home exactly once, recursively, and the legacy dir is
// never modified; a populated new home wins and nothing moves.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "platform/Migration.hpp"

using namespace cd;
namespace fs = std::filesystem;

namespace {

fs::path freshDir(const char* tag) {
    static int counter = 0;
    const fs::path dir =
        fs::temp_directory_path() / ("cd_migrate_" + std::string(tag) + std::to_string(counter++));
    fs::remove_all(dir);
    return dir;
}

void writeFile(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p) << text;
}

std::string readFile(const fs::path& p) {
    std::ifstream in(p);
    std::string s;
    std::getline(in, s);
    return s;
}

}  // namespace

TEST_CASE("migration: legacy data copies once, nested and intact", "[migration][m108]") {
    const fs::path from = freshDir("from");
    const fs::path to = freshDir("to");
    writeFile(from / "save_slot_1.json", "slot-one");
    writeFile(from / "settings.json", "settings");
    writeFile(from / "logs" / "old.log", "log-line");

    CHECK(platform::migrateUserData(from, to) == 3);
    CHECK(readFile(to / "save_slot_1.json") == "slot-one");
    CHECK(readFile(to / "logs" / "old.log") == "log-line");
    // The legacy dir is untouched — still the recovery source.
    CHECK(readFile(from / "settings.json") == "settings");

    // Idempotent: the populated new home wins; nothing copies twice.
    writeFile(from / "late_addition.json", "never-moves");
    CHECK(platform::migrateUserData(from, to) == 0);
    CHECK_FALSE(fs::exists(to / "late_addition.json"));

    fs::remove_all(from);
    fs::remove_all(to);
}

TEST_CASE("migration: fresh installs and missing sources are quiet no-ops",
          "[migration][m108]") {
    const fs::path from = freshDir("none");
    const fs::path to = freshDir("target");
    CHECK(platform::migrateUserData(from, to) == 0);  // no legacy dir at all
    CHECK_FALSE(fs::exists(to));                      // nothing invented

    // An EMPTY new home still accepts the copy (created-but-unwritten dirs
    // must not block a real migration).
    writeFile(from / "profile.json", "profile");
    fs::create_directories(to);
    CHECK(platform::migrateUserData(from, to) == 1);
    CHECK(readFile(to / "profile.json") == "profile");

    fs::remove_all(from);
    fs::remove_all(to);
}
