#include <catch2/catch_test_macros.hpp>

#include "game/StakesLadder.hpp"
#include "score/Scoring.hpp"

// M33 stakes escalation: the pure state machine (raise / repeat / cap / reset /
// town-first comparison) and the visible score penalty. Score-0 runs "not moving
// the baseline" is a wiring guard in completeDungeon (afterCompletedRun is only
// called for scoring completions); the transitions themselves are pinned here.

using namespace cd;

TEST_CASE("stakes: a fresh state means the first run always raises", "[stakes]") {
    const StakesState s;  // {0, 0, 0}
    CHECK(stakesRaised(s, 1, 1));
    CHECK(stakesRaised(s, 1, 20));
    CHECK(stakesRaised(s, 7, 1));
    CHECK(stakesPenaltyPct(s, 1, 1) == 0);
}

TEST_CASE("stakes: repeating a stake grows -15% per step to a -90% cap", "[stakes]") {
    StakesState s = afterCompletedRun(StakesState{}, 3, 5);  // first run raises
    REQUIRE(s.penaltySteps == 0);
    REQUIRE(s.prevTown == 3);
    REQUIRE(s.prevDepth == 5);

    // Each subsequent repeat of (3,5) incurs one more step, capped at 90.
    const int expected[] = {15, 30, 45, 60, 75, 90, 90, 90};
    for (int p : expected) {
        CHECK(stakesPenaltyPct(s, 3, 5) == p);
        s = afterCompletedRun(s, 3, 5);
    }
    CHECK(s.penaltySteps == kStakesPenaltyMaxSteps);
    CHECK(stakesPenaltyPct(s, 3, 5) == kStakesPenaltyCapPct);
}

TEST_CASE("stakes: raising the stakes resets the penalty to zero", "[stakes]") {
    StakesState s = afterCompletedRun(StakesState{}, 3, 5);
    s = afterCompletedRun(s, 3, 5);  // repeat -> steps 1
    s = afterCompletedRun(s, 3, 5);  // repeat -> steps 2
    REQUIRE(s.penaltySteps == 2);

    CHECK(stakesPenaltyPct(s, 3, 6) == 0);  // deeper raises -> no penalty
    s = afterCompletedRun(s, 3, 6);         // raise
    CHECK(s.penaltySteps == 0);
    CHECK(s.prevDepth == 6);
    // And a fresh repeat at the new baseline starts the ladder over at 15%.
    CHECK(stakesPenaltyPct(s, 3, 6) == 15);
}

TEST_CASE("stakes: comparison is town-first (lexicographic)", "[stakes]") {
    StakesState s;
    s.prevTown = 2;
    s.prevDepth = 5;
    s.penaltySteps = 3;

    CHECK(stakesRaised(s, 3, 1));               // higher town beats any depth
    CHECK(stakesPenaltyPct(s, 3, 1) == 0);
    CHECK_FALSE(stakesRaised(s, 1, 20));        // lower town, even deeper, does not raise
    CHECK(stakesPenaltyPct(s, 1, 20) == (3 + 1) * kStakesPenaltyPerStep);  // 60
    CHECK_FALSE(stakesRaised(s, 2, 5));         // identical stake
    CHECK_FALSE(stakesRaised(s, 2, 4));         // same town, shallower
    CHECK(stakesRaised(s, 2, 6));               // same town, deeper
}

TEST_CASE("stakes: clampStakesSteps keeps steps in range", "[stakes]") {
    CHECK(clampStakesSteps(-3) == 0);
    CHECK(clampStakesSteps(0) == 0);
    CHECK(clampStakesSteps(kStakesPenaltyMaxSteps) == kStakesPenaltyMaxSteps);
    CHECK(clampStakesSteps(999) == kStakesPenaltyMaxSteps);
}

TEST_CASE("stakes: penalty subtracts from the subtotal, after the town bonus", "[stakes]") {
    score::RunSummary run;
    run.completed = true;
    run.battleTurns = 20;
    run.dangerDefeated = 10;
    run.chestsOpened = 3;
    run.treasureGold = 400;
    run.noDeath = true;

    const score::ScoreBreakdown base = score::scoreBreakdown(run);  // no town/penalty
    REQUIRE(base.stakesPenalty == 0);
    REQUIRE(base.total > 0);  // base.total == the positive subtotal here

    run.stakesPenaltyPct = 30;
    const score::ScoreBreakdown pen = score::scoreBreakdown(run);
    CHECK(pen.stakesPenalty == base.total * 30 / 100);
    CHECK(pen.total == base.total - pen.stakesPenalty);

    // Combined with a town bonus: both are percentages of the same subtotal.
    run.townBonusPct = 50;
    const score::ScoreBreakdown both = score::scoreBreakdown(run);
    CHECK(both.townBonus == base.total * 50 / 100);
    CHECK(both.stakesPenalty == base.total * 30 / 100);
    CHECK(both.total == base.total + both.townBonus - both.stakesPenalty);

    // A -90% penalty at town 1 (no bonus) leaves ~10% of the subtotal.
    run.townBonusPct = 0;
    run.stakesPenaltyPct = 90;
    const score::ScoreBreakdown floorRun = score::scoreBreakdown(run);
    CHECK(floorRun.total == base.total - base.total * 90 / 100);
    CHECK(floorRun.total > 0);
}
