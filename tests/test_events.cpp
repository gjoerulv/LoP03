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
    // Events appear regularly and every kind shows up somewhere in the sample.
    // M55: a ruined_keep dungeon guarantees the Armory Ghost rite on its first
    // event slot, so the 6 pre-M55 kinds (RestToken since M30) plus ArmoryGhost
    // all appear across the sample. (Town defaults to 1 here, so no Royal
    // Relic leaks in.) M76: the Duckling Peddler's pure-hash replacement
    // (~10% of seeds) joins them; M93 adds Dragonform on the same contract
    // (~8% of seeds; the Surveyor is multi-floor-only and stays absent here)
    // — 9 kinds.
    REQUIRE(eventRooms > 100);
    REQUIRE(kindsSeen.size() == 9);
    REQUIRE(kindsSeen.count(dungeon::RoomEventKind::RestToken) == 1);
    REQUIRE(kindsSeen.count(dungeon::RoomEventKind::ArmoryGhost) == 1);  // the guaranteed rite
    REQUIRE(kindsSeen.count(dungeon::RoomEventKind::DuckPeddler) == 1);  // M76: rare, real
    REQUIRE(kindsSeen.count(dungeon::RoomEventKind::Dragonform) == 1);   // M93: rare, real
    REQUIRE(kindsSeen.count(dungeon::RoomEventKind::Surveyor) == 0);     // M93: never on 1F
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

// --- M93 (generation v17): the Surveyor & Dragonform placements ---------------

#ifdef CRYSTAL_TEST_DATA_DIR
#include "dungeon/ThemeEvents.hpp"
#include "game/Dragonform.hpp"
#include "game/Spoils.hpp"

TEST_CASE("m93: the new events replace only plain rolled slots, deterministically",
          "[events][m93]") {
    const content::ContentDatabase db = loadContent();
    int surveyors = 0;
    int dragonforms = 0;
    for (std::uint64_t seed = 1; seed <= 120; ++seed) {
        const std::vector<dungeon::Dungeon> run =
            dungeon::generateFloors(seed, 6, db, "crystal_mine", 3, 4);
        const std::vector<dungeon::Dungeon> again =
            dungeon::generateFloors(seed, 6, db, "crystal_mine", 3, 4);
        for (std::size_t f = 0; f < run.size(); ++f) {
            for (std::size_t r = 0; r < run[f].rooms.size(); ++r) {
                const dungeon::RoomEvent& ev = run[f].rooms[r].event;
                // Determinism: the same seed reproduces the same events.
                CHECK(ev.kind == again[f].rooms[r].event.kind);
                if (ev.kind == dungeon::RoomEventKind::Surveyor) {
                    ++surveyors;
                    CHECK(run[f].rooms[r].type == dungeon::RoomType::Event);
                    CHECK(ev.goldCost == dungeon::kSurveyorPriceGold);
                }
                if (ev.kind == dungeon::RoomEventKind::Dragonform) {
                    ++dragonforms;
                    CHECK(run[f].rooms[r].type == dungeon::RoomType::Event);
                }
            }
        }
    }
    // Both appear across a wide sample (rates are authored, not asserted
    // exactly here: 25%/floor for the Surveyor, 8%/dungeon for Dragonform).
    CHECK(surveyors > 0);
    CHECK(dragonforms > 0);

    // The Surveyor never appears on a 1-floor run (no fog to sell away).
    for (std::uint64_t seed = 1; seed <= 120; ++seed) {
        const dungeon::Dungeon d = dungeon::generate(seed, 6, db, "crystal_mine", 3);
        for (const dungeon::Room& room : d.rooms) {
            CHECK(room.event.kind != dungeon::RoomEventKind::Surveyor);
        }
    }
}

TEST_CASE("m93: the patrol is composed by the dungeon's rules and pays XP only",
          "[events][m93][patrol]") {
    const content::ContentDatabase db = loadContent();
    const dungeon::EnemyTeam a = dungeon::patrolTeam(db, "ruined_keep", 4, 8, 777ull, 0);
    const dungeon::EnemyTeam b = dungeon::patrolTeam(db, "ruined_keep", 4, 8, 777ull, 0);
    CHECK(a.enemyIds == b.enemyIds);  // reload-honest
    CHECK(a.patrol);
    CHECK(a.name == "Roused Patrol");
    CHECK_FALSE(a.enemyIds.empty());
    CHECK(a.bossId.empty());
    CHECK(a.statScalePct > 100);  // town 4 / depth 8 scaling really applied

    // The Nth patrol differs from the first somewhere across a few indices.
    bool anyDiff = false;
    for (int i = 1; i <= 6 && !anyDiff; ++i) {
        anyDiff = dungeon::patrolTeam(db, "ruined_keep", 4, 8, 777ull, i).enemyIds != a.enemyIds;
    }
    CHECK(anyDiff);

    // Owner decision 7: XP flows, gold never does.
    const BattleSpoils spoils = teamSpoils(a, db);
    CHECK(spoils.xp > 0);
    CHECK(spoils.gold == 0);
}

TEST_CASE("m93: dragonform swaps the party by percentage and swaps it back",
          "[events][m93][dragonform]") {
    const content::ContentDatabase db = loadContent();
    Party party;
    party.members.push_back(createCharacter(*db.findClass("knight"), "Rolan", 20));
    party.members.push_back(createCharacter(*db.findClass("cleric"), "Mira", 20));
    party.members[0].hp = party.members[0].maxHp / 2;  // half health carries over
    party.members[1].hp = 0;                           // the fallen stay fallen
    const Character before0 = party.members[0];
    const Character before1 = party.members[1];

    DragonformStash stash = enterDragonform(party, db);
    REQUIRE(stash.original.size() == 2);
    CHECK(party.members[0].classId == std::string(kDragonformClassId));
    CHECK(party.members[0].name == "Rolan");
    CHECK(party.members[0].level == 20);
    CHECK(party.members[0].weapon.empty());  // a Dragon equips nothing
    // Half health in, (about) half health out — never 0 for a survivor.
    CHECK(party.members[0].hp > 0);
    CHECK(party.members[0].hp <= party.members[0].maxHp);
    CHECK(party.members[1].hp == 0);  // KO carried in as KO

    // The fight: the dragon takes some damage, the fallen one stays down.
    party.members[0].hp = party.members[0].maxHp / 4;

    leaveDragonform(party, stash);
    CHECK(party.members[0].classId == before0.classId);
    CHECK(party.members[0].weapon == before0.weapon);
    CHECK(party.members[0].maxHp == before0.maxHp);
    CHECK(party.members[0].hp > 0);
    CHECK(party.members[0].hp <= before0.maxHp / 3);  // ~a quarter maps back
    CHECK(party.members[1].hp == 0);                  // KO stays KO
    CHECK(party.members[1].classId == before1.classId);

    // The scoring pact: -100 per dragonform fight, itemized.
    score::RunSummary run;
    run.completed = true;
    run.dragonformFights = 2;
    const score::ScoreBreakdown with = score::scoreBreakdown(run);
    run.dragonformFights = 0;
    const score::ScoreBreakdown without = score::scoreBreakdown(run);
    CHECK(with.dragonformPact == 200);
    CHECK(without.dragonformPact == 0);
    CHECK(with.total == std::max(0, without.total - 200));
}

#endif  // CRYSTAL_TEST_DATA_DIR (M93)
