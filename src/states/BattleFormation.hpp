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

}  // namespace cd::battle_ui
