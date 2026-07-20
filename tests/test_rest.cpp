#include <catch2/catch_test_macros.hpp>

#include "game/Party.hpp"

// M30 paid rest: the inn cost scales with the highest party level and clamps,
// so a full rest stays a real decision as income grows. Pure and headless.

using namespace cd;

namespace {

Party partyAtLevel(int level) {
    Party p;
    Character c;
    c.level = level;
    p.members.push_back(c);
    return p;
}

}  // namespace

TEST_CASE("rest: cost is base at level 1 and adds per level", "[rest]") {
    REQUIRE(restCost(partyAtLevel(1)) == kRestCostBase);
    REQUIRE(restCost(partyAtLevel(2)) == kRestCostBase + kRestCostPerLevel);
    REQUIRE(restCost(partyAtLevel(5)) == kRestCostBase + 4 * kRestCostPerLevel);
}

TEST_CASE("rest: the highest member's level drives the cost", "[rest]") {
    Party mixed;
    Character a;
    a.level = 1;
    Character b;
    b.level = 10;
    mixed.members = {a, b};
    REQUIRE(restCost(mixed) == restCost(partyAtLevel(10)));
}

TEST_CASE("rest: cost clamps to the cap and never underflows", "[rest]") {
    REQUIRE(restCost(partyAtLevel(kMaxLevel)) == kRestCostMax);
    REQUIRE(restCost(partyAtLevel(100000)) == kRestCostMax);
    Party empty;
    REQUIRE(restCost(empty) == kRestCostBase);  // highestLevel 0 -> clamped to 1
}

TEST_CASE("rest: cost is non-decreasing in level and stays within bounds", "[rest]") {
    int previous = 0;
    for (int lv = 1; lv <= kMaxLevel; ++lv) {
        const int cost = restCost(partyAtLevel(lv));
        REQUIRE(cost >= previous);
        REQUIRE(cost >= kRestCostBase);
        REQUIRE(cost <= kRestCostMax);
        previous = cost;
    }
}
