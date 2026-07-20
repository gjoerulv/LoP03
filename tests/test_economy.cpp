#include <catch2/catch_test_macros.hpp>

#include "score/Scoring.hpp"

#ifdef CRYSTAL_TEST_DATA_DIR
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "danger/DangerRating.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/RoomLayout.hpp"
#include "game/Party.hpp"
#endif

using namespace cd;

// --- Pure score-incentive guards (no data needed) -------------------------

namespace {

score::RunSummary baseRun() {
    score::RunSummary r;
    r.completed = true;
    r.battleTurns = 20;
    r.dangerDefeated = 10;
    r.chestsOpened = 2;
    r.treasureGold = 100;
    r.noDeath = true;
    r.escapes = 0;
    return r;
}

}  // namespace

TEST_CASE("economy: fewer battle turns never scores worse", "[economy]") {
    score::RunSummary slow = baseRun();
    score::RunSummary fast = baseRun();
    fast.battleTurns = slow.battleTurns - 1;
    REQUIRE(score::computeScore(fast) > score::computeScore(slow));
}

TEST_CASE("economy: stalling for treasure cannot pay for its turns", "[economy]") {
    // The turn penalty (12/round) must dominate any per-run treasure points a
    // player could add by fighting longer: gold only enters the treasure
    // bonus from chests (fixed per dungeon), and battle gold is excluded, so
    // one extra round can never add points. Guard the coefficient relation
    // that keeps this true even if chest gold were somehow farmed: a chest's
    // maximum gold (30*depth+40 at depth 10 = 340 -> 68 pts) costs less than
    // 6 rounds of penalty to justify — i.e. detours stay bounded, stalling
    // never profits.
    score::RunSummary run = baseRun();
    const int before = score::computeScore(run);
    run.battleTurns += 1;  // one extra round, nothing gained
    REQUIRE(score::computeScore(run) < before);
}

TEST_CASE("economy: escapes always cost score", "[economy]") {
    score::RunSummary clean = baseRun();
    score::RunSummary fled = baseRun();
    fled.escapes = 1;
    REQUIRE(score::computeScore(fled) < score::computeScore(clean));
}

TEST_CASE("economy: an unfinished run scores zero regardless of loot", "[economy]") {
    score::RunSummary r = baseRun();
    r.completed = false;
    r.treasureGold = 100000;
    r.chestsOpened = 99;
    REQUIRE(score::computeScore(r) == 0);
}

#ifdef CRYSTAL_TEST_DATA_DIR

// --- Data-driven battery ---------------------------------------------------

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

Party makeParty(const content::ContentDatabase& db, int level) {
    Party p;
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        if (const content::ClassDef* cls = db.findClass(id)) {
            p.members.push_back(createCharacter(*cls, id, level));
        }
    }
    return p;
}

struct ClearStats {
    bool cleared = false;
    int rounds = 0;
    int xp = 0;      // per party member (XP is granted party-wide)
    int gold = 0;    // battle gold + chest gold
    int chestGold = 0;
};

// Simulates fighting every team in the dungeon in order (gates, guards,
// boss) with a fixed-level party, fresh HP per battle (town-prepared pace).
ClearStats simulateClear(const content::ContentDatabase& db, const dungeon::Dungeon& d,
                         int level) {
    ClearStats s;
    for (const dungeon::EnemyTeam& team : d.teams) {
        battle::Battle b = battle::buildBattle(makeParty(db, level), team, db);
        const battle::SimResult r = battle::simulate(b, db);
        if (r.outcome != battle::Outcome::Victory) {
            return s;  // not cleared
        }
        s.rounds += r.rounds;
        for (const std::string& id : team.enemyIds) {
            if (const content::EnemyDef* e = db.findEnemy(id)) {
                s.xp += e->xpReward;
                s.gold += e->goldReward;
            }
        }
        if (const content::BossDef* boss = db.findBoss(team.bossId)) {
            s.xp += boss->xpReward;
            s.gold += boss->goldReward;
        }
    }
    for (const dungeon::Room& r : d.rooms) {
        if (r.chest.present) {
            s.chestGold += r.chest.gold;
        }
    }
    s.gold += s.chestGold;
    s.cleared = true;
    return s;
}

// Lowest party level (1..kMaxLevel) that clears every team of the dungeon.
int clearingLevel(const content::ContentDatabase& db, const dungeon::Dungeon& d) {
    for (int level = 1; level <= kMaxLevel; ++level) {
        if (simulateClear(db, d, level).cleared) {
            return level;
        }
    }
    return kMaxLevel + 1;
}

int trainPartyCost(int fromLevel) {
    return 4 * (40 + fromLevel * 30);  // TrainingHall::trainingCost x4 members
}

}  // namespace

TEST_CASE("economy: the depth difficulty curve is playable and rising", "[economy]") {
    const content::ContentDatabase db = loadContent();
    // Averaged over a few seeds so one lucky/unlucky roll cannot pass or
    // fail the milestone's curve expectations.
    const std::uint64_t seeds[] = {11, 222, 3333};
    int previousWorst = 0;
    for (int depth : {1, 3, 5, 8}) {
        int worst = 0;
        for (std::uint64_t seed : seeds) {
            const dungeon::Dungeon d = dungeon::generate(seed, depth, db, "ruined_keep");
            const int lv = clearingLevel(db, d);
            worst = std::max(worst, lv);
        }
        INFO("depth " << depth << " worst clearing level " << worst);
        REQUIRE(worst <= kMaxLevel);            // every depth is clearable
        REQUIRE(worst >= previousWorst);        // difficulty never inverts
        previousWorst = worst;
    }
    // Depth 1 must be clearable by a fresh, ungeared party.
    for (std::uint64_t seed : seeds) {
        const dungeon::Dungeon d = dungeon::generate(seed, 1, db, "ruined_keep");
        REQUIRE(simulateClear(db, d, 1).cleared);
    }
}

TEST_CASE("economy: training is a real sink, battles a real income", "[economy]") {
    const content::ContentDatabase db = loadContent();
    const dungeon::Dungeon d = dungeon::generate(2024, 1, db, "ruined_keep");
    const ClearStats s = simulateClear(db, d, 3);
    REQUIRE(s.cleared);
    // One depth-1 clear must not fund training the whole party even one
    // level at mid game (level 5) — otherwise gold trivially converts to
    // power and the Training Hall stops being a decision.
    REQUIRE(s.gold < trainPartyCost(5));
    // But income is not vanishing either: a clear funds at least one single
    // early training (the two systems keep distinct roles).
    REQUIRE(s.gold >= 40 + 1 * 30);
    // Battle XP alone levels a fresh party out of level 1 within one clear.
    REQUIRE(s.xp >= xpToNext(1));
}

TEST_CASE("economy: class identity survives leveling", "[economy]") {
    const content::ContentDatabase db = loadContent();
    for (int level : {1, 10, 25, 50}) {
        const Party p = makeParty(db, level);
        const Character* knight = &p.members[0];
        const Character* ranger = &p.members[1];
        const Character* mage = &p.members[2];
        const Character* cleric = &p.members[3];
        INFO("level " << level);
        // Roles must not converge as growth accumulates.
        REQUIRE(knight->stats.defense > mage->stats.defense);
        REQUIRE(knight->stats.attack > mage->stats.attack);
        REQUIRE(mage->stats.magic > knight->stats.magic);
        REQUIRE(ranger->stats.speed > knight->stats.speed);
        REQUIRE(cleric->stats.magic > ranger->stats.magic);
    }
}

// On-demand analysis table for the milestone note and owner review:
//   crystal_tests.exe "[economy-report]" -s
TEST_CASE("economy report: depth/level/income battery", "[.][economy-report]") {
    const content::ContentDatabase db = loadContent();
    std::cout << "depth | clearing level (worst of 3 seeds) | rounds | xp/member | gold(total) "
                 "| chest gold | train-4 cost @clearing lv\n";
    for (int depth : {1, 2, 3, 4, 5, 6, 8, 10}) {
        int worstLv = 0;
        ClearStats sample;
        for (std::uint64_t seed : {11ull, 222ull, 3333ull}) {
            const dungeon::Dungeon d = dungeon::generate(seed, depth, db, "ruined_keep");
            const int lv = clearingLevel(db, d);
            if (lv > worstLv) {
                worstLv = lv;
                sample = simulateClear(db, d, std::min(lv, kMaxLevel));
            }
        }
        std::cout << depth << " | " << worstLv << " | " << sample.rounds << " | " << sample.xp
                  << " | " << sample.gold << " | " << sample.chestGold << " | "
                  << trainPartyCost(worstLv) << "\n";
    }
    SUCCEED();
}

// Machine-readable balance report (M23): progression bands per depth plus
// outlier encounters (teams whose simulated rounds deviate hard from the
// per-dungeon median at the appropriate party level). Reproducible JSON:
//   crystal_tests.exe "[sim-report]" -s > sim_report.json
TEST_CASE("sim report: progression bands and outlier encounters (JSON)",
          "[.][sim-report]") {
    const content::ContentDatabase db = loadContent();
    const char* themes[] = {"ruined_keep", "crystal_mine", "hollow_forest"};
    std::cout << "{\n  \"generationVersion\": " << dungeon::kGenerationVersion
              << ",\n  \"bands\": [\n";
    bool firstBand = true;
    for (int depth : {1, 2, 3, 4, 5, 6, 8, 10}) {
        int worstLv = 0;
        ClearStats sample;
        for (std::uint64_t seed : {11ull, 222ull, 3333ull}) {
            const dungeon::Dungeon d = dungeon::generate(seed, depth, db, "ruined_keep");
            const int lv = clearingLevel(db, d);
            if (lv > worstLv) {
                worstLv = lv;
                sample = simulateClear(db, d, std::min(lv, kMaxLevel));
            }
        }
        std::cout << (firstBand ? "" : ",\n") << "    {\"depth\": " << depth
                  << ", \"clearingLevel\": " << worstLv << ", \"rounds\": " << sample.rounds
                  << ", \"xpPerMember\": " << sample.xp << ", \"gold\": " << sample.gold
                  << "}";
        firstBand = false;
    }
    std::cout << "\n  ],\n  \"outliers\": [\n";
    // Outliers: per theme at a mid depth, any team needing > 2x the median
    // rounds (or unwinnable) at that dungeon's clearing level.
    bool firstOutlier = true;
    for (const char* theme : themes) {
        for (std::uint64_t seed : {11ull, 222ull, 3333ull}) {
            const dungeon::Dungeon d = dungeon::generate(seed, 5, db, theme);
            const int lv = std::min(clearingLevel(db, d), kMaxLevel);
            std::vector<int> rounds;
            for (const dungeon::EnemyTeam& team : d.teams) {
                battle::Battle b = battle::buildBattle(makeParty(db, lv), team, db);
                const battle::SimResult r = battle::simulate(b, db);
                rounds.push_back(r.outcome == battle::Outcome::Victory ? r.rounds : 999);
            }
            std::vector<int> sorted = rounds;
            std::sort(sorted.begin(), sorted.end());
            const int median = sorted[sorted.size() / 2];
            for (std::size_t i = 0; i < d.teams.size(); ++i) {
                if (rounds[i] > median * 2) {
                    std::cout << (firstOutlier ? "" : ",\n") << "    {\"theme\": \"" << theme
                              << "\", \"seed\": " << seed << ", \"team\": \""
                              << d.teams[i].name << "\", \"rounds\": " << rounds[i]
                              << ", \"medianRounds\": " << median << "}";
                    firstOutlier = false;
                }
            }
        }
    }
    std::cout << "\n  ]\n}\n";
    SUCCEED();
}

#endif  // CRYSTAL_TEST_DATA_DIR
