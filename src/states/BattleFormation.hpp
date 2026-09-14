#pragma once

#include <algorithm>
#include <utility>
#include <vector>


// M101 (owner request): enemies fill the battlefield CENTER-OUT — the first
// enemy unit takes the middle row, then center-top, center-bottom, top,
// bottom. buildBattle constructs the boss as the first enemy unit, so a boss
// always stands dead-center; plain teams lead with their first member there.
// Presentation ONLY: the sim's unit order is untouched (determinism, and the
// Simulator never reads a screen row). Rows are fixed per unit for the whole
// battle — the dead do not reflow. Beyond five (the M75 summon clone as a
// sixth), rows continue downward exactly as the old sequential stack did.
//
// Pure and raylib-free so tests/test_battle_formation.cpp can pin the shape.

namespace cd::battle_ui {

inline int enemyRowSlot(int enemyOrdinal) {
    constexpr int kCenterOut[5] = {2, 1, 3, 0, 4};
    return (enemyOrdinal >= 0 && enemyOrdinal < 5) ? kCenterOut[enemyOrdinal] : enemyOrdinal;
}

// Vertical pitch between enemy rows, and the extra headroom the two rows
// ABOVE the boss's center slot give up when a boss is on the field: a 36px
// boss sprite reaches 12px higher into its cell than a 24px enemy, so the
// neighbour's sprite bottom and HP meter otherwise cut across its crown
// (owner-reported overlap, 2026-08-17). Lifting slots 0 and 1 by 10px keeps
// their own 34px spacing while clearing the boss's head; slots below need
// nothing (the boss's meter sits inside the normal pitch).
inline constexpr int kEnemyRowPitch = 34;
inline constexpr int kBossHeadroom = 10;

// The y offset of an enemy ordinal's row from the enemy base line.
inline int enemyRowOffset(int enemyOrdinal, bool bossOnField) {
    const int slot = enemyRowSlot(enemyOrdinal);
    return slot * kEnemyRowPitch - ((bossOnField && slot < 2) ? kBossHeadroom : 0);
}

// M115: the bounds-aware generalization. A unit's vertical ENVELOPE around
// its row line: `above` = how far its sprite reaches above the line (height
// - 16, since a sprite anchors bottom-centre at line + 16 and grows upward),
// `below` = how far its cell reaches beneath (the HP meter's bottom, 22 —
// the enemy status column stays inside that, M114 keeps it so). Two rows
// that are adjacent on screen need pitch >= below(upper) + above(lower) +
// kEnvelopeGap; when the plain pitch falls short the upper row AND every row
// above it lift by the deficit, keeping their own pitch (the M101 headroom
// generalized: 24 over 36 gives exactly kBossHeadroom; two 36s give more).
// A block that would climb above y 0 is pushed down as a whole instead.
struct UnitEnvelope {
    int above = 8;   // a 24px sprite
    int below = 22;  // the meter's bottom
};
inline constexpr int kEnvelopeGap = 2;

// Absolute screen y of every enemy ORDINAL's row line (index = ordinal, the
// same seating enemyRowSlot gives), from `baseY`. Pure.
inline std::vector<int> enemyRowYs(const std::vector<UnitEnvelope>& byOrdinal, int baseY) {
    const int n = static_cast<int>(byOrdinal.size());
    std::vector<int> ys(static_cast<std::size_t>(n), 0);
    if (n == 0) {
        return ys;
    }
    // Occupied slots in visual order, with the ordinal seated there.
    std::vector<std::pair<int, int>> seats;  // (slot, ordinal)
    for (int i = 0; i < n; ++i) {
        seats.emplace_back(enemyRowSlot(i), i);
    }
    std::sort(seats.begin(), seats.end());
    std::vector<int> y(seats.size());
    for (std::size_t k = 0; k < seats.size(); ++k) {
        y[k] = seats[k].first * kEnemyRowPitch;
    }
    // Bottom-up: a short pair lifts the upper row and everything above it.
    for (std::size_t k = seats.size() - 1; k >= 1; --k) {
        const UnitEnvelope& upper = byOrdinal[static_cast<std::size_t>(seats[k - 1].second)];
        const UnitEnvelope& lower = byOrdinal[static_cast<std::size_t>(seats[k].second)];
        const int need = upper.below + lower.above + kEnvelopeGap;
        const int have = y[k] - y[k - 1];
        if (have < need) {
            const int deficit = need - have;
            for (std::size_t t = 0; t < k; ++t) {
                y[t] -= deficit;
            }
        }
        if (k == 1) {
            break;
        }
    }
    // Never above the screen's top: push the whole block down instead.
    int minTop = 0;
    for (std::size_t k = 0; k < seats.size(); ++k) {
        const int top = baseY + y[k] - byOrdinal[static_cast<std::size_t>(seats[k].second)].above;
        minTop = std::min(minTop, top);
    }
    for (std::size_t k = 0; k < seats.size(); ++k) {
        ys[static_cast<std::size_t>(seats[k].second)] = baseY + y[k] - minTop;
    }
    return ys;
}

}  // namespace cd::battle_ui
