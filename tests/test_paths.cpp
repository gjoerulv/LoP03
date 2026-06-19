#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "platform/Paths.hpp"

namespace fs = std::filesystem;
using cd::paths::sanitizeRelative;
using cd::paths::userDataDir;

TEST_CASE("paths: accepts safe relative paths", "[paths]") {
    auto a = sanitizeRelative("saves/slot1.json");
    REQUIRE(a.has_value());
    REQUIRE(*a == fs::path("saves") / "slot1.json");

    auto b = sanitizeRelative("data/./config.json");
    REQUIRE(b.has_value());
    REQUIRE(*b == fs::path("data") / "config.json");

    // Internal ".." that does not escape the root is allowed after normalization.
    REQUIRE(sanitizeRelative("a/b/..").has_value());
}

TEST_CASE("paths: rejects empty and current-dir-only input", "[paths]") {
    REQUIRE_FALSE(sanitizeRelative("").has_value());
    REQUIRE_FALSE(sanitizeRelative(".").has_value());
}

TEST_CASE("paths: rejects traversal that escapes the root", "[paths]") {
    REQUIRE_FALSE(sanitizeRelative("../secret").has_value());
    REQUIRE_FALSE(sanitizeRelative("a/../../b").has_value());
    REQUIRE_FALSE(sanitizeRelative("..").has_value());
}

TEST_CASE("paths: rejects absolute paths and root directories", "[paths]") {
    REQUIRE_FALSE(sanitizeRelative("/etc/passwd").has_value());
    REQUIRE_FALSE(sanitizeRelative("/abs/path").has_value());
}

TEST_CASE("paths: user data dir is named for the app", "[paths]") {
    const fs::path dir = userDataDir();
    REQUIRE_FALSE(dir.empty());
    REQUIRE(dir.filename() == "CrystalDungeons");
}
