#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "content/LoadReport.hpp"
#include "score/ScoreEntry.hpp"
#include "score/Scoreboard.hpp"

using namespace cd;
namespace fs = std::filesystem;

namespace {

fs::path makeTempDir() {
    static std::atomic<int> counter{0};
    std::random_device rd;
    fs::path dir = fs::temp_directory_path() /
                   ("cd_score_" + std::to_string(rd()) + "_" + std::to_string(counter++));
    fs::create_directories(dir);
    return dir;
}

score::ScoreEntry entry(int s, int turns) {
    score::ScoreEntry e;
    e.score = s;
    e.battleTurns = turns;
    e.theme = "Ruined Keep";
    e.seed = 12345;
    return e;
}

}  // namespace

TEST_CASE("scoreboard: entries are ranked best-first", "[score]") {
    const fs::path dir = makeTempDir();
    score::Scoreboard board(dir / "scoreboard.json");
    board.add(entry(1000, 20));
    board.add(entry(3000, 10));
    board.add(entry(2000, 15));

    REQUIRE(board.entries().size() == 3);
    REQUIRE(board.entries()[0].score == 3000);
    REQUIRE(board.entries()[1].score == 2000);
    REQUIRE(board.entries()[2].score == 1000);

    fs::remove_all(dir);
}

TEST_CASE("scoreboard: equal scores break ties on fewer turns", "[score]") {
    REQUIRE(score::ranksAbove(entry(2000, 8), entry(2000, 12)));
    REQUIRE_FALSE(score::ranksAbove(entry(2000, 12), entry(2000, 8)));
}

TEST_CASE("scoreboard: saves and reloads", "[score]") {
    const fs::path dir = makeTempDir();
    const fs::path file = dir / "scoreboard.json";
    {
        score::Scoreboard board(file);
        board.add(entry(2500, 9));
        board.add(entry(1500, 14));
        content::LoadReport rep;
        REQUIRE(board.save(rep));
    }
    {
        score::Scoreboard board(file);
        content::LoadReport rep;
        REQUIRE(board.load(rep));
        REQUIRE(board.entries().size() == 2);
        REQUIRE(board.entries()[0].score == 2500);
        REQUIRE(board.entries()[0].battleTurns == 9);
    }
    fs::remove_all(dir);
}

TEST_CASE("scoreboard: a missing file loads as an empty board", "[score]") {
    const fs::path dir = makeTempDir();
    score::Scoreboard board(dir / "nope.json");
    content::LoadReport rep;
    REQUIRE(board.load(rep));
    REQUIRE(board.empty());
    fs::remove_all(dir);
}

TEST_CASE("scoreboard: entries without generationVersion load as pre-M16", "[score]") {
    const fs::path dir = makeTempDir();
    const fs::path file = dir / "scoreboard.json";
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << R"({"version":1,"entries":[{"score":900,"battleTurns":11,)"
            << R"("dangerDefeated":4,"chestsOpened":2,"noDeath":1,"depth":2,)"
            << R"("theme":"Ruined Keep","seed":42}]})";
    }
    score::Scoreboard board(file);
    content::LoadReport rep;
    REQUIRE(board.load(rep));
    REQUIRE(board.entries().size() == 1);
    REQUIRE(board.entries()[0].score == 900);
    REQUIRE(board.entries()[0].generationVersion == 0);  // absent field = pre-versioning
    REQUIRE(board.entries()[0].partyLevel == 0);         // absent field = legacy run
    fs::remove_all(dir);
}

TEST_CASE("scoreboard: partyLevel comparability tag round-trips", "[score]") {
    const fs::path dir = makeTempDir();
    const fs::path file = dir / "scoreboard.json";
    {
        score::Scoreboard board(file);
        score::ScoreEntry e = entry(1500, 9);
        e.partyLevel = 7;
        board.add(e);
        content::LoadReport rep;
        REQUIRE(board.save(rep));
    }
    {
        score::Scoreboard board(file);
        content::LoadReport rep;
        REQUIRE(board.load(rep));
        REQUIRE(board.entries().size() == 1);
        REQUIRE(board.entries()[0].partyLevel == 7);
    }
    fs::remove_all(dir);
}

TEST_CASE("scoreboard: generationVersion round-trips", "[score]") {
    const fs::path dir = makeTempDir();
    const fs::path file = dir / "scoreboard.json";
    {
        score::Scoreboard board(file);
        score::ScoreEntry e = entry(1200, 7);
        e.generationVersion = 2;
        board.add(e);
        content::LoadReport rep;
        REQUIRE(board.save(rep));
    }
    {
        score::Scoreboard board(file);
        content::LoadReport rep;
        REQUIRE(board.load(rep));
        REQUIRE(board.entries().size() == 1);
        REQUIRE(board.entries()[0].generationVersion == 2);
    }
    fs::remove_all(dir);
}

TEST_CASE("scoreboard: a malformed file is reported, never crashes", "[score]") {
    const fs::path dir = makeTempDir();
    const fs::path file = dir / "scoreboard.json";
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << "{ this is not valid json ";
    }
    score::Scoreboard board(file);
    content::LoadReport rep;
    REQUIRE_FALSE(board.load(rep));
    REQUIRE(board.empty());
    fs::remove_all(dir);
}
