#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "game/WorldLadder.hpp"
#include "score/Scoring.hpp"

// M32 town ladder: the pure ladder curves, the (depth, town) stat combination,
// the visible score bonus, and the guarantee that town 1 leaves generation and
// scoring byte-identical to pre-M32 while higher towns scale stats/score only.

using namespace cd;

namespace {

content::ContentDatabase loadShippedContent() {
    content::ContentDatabase db;
    content::LoadReport report;
    REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, report));
    return db;
}

}  // namespace

TEST_CASE("town ladder: stat and score curves match the owner ladder", "[townladder]") {
    CHECK(kTownCount == 7);
    // Enemy stat multiplier: +0/25/50/75/100/150/200 %.
    CHECK(townScalePct(1) == 100);
    CHECK(townScalePct(2) == 125);
    CHECK(townScalePct(5) == 200);
    CHECK(townScalePct(6) == 250);
    CHECK(townScalePct(7) == 300);
    // Score bonus: +0/10/20/30/40/50/100 % (town 6 is 50, not a duplicate 100).
    CHECK(townScoreBonusPct(1) == 0);
    CHECK(townScoreBonusPct(5) == 40);
    CHECK(townScoreBonusPct(6) == 50);
    CHECK(townScoreBonusPct(7) == 100);
}

TEST_CASE("town ladder: clamp, combine, and unlock are well-behaved", "[townladder]") {
    CHECK(clampTown(0) == 1);
    CHECK(clampTown(1) == 1);
    CHECK(clampTown(7) == 7);
    CHECK(clampTown(99) == 7);

    // Town 1 is the identity on the depth-derived base; higher towns multiply.
    CHECK(combineTownScale(118, 1) == 118);
    CHECK(combineTownScale(100, 2) == 125);
    CHECK(combineTownScale(118, 5) == 236);  // 118 * 200 / 100

    // Unlock never lowers, caps at kTownCount, opens exactly town+1.
    CHECK(unlockAfterClear(1, 1) == 2);
    CHECK(unlockAfterClear(3, 3) == 4);
    CHECK(unlockAfterClear(5, 2) == 5);   // clearing an earlier town doesn't lower
    CHECK(unlockAfterClear(1, 7) == 7);   // capped
    CHECK(unlockAfterClear(4, 6) == 7);
}

TEST_CASE("town ladder: score town bonus applies to the whole subtotal, town 1 unchanged",
          "[townladder]") {
    score::RunSummary run;
    run.completed = true;
    run.battleTurns = 20;
    run.dangerDefeated = 10;
    run.chestsOpened = 3;
    run.treasureGold = 400;
    run.noDeath = true;

    const score::ScoreBreakdown base = score::scoreBreakdown(run);  // townBonusPct 0
    REQUIRE(base.townBonus == 0);
    REQUIRE(base.total > 0);

    run.townBonusPct = 20;
    const score::ScoreBreakdown boosted = score::scoreBreakdown(run);
    // The subtotal is unchanged; the bonus is 20% of it, added on top.
    CHECK(boosted.townBonus == base.total * 20 / 100);
    CHECK(boosted.total == base.total + boosted.townBonus);

    run.townBonusPct = 100;  // town 7
    const score::ScoreBreakdown top = score::scoreBreakdown(run);
    CHECK(top.townBonus == base.total);
    CHECK(top.total == base.total * 2);
}

TEST_CASE("town ladder: town 1 generation equals the depth-only scaling", "[townladder]") {
    const content::ContentDatabase db = loadShippedContent();
    const int depth = 8;  // > scaleStartDepth so depth scaling is nonzero
    const dungeon::Dungeon def = dungeon::generate(424242, depth, db, "ruined_keep");
    const dungeon::Dungeon one = dungeon::generate(424242, depth, db, "ruined_keep", 1);

    REQUIRE(def.teams.size() == one.teams.size());
    REQUIRE(!def.teams.empty());
    const int expected = 100 + db.composition().statScalePct(depth);
    CHECK(def.town == 1);
    for (std::size_t i = 0; i < def.teams.size(); ++i) {
        CHECK(def.teams[i].statScalePct == expected);            // identity multiplier
        CHECK(def.teams[i].statScalePct == one.teams[i].statScalePct);
        CHECK(def.teams[i].enemyIds == one.teams[i].enemyIds);   // same RNG stream
    }
}

TEST_CASE("town ladder: higher town scales stats, not topology; deterministic", "[townladder]") {
    const content::ContentDatabase db = loadShippedContent();
    const int depth = 8;
    const dungeon::Dungeon t1 = dungeon::generate(424242, depth, db, "ruined_keep", 1);
    const dungeon::Dungeon t3 = dungeon::generate(424242, depth, db, "ruined_keep", 3);
    const dungeon::Dungeon t3b = dungeon::generate(424242, depth, db, "ruined_keep", 3);

    REQUIRE(t1.teams.size() == t3.teams.size());
    CHECK(t3.town == 3);
    for (std::size_t i = 0; i < t1.teams.size(); ++i) {
        // Town does not touch the RNG stream: same enemies, boss flags, order.
        CHECK(t1.teams[i].enemyIds == t3.teams[i].enemyIds);
        CHECK(t1.teams[i].isBoss == t3.teams[i].isBoss);
        // Town 3 multiplies the stat scale (x1.50) over the town-1 value.
        CHECK(t3.teams[i].statScalePct == combineTownScale(t1.teams[i].statScalePct, 3));
        CHECK(t3.teams[i].statScalePct > t1.teams[i].statScalePct);
        // Fully deterministic per (seed, depth, town).
        CHECK(t3.teams[i].statScalePct == t3b.teams[i].statScalePct);
        CHECK(t3.teams[i].enemyIds == t3b.teams[i].enemyIds);
    }
}
