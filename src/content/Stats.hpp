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

// M52: the one home for the per-field enemy stat scale (×pct/100, integer floor
// per field). buildBattle applies this to every scaled combatant; the bestiary
// uses it to show a foe's strongest-context stats. Keeping the multiply here
// means the two can never drift. Pure and header-only. (DangerRating scales the
// summed threat scalar, not a StatBlock, so it deliberately does not route
// through this — see the M52 note.)
inline StatBlock scaledStats(const StatBlock& base, int pct) {
    StatBlock s;
    s.maxHp = base.maxHp * pct / 100;
    s.attack = base.attack * pct / 100;
    s.magic = base.magic * pct / 100;
    s.defense = base.defense * pct / 100;
    s.speed = base.speed * pct / 100;
    return s;
}

}  // namespace cd::content
