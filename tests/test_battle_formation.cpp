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
using cd::battle_ui::enemyRowYs;
using cd::battle_ui::kBossHeadroom;
using cd::battle_ui::kEnemyRowPitch;
using cd::battle_ui::kEnvelopeGap;
using cd::battle_ui::UnitEnvelope;

namespace {
UnitEnvelope env(int height) { return UnitEnvelope{height - 16, 22}; }
// The spans an enemy cell occupies around its row line: sprite [top, line+16)
// and meter [line+17, line+22); brackets add one pixel each way.
int spriteTop(int lineY, int height) { return lineY + 16 - height; }
int cellBottom(int lineY) { return lineY + 22; }
}  // namespace

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

TEST_CASE("formation: the envelope layout reproduces the M101 pins", "[battle][formation][dragon]") {
    // All 24px units: the plain pitch, every count.
    for (int count = 1; count <= 6; ++count) {
        std::vector<UnitEnvelope> e(static_cast<std::size_t>(count), env(24));
        const std::vector<int> ys = enemyRowYs(e, 36);
        for (int i = 0; i < count; ++i) {
            CHECK(ys[static_cast<std::size_t>(i)] == 36 + enemyRowOffset(i, false));
        }
    }
    // One 36px boss at the centre with 24px minions: exactly the old headroom.
    for (int count = 1; count <= 5; ++count) {
        std::vector<UnitEnvelope> e(static_cast<std::size_t>(count), env(24));
        e[0] = env(36);
        const std::vector<int> ys = enemyRowYs(e, count >= 5 ? 20 : 36);
        for (int i = 0; i < count; ++i) {
            CHECK(ys[static_cast<std::size_t>(i)] ==
                  (count >= 5 ? 20 : 36) + enemyRowOffset(i, true));
        }
    }
    CHECK(24 - 16 + 22 + kEnvelopeGap == 32);          // 24 over 24: within the pitch
    CHECK(36 - 16 + 22 + kEnvelopeGap - kEnemyRowPitch == kBossHeadroom);  // 24 over 36
}

TEST_CASE("formation: the Dragon and its clone never overlap", "[battle][formation][dragon]") {
    // Two 36px units: the Dragon (ordinal 0, centre) and its clone (ordinal 1,
    // centre-top). Sprite, meter and bracket spans stay apart by the gap.
    const std::vector<UnitEnvelope> e = {env(36), env(36)};
    const std::vector<int> ys = enemyRowYs(e, 36);
    const int cloneLine = ys[1];
    const int dragonLine = ys[0];
    CHECK(cloneLine < dragonLine);
    CHECK(spriteTop(dragonLine, 36) - cellBottom(cloneLine) >= kEnvelopeGap);
    CHECK(spriteTop(dragonLine, 36) - 1 > cellBottom(cloneLine));  // the bracket's top pixel
    CHECK(spriteTop(cloneLine, 36) >= 0);
    // Deterministic and stable: the same envelopes give the same rows.
    CHECK(enemyRowYs(e, 36) == ys);
    // With three 24px minions beneath as well (a mixed roster of five),
    // nothing overlaps anywhere and nothing climbs above the screen.
    const std::vector<UnitEnvelope> five = {env(36), env(36), env(24), env(24), env(24)};
    const std::vector<int> ys5 = enemyRowYs(five, 20);
    std::vector<std::pair<int, int>> spans;  // (top, bottom) per unit
    for (std::size_t i = 0; i < five.size(); ++i) {
        spans.emplace_back(spriteTop(ys5[i], five[i].above + 16), cellBottom(ys5[i]));
        CHECK(spans.back().first >= 0);
    }
    std::sort(spans.begin(), spans.end());
    for (std::size_t i = 1; i < spans.size(); ++i) {
        CHECK(spans[i].first - spans[i - 1].second >= kEnvelopeGap);
    }
}

TEST_CASE("formation: a tall unit in the top slot pushes the block down, never off-screen",
          "[battle][formation][dragon]") {
    // Five enemies (base 20) with a 36px unit seated at the top row (ordinal 3).
    std::vector<UnitEnvelope> e(5, env(24));
    e[3] = env(36);
    const std::vector<int> ys = enemyRowYs(e, 20);
    for (std::size_t i = 0; i < e.size(); ++i) {
        CHECK(spriteTop(ys[i], e[i].above + 16) >= 0);
    }
    // Screen order is still the M101 order (top, centre-top, centre, ...).
    CHECK(ys[3] < ys[1]);
    CHECK(ys[1] < ys[0]);
    CHECK(ys[0] < ys[2]);
    CHECK(ys[2] < ys[4]);
    // A sixth-slot clone under a boss still sits below everything.
    std::vector<UnitEnvelope> six(6, env(24));
    six[0] = env(36);
    six[5] = env(36);
    const std::vector<int> ys6 = enemyRowYs(six, 20);
    CHECK(ys6[5] > ys6[4]);
    CHECK(spriteTop(ys6[5], 36) - cellBottom(ys6[4]) >= kEnvelopeGap);
}

