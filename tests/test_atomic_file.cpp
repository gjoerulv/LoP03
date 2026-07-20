#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

#include "platform/AtomicFile.hpp"

namespace fs = std::filesystem;
using cd::platform::AtomicWriteOptions;
using cd::platform::writeTextFileAtomically;

namespace {

fs::path uniqueTempDir() {
    static std::atomic<unsigned> counter{0};
    const auto token = std::random_device{}();
    return fs::temp_directory_path() /
           ("crystal_atomic_" + std::to_string(token) + "_" +
            std::to_string(counter.fetch_add(1)));
}

std::string readAll(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

struct TempDir {
    fs::path path = uniqueTempDir();
    TempDir() { fs::create_directories(path); }
    ~TempDir() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

} // namespace

TEST_CASE("atomic text write replaces the destination only after a complete write") {
    TempDir temp;
    const fs::path file = temp.path / "settings.json";
    {
        std::ofstream out(file);
        out << "old";
    }

    std::string error;
    REQUIRE(writeTextFileAtomically(file, "new\n", error));
    CHECK(error.empty());
    CHECK(readAll(file) == "new\n");
    CHECK_FALSE(fs::exists(fs::path(file.string() + ".tmp")));
}

TEST_CASE("atomic text write can preserve one previous generation") {
    TempDir temp;
    const fs::path file = temp.path / "slot1.json";
    {
        std::ofstream out(file);
        out << "previous";
    }

    std::string error;
    REQUIRE(writeTextFileAtomically(file, "current", error, AtomicWriteOptions{true}));
    CHECK(readAll(file) == "current");
    CHECK(readAll(fs::path(file.string() + ".bak")) == "previous");
}

TEST_CASE("atomic text write leaves an existing destination intact on temporary-file failure") {
    TempDir temp;
    const fs::path file = temp.path / "scoreboard.json";
    {
        std::ofstream out(file);
        out << "valid-existing-data";
    }

    const fs::path blockingTemp = fs::path(file.string() + ".tmp");
    fs::create_directories(blockingTemp);
    {
        std::ofstream out(blockingTemp / "block-removal.txt");
        out << "x";
    }

    std::string error;
    CHECK_FALSE(writeTextFileAtomically(file, "replacement", error));
    CHECK_FALSE(error.empty());
    CHECK(readAll(file) == "valid-existing-data");
}
