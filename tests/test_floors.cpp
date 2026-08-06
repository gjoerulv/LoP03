// M82 — 1-or-4-floor runs (generation v15). Proves: the floor sub-seed
// derivation (floor 0 IS the run seed, so 1-floor output is byte-identical to
// v14), the stair-gate post-pass (only the stair floors' boss-slot team
// differs from that sub-seed's standalone generation), per-floor topology and
// layout invariants over both shapes, and the split-scoreboard field.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/RoomLayout.hpp"
#include "score/Scoreboard.hpp"
#include "score/ScoreEntry.hpp"

using namespace cd;
namespace fs = std::filesystem;

namespace {

const content::ContentDatabase& db() {
    static content::ContentDatabase database = [] {
        content::ContentDatabase d;
        content::LoadReport report;
        REQUIRE(content::loadAll(fs::path(CRYSTAL_TEST_DATA_DIR), d, report));
        return d;
    }();
    return database;
}

fs::path makeTempDir() {
    static std::atomic<int> counter{0};
    std::random_device rd;
    fs::path dir = fs::temp_directory_path() /
                   ("cd_floors_" + std::to_string(rd()) + "_" + std::to_string(counter++));
    fs::create_directories(dir);
    return dir;
}

bool sameTeam(const dungeon::EnemyTeam& a, const dungeon::EnemyTeam& b) {
    return a.name == b.name && a.enemyIds == b.enemyIds && a.tags == b.tags &&
           a.isBoss == b.isBoss && a.bossId == b.bossId && a.statScalePct == b.statScalePct;
}

bool sameRoom(const dungeon::Room& a, const dungeon::Room& b) {
    if (a.gridX != b.gridX || a.gridY != b.gridY || a.type != b.type ||
        a.teamIndex != b.teamIndex) {
        return false;
    }
    for (int i = 0; i < dungeon::kDirCount; ++i) {
        const auto& da = a.doors[static_cast<std::size_t>(i)];
        const auto& dbr = b.doors[static_cast<std::size_t>(i)];
        if (da.neighbor != dbr.neighbor || da.gated != dbr.gated ||
            da.teamIndex != dbr.teamIndex) {
            return false;
        }
    }
    const auto& ca = a.chest;
    const auto& cb = b.chest;
    if (ca.present != cb.present || ca.guarded != cb.guarded || ca.trapped != cb.trapped ||
        ca.gold != cb.gold || ca.itemId != cb.itemId || ca.rarity != cb.rarity) {
        return false;
    }
    return a.event.kind == b.event.kind && a.event.goldCost == b.event.goldCost &&
           a.event.itemId == b.event.itemId;
}

// Everything a seed decides, EXCEPT the M82 metadata (runSeed/floorIndex/
// floorCount) and — when skipBossTeam — the boss-room team the stair-gate
// post-pass swaps.
bool sameDungeon(const dungeon::Dungeon& a, const dungeon::Dungeon& b, bool skipBossTeam) {
    if (a.seed != b.seed || a.depth != b.depth || a.town != b.town ||
        a.themeId != b.themeId || a.rooms.size() != b.rooms.size() ||
        a.teams.size() != b.teams.size() || a.mainPath != b.mainPath ||
        a.startRoom != b.startRoom || a.bossRoom != b.bossRoom ||
        a.mandatoryGates != b.mandatoryGates || a.mapPieceRoom != b.mapPieceRoom ||
        a.chartRoom != b.chartRoom || a.buriedRoom != b.buriedRoom) {
        return false;
    }
    for (std::size_t i = 0; i < a.rooms.size(); ++i) {
        if (!sameRoom(a.rooms[i], b.rooms[i])) {
            return false;
        }
    }
    const int bossTeam = a.rooms[static_cast<std::size_t>(a.bossRoom)].teamIndex;
    for (std::size_t i = 0; i < a.teams.size(); ++i) {
        if (skipBossTeam && static_cast<int>(i) == bossTeam) {
            continue;
        }
        if (!sameTeam(a.teams[i], b.teams[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("floors: floor 0 is the run seed and deeper floors are distinct", "[floors]") {
    CHECK(dungeon::floorSeed(424242, 0) == 424242ull);
    CHECK(dungeon::floorSeed(7, 0) == 7ull);
    const std::uint64_t f1 = dungeon::floorSeed(424242, 1);
    const std::uint64_t f2 = dungeon::floorSeed(424242, 2);
    const std::uint64_t f3 = dungeon::floorSeed(424242, 3);
    CHECK(f1 != 424242ull);
    CHECK(f1 != f2);
    CHECK(f2 != f3);
    CHECK(f1 != f3);
    CHECK(dungeon::floorSeed(424242, 1) == f1);  // stable
    CHECK(dungeon::floorSeed(424243, 1) != f1);  // seed-dependent
}

TEST_CASE("floors: a 1-floor run is byte-identical to plain generation", "[floors]") {
    // The v14-identity guarantee: the picker's existence changes nothing about
    // what a seed always meant at Floors: 1.
    for (std::uint64_t seed : {1ull, 424242ull, 999983ull}) {
        const dungeon::Dungeon plain = dungeon::generate(seed, 6, db(), "ruined_keep", 3);
        const std::vector<dungeon::Dungeon> run =
            dungeon::generateFloors(seed, 6, db(), "ruined_keep", 3, 1);
        REQUIRE(run.size() == 1);
        INFO("seed " << seed);
        CHECK(sameDungeon(plain, run[0], /*skipBossTeam=*/false));
        CHECK(run[0].runSeed == seed);
        CHECK(run[0].floorIndex == 0);
        CHECK(run[0].floorCount == 1);
        // The boss slot holds the real boss, exactly as always.
        const int bt = run[0].rooms[static_cast<std::size_t>(run[0].bossRoom)].teamIndex;
        REQUIRE(bt >= 0);
        CHECK(run[0].teams[static_cast<std::size_t>(bt)].isBoss);
    }
}

TEST_CASE("floors: a 4-floor run swaps exactly the stair floors' boss teams", "[floors]") {
    const std::uint64_t seed = 424242;
    const std::vector<dungeon::Dungeon> run =
        dungeon::generateFloors(seed, 8, db(), "crystal_mine", 4, 4);
    REQUIRE(run.size() == 4);

    for (int i = 0; i < 4; ++i) {
        INFO("floor " << i);
        const dungeon::Dungeon& f = run[static_cast<std::size_t>(i)];
        CHECK(f.runSeed == seed);
        CHECK(f.floorIndex == i);
        CHECK(f.floorCount == 4);
        CHECK(f.seed == dungeon::floorSeed(seed, i));

        // Sub-seed independence: except the swapped team, the floor is what
        // its sub-seed generates standalone — editing floor 3 can never move
        // floor 1.
        const dungeon::Dungeon standalone =
            dungeon::generate(dungeon::floorSeed(seed, i), 8, db(), "crystal_mine", 4);
        const bool stairFloor = i < 3;
        CHECK(sameDungeon(standalone, f, /*skipBossTeam=*/stairFloor));

        const int bt = f.rooms[static_cast<std::size_t>(f.bossRoom)].teamIndex;
        REQUIRE(bt >= 0);
        const dungeon::EnemyTeam& team = f.teams[static_cast<std::size_t>(bt)];
        if (stairFloor) {
            CHECK_FALSE(team.isBoss);
            CHECK(team.bossId.empty());
            CHECK(team.name == "Stairway Wardens");
            CHECK_FALSE(team.enemyIds.empty());
            // The gate is drawn all-elite (normals only when a theme has no
            // elites at this town — the shipped themes all do).
            for (const std::string& id : team.enemyIds) {
                const content::EnemyDef* def = db().findEnemy(id);
                REQUIRE(def != nullptr);
                CHECK(def->tier == content::EnemyTier::Elite);
            }
            // Scaled like every other team at this depth/town.
            const dungeon::EnemyTeam& standaloneBoss =
                standalone.teams[static_cast<std::size_t>(
                    standalone.rooms[static_cast<std::size_t>(standalone.bossRoom)].teamIndex)];
            CHECK(team.statScalePct == standaloneBoss.statScalePct);
        } else {
            CHECK(team.isBoss);  // the real boss waits at the bottom
        }
    }

    // Determinism: the same call reproduces every floor.
    const std::vector<dungeon::Dungeon> again =
        dungeon::generateFloors(seed, 8, db(), "crystal_mine", 4, 4);
    REQUIRE(again.size() == 4);
    for (int i = 0; i < 4; ++i) {
        CHECK(sameDungeon(run[static_cast<std::size_t>(i)], again[static_cast<std::size_t>(i)],
                          /*skipBossTeam=*/false));
    }
}

TEST_CASE("floors: every floor of both shapes holds the invariants", "[floors]") {
    const char* themes[] = {"ruined_keep", "crystal_mine", "hollow_forest"};
    int floorsChecked = 0;
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        for (int count : {1, 4}) {
            const std::vector<dungeon::Dungeon> run = dungeon::generateFloors(
                seed * 104729u, 6, db(), themes[seed % 3], 4, count);
            REQUIRE(static_cast<int>(run.size()) == count);
            for (const dungeon::Dungeon& f : run) {
                ++floorsChecked;
                INFO("seed " << seed * 104729u << " floor " << f.floorIndex << " of "
                             << f.floorCount);
                REQUIRE_FALSE(f.mainPath.empty());
                CHECK(f.rooms[static_cast<std::size_t>(f.startRoom)].type ==
                      dungeon::RoomType::Start);
                CHECK(f.rooms[static_cast<std::size_t>(f.bossRoom)].type ==
                      dungeon::RoomType::Boss);
                CHECK(f.mandatoryGates >= 3);
                CHECK(f.rooms[static_cast<std::size_t>(f.bossRoom)].teamIndex >= 0);
                // Every room realizes and validates (the M23 layout bar).
                const std::vector<dungeon::RoomLayout> layouts =
                    dungeon::realizeAllRooms(f);
                for (std::size_t r = 0; r < layouts.size(); ++r) {
                    const std::vector<std::string> problems =
                        dungeon::validateLayout(f, static_cast<int>(r), layouts[r]);
                    INFO("room " << r << ": "
                                 << (problems.empty() ? "ok" : problems.front()));
                    REQUIRE(problems.empty());
                }
            }
        }
    }
    CHECK(floorsChecked == 8 * (1 + 4));
}

TEST_CASE("floors: score entries carry the shape and split onto boards", "[floors]") {
    score::ScoreEntry classic;
    classic.score = 100;
    CHECK(classic.floors == 1);  // the default IS the legacy meaning
    score::ScoreEntry deep;
    deep.score = 200;
    deep.floors = 4;

    CHECK(score::onFloorsBoard(classic, 1));
    CHECK_FALSE(score::onFloorsBoard(classic, 4));
    CHECK(score::onFloorsBoard(deep, 4));
    CHECK_FALSE(score::onFloorsBoard(deep, 1));

    // Round-trip through the file.
    const fs::path dir = makeTempDir();
    const fs::path file = dir / "scoreboard.json";
    {
        score::Scoreboard board(file);
        board.add(classic);
        board.add(deep);
        content::LoadReport report;
        REQUIRE(board.save(report));
    }
    {
        score::Scoreboard board(file);
        content::LoadReport report;
        REQUIRE(board.load(report));
        REQUIRE(board.entries().size() == 2);
        CHECK(board.entries()[0].floors == 4);  // score 200 ranks first
        CHECK(board.entries()[1].floors == 1);
    }
    // A legacy file WITHOUT the field loads as a 1-floor entry.
    {
        std::ofstream out(file);
        out << R"({"version":1,"entries":[{"score":50,"battleTurns":9,"theme":"Ruined Keep"}]})";
    }
    {
        score::Scoreboard board(file);
        content::LoadReport report;
        REQUIRE(board.load(report));
        REQUIRE(board.entries().size() == 1);
        CHECK(board.entries()[0].floors == 1);
    }
    fs::remove_all(dir);
}
