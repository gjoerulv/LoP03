#pragma once

// Core stat structures shared by classes, enemies, and equipment. Plain data.

namespace cd::content {

// Absolute stat values (a character's level-1 base, an enemy's stats, or the
// flat bonus an equipment piece grants).
struct StatBlock {
    int maxHp = 0;
    int attack = 0;
    int magic = 0;
    int defense = 0;
    int speed = 0;
};

// Per-level stat growth for a class. Fractional values accumulate across levels.
struct StatGrowth {
    float maxHp = 0.0f;
    float attack = 0.0f;
    float magic = 0.0f;
    float defense = 0.0f;
    float speed = 0.0f;
};

}  // namespace cd::content
