#pragma once

#include <algorithm>

// M122: the heal arithmetic, in ONE place. Battle::useSkill and the out-of-
// battle field cast (game/FieldSkills.hpp, the Party panel's dungeon healing)
// both call these, so a heal cast from the menu restores exactly what the same
// cast restores in a fight. Pure integer math lifted verbatim from Battle.cpp -
// extracting it changed no battle result (the battle tests pin that).

namespace cd::battle {

// A heal's base amount: the skill's power plus half the caster's Magic.
inline int healBase(int casterMagic, int power) { return power + casterMagic / 2; }

// M63 (Devotion): heals this caster casts restore +healCastPct%.
inline int healWithCastBonus(int amount, int healCastPct) {
    return healCastPct > 0 ? amount * (100 + healCastPct) / 100 : amount;
}

// M43/M63: the HP a revive-capable heal raises a fallen ally at - the higher
// of the skill's own share and the caster's Blessed Renew share, never 0.
inline int reviveHp(int targetMaxHp, int skillRevivePct, int casterRevivePct) {
    const int pct = std::max(skillRevivePct, casterRevivePct);
    return std::max(1, targetMaxHp * pct / 100);
}

}  // namespace cd::battle
