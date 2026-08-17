// M101 — the center-out enemy formation: a pure row mapping (presentation
// only; the sim's unit order is untouched). The owner's spec, verbatim:
// "enemy should spawn in this order: center -> center-top -> center-bottom ->
// top -> bottom", with the boss (always the first enemy unit, buildBattle's
// order) at the center, always.

#include <catch2/catch_test_macros.hpp>

#include <set>

#include "states/BattleFormation.hpp"

using cd::battle_ui::enemyRowSlot;

TEST_CASE("formation: the owner's fill order, verbatim", "[battle][formation]") {
    CHECK(enemyRowSlot(0) == 2);  // center — the boss's seat
    CHECK(enemyRowSlot(1) == 1);  // center-top
    CHECK(enemyRowSlot(2) == 3);  // center-bottom
    CHECK(enemyRowSlot(3) == 0);  // top
    CHECK(enemyRowSlot(4) == 4);  // bottom
}

TEST_CASE("formation: every count seats distinct rows", "[battle][formation]") {
    // For any live enemy count 1..6 the assigned rows never collide — two
    // units can never share a screen row.
    for (int count = 1; count <= 6; ++count) {
        std::set<int> rows;
        for (int i = 0; i < count; ++i) {
            rows.insert(enemyRowSlot(i));
        }
        CHECK(rows.size() == static_cast<std::size_t>(count));
    }
}

TEST_CASE("formation: past five, rows continue downward (the summon clone)",
          "[battle][formation]") {
    // The M75 clone rides as a sixth enemy unit; it takes row 5 exactly as the
    // old sequential stack would have, so nothing changes for it.
    CHECK(enemyRowSlot(5) == 5);
    CHECK(enemyRowSlot(6) == 6);
    // Defensive: a negative ordinal passes through rather than indexing.
    CHECK(enemyRowSlot(-1) == -1);
}
