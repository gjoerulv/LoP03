// M101 — the center-out enemy formation: a pure row mapping (presentation
// only; the sim's unit order is untouched). The owner's spec, verbatim:
// "enemy should spawn in this order: center -> center-top -> center-bottom ->
// top -> bottom", with the boss (always the first enemy unit, buildBattle's
// order) at the center, always.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <vector>

#include "states/BattleFormation.hpp"

using cd::battle_ui::enemyRowOffset;
using cd::battle_ui::enemyRowSlot;
using cd::battle_ui::kBossHeadroom;
using cd::battle_ui::kEnemyRowPitch;

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

TEST_CASE("formation: a boss's crown gets headroom, plain fights do not",
          "[battle][formation]") {
    // Without a boss the rows sit on the plain pitch.
    for (int i = 0; i < 6; ++i) {
        CHECK(enemyRowOffset(i, false) == enemyRowSlot(i) * kEnemyRowPitch);
    }
    // With a boss on the field the two visual rows ABOVE its center seat lift
    // by the headroom; the center and everything below hold still.
    CHECK(enemyRowOffset(3, true) == 0 * kEnemyRowPitch - kBossHeadroom);  // top
    CHECK(enemyRowOffset(1, true) == 1 * kEnemyRowPitch - kBossHeadroom);  // center-top
    CHECK(enemyRowOffset(0, true) == 2 * kEnemyRowPitch);                  // the boss
    CHECK(enemyRowOffset(2, true) == 3 * kEnemyRowPitch);
    CHECK(enemyRowOffset(4, true) == 4 * kEnemyRowPitch);

    // The geometric contract behind the owner's overlap report: a 24px enemy
    // cell (sprite from rowY-8 plus its meter to rowY+22) must end above a
    // 36px boss sprite's top (rowY-20). The gap the headroom buys:
    const int bossTop = enemyRowOffset(0, true) - 20;
    const int aboveCellBottom = enemyRowOffset(1, true) + 22;
    CHECK(aboveCellBottom < bossTop);

    // And the lifted rows keep their own pitch, so they cannot collide.
    std::vector<int> ys;
    for (int i = 0; i < 5; ++i) {
        ys.push_back(enemyRowOffset(i, true));
    }
    std::sort(ys.begin(), ys.end());
    for (std::size_t i = 1; i < ys.size(); ++i) {
        CHECK(ys[i] - ys[i - 1] >= 30);  // a normal cell is 30px tall
    }
}
