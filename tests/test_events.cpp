#include <catch2/catch_test_macros.hpp>

#include "score/Scoring.hpp"

#ifdef CRYSTAL_TEST_DATA_DIR
#include <filesystem>
#include <set>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#endif

using namespace cd;

TEST_CASE("events: the score wager pays and costs exactly as stated", "[events]") {
    score::RunSummary run;
    run.completed = true;
    run.battleTurns = 20;
    run.noDeath = true;

    const int base = score::computeScore(run);
    run.wagerAccepted = true;
    REQUIRE(score::computeScore(run) == base + 150);
    REQUIRE(score::scoreBreakdown(run).wager == 150);

    run.noDeath = false;
    const score::ScoreBreakdown lost = score::scoreBreakdown(run);
    REQUIRE(lost.wager == -100);

    // An unfinished run scores zero, wager or not.
    run.completed = false;
    REQUIRE(score::computeScore(run) == 0);
}

#ifdef CRYSTAL_TEST_DATA_DIR

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

int doorCount(const dungeon::Room& r) {
    int n = 0;
    for (int d = 0; d < dungeon::kDirCount; ++d) {
        n += r.doors[static_cast<std::size_t>(d)].neighbor >= 0 ? 1 : 0;
    }
    return n;
}

}  // namespace

TEST_CASE("events: generated event rooms are well-formed dead ends", "[events]") {
    const content::ContentDatabase db = loadContent();
    int eventRooms = 0;
    std::set<dungeon::RoomEventKind> kindsSeen;
    int trappedChests = 0;

    for (int i = 0; i < 30; ++i) {
        const std::uint64_t seed = static_cast<std::uint64_t>(i) * 9973u + 3u;
        for (int depth : {1, 4, 8}) {
            const dungeon::Dungeon d = dungeon::generate(seed, depth, db, "ruined_keep");
            std::set<dungeon::RoomEventKind> inThisDungeon;
            for (const dungeon::Room& r : d.rooms) {
                if (r.chest.present && r.chest.trapped) {
                    REQUIRE_FALSE(r.chest.guarded);  // traps only on unguarded chests
                    ++trappedChests;
                }
                if (r.type != dungeon::RoomType::Event) {
                    continue;
                }
                ++eventRooms;
                REQUIRE(r.event.kind != dungeon::RoomEventKind::None);
                REQUIRE(doorCount(r) == 1);  // dead end off the main path
                REQUIRE(inThisDungeon.count(r.event.kind) == 0);  // kinds unique per dungeon
                inThisDungeon.insert(r.event.kind);
                kindsSeen.insert(r.event.kind);
                switch (r.event.kind) {
                    case dungeon::RoomEventKind::Shrine:
                        REQUIRE(r.event.goldCost == 40 + 20 * depth);
                        break;
                    case dungeon::RoomEventKind::Merchant: {
                        REQUIRE_FALSE(r.event.itemId.empty());
                        REQUIRE(r.event.goldCost > 0);
                        const content::ItemDef* mit = db.findItem(r.event.itemId);
                        REQUIRE(mit != nullptr);
                        // M37: the merchant sells at 75% of value (a bargain, not a markup).
                        REQUIRE(r.event.goldCost == mit->value * 75 / 100);
                        break;
                    }
                    case dungeon::RoomEventKind::EliteChallenge:
                        REQUIRE(r.teamIndex >= 0);
                        REQUIRE(r.teamIndex < static_cast<int>(d.teams.size()));
                        REQUIRE_FALSE(d.teams[static_cast<std::size_t>(r.teamIndex)]
                                          .enemyIds.empty());
                        break;
                    default:
                        break;
                }
            }
        }
    }
    // Events appear regularly and every kind shows up somewhere in the sample
    // (6 kinds since M30 added the rest-token event).
    REQUIRE(eventRooms > 100);
    REQUIRE(kindsSeen.size() == 6);
    REQUIRE(kindsSeen.count(dungeon::RoomEventKind::RestToken) == 1);
    REQUIRE(trappedChests > 20);
}

TEST_CASE("events: event rooms never sit on the main path", "[events]") {
    const content::ContentDatabase db = loadContent();
    for (std::uint64_t seed : {5ull, 55ull, 555ull}) {
        const dungeon::Dungeon d = dungeon::generate(seed, 3, db, "hollow_forest");
        for (int idx : d.mainPath) {
            REQUIRE(d.rooms[static_cast<std::size_t>(idx)].type != dungeon::RoomType::Event);
        }
    }
}

#endif  // CRYSTAL_TEST_DATA_DIR
