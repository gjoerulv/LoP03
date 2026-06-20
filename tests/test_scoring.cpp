#include <catch2/catch_test_macros.hpp>

#include "score/Scoring.hpp"

using namespace cd::score;

namespace {
RunSummary completedRun() {
    RunSummary r;
    r.completed = true;
    r.battleTurns = 10;
    r.dangerDefeated = 0;
    r.chestsOpened = 0;
    r.treasureGold = 0;
    r.noDeath = true;
    r.escapes = 0;
    return r;
}
}  // namespace

TEST_CASE("scoring: an unfinished run scores zero", "[scoring]") {
    RunSummary r = completedRun();
    r.completed = false;
    REQUIRE(computeScore(r) == 0);
}

TEST_CASE("scoring: a completed run scores positive", "[scoring]") {
    REQUIRE(computeScore(completedRun()) > 0);
}

TEST_CASE("scoring: fewer battle turns scores higher", "[scoring]") {
    RunSummary few = completedRun();
    few.battleTurns = 5;
    RunSummary many = completedRun();
    many.battleTurns = 20;
    REQUIRE(computeScore(few) > computeScore(many));
}

TEST_CASE("scoring: bonuses move the score as expected", "[scoring]") {
    const int base = computeScore(completedRun());

    RunSummary died = completedRun();
    died.noDeath = false;
    REQUIRE(computeScore(died) < base);  // losing the no-death bonus

    RunSummary chests = completedRun();
    chests.chestsOpened = 3;
    REQUIRE(computeScore(chests) > base);

    RunSummary danger = completedRun();
    danger.dangerDefeated = 10;
    REQUIRE(computeScore(danger) > base);

    RunSummary escaped = completedRun();
    escaped.escapes = 2;
    REQUIRE(computeScore(escaped) < base);  // escape penalty
}

TEST_CASE("scoring: score never goes negative", "[scoring]") {
    RunSummary slow = completedRun();
    slow.battleTurns = 100000;
    REQUIRE(computeScore(slow) == 0);
}

TEST_CASE("scoring: breakdown total matches computeScore", "[scoring]") {
    RunSummary r = completedRun();
    r.chestsOpened = 2;
    r.dangerDefeated = 6;
    r.treasureGold = 100;
    REQUIRE(scoreBreakdown(r).total == computeScore(r));
}
