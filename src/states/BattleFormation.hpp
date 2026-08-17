#pragma once

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

}  // namespace cd::battle_ui
