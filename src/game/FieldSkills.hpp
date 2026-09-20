#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "battle/HealMath.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "content/Enums.hpp"
#include "game/Character.hpp"
#include "game/Milestones.hpp"
#include "game/Party.hpp"

// M122: casting heals from the Party panel - the pure rules. A member may
// spend MP on a healing skill OUTSIDE battle, but only inside a dungeon
// (owner decision: a town has an Inn for that), only on skills that restore
// HP or raise the fallen, for the SAME MP cost and the SAME amount the battle
// would restore (battle/HealMath.hpp + the caster's own milestones). Statuses
// are battle-scoped, so a heal's buff rider and a cleanse simply have nothing
// to do here; Purify (a pure cleanse) and summons stay battle-only. Like the
// M90 item rules, a cast that would do nothing is refused WITH THE REASON and
// costs nothing. Raylib-free and unit-tested; PartyState only presents.

namespace cd {

// A field skill restores HP or raises the fallen. Summons keep their battle
// stagecraft and once-per-run ledger; a pure cleanse has nothing to cleanse.
inline bool isFieldSkill(const content::SkillDef& s) {
    if (s.category != content::SkillCategory::Heal || content::isSummonSkill(s)) {
        return false;
    }
    return s.power > 0 || s.reviveHpPct > 0;
}

// The caster's milestone shares, resolved exactly as buildBattle resolves them
// onto the Combatant (HealCastPct sums; ReviveAtPct takes the highest).
struct FieldCaster {
    int healCastPct = 0;
    int reviveAtPct = 0;
};

inline FieldCaster fieldCasterFor(const Character& caster, const content::ContentDatabase& db) {
    FieldCaster f;
    forEachChosenMilestone(caster, db, [&f](const content::MilestoneDef& m) {
        if (m.effect == content::MilestoneEffect::HealCastPct) {
            f.healCastPct += m.magnitude;
        } else if (m.effect == content::MilestoneEffect::ReviveAtPct) {
            f.reviveAtPct = std::max(f.reviveAtPct, m.magnitude);
        }
    });
    return f;
}

// What one living target would be offered (before the max-HP cap).
inline int fieldHealAmount(const Character& caster, const content::SkillDef& s,
                           const content::ContentDatabase& db) {
    return battle::healWithCastBonus(battle::healBase(caster.stats.magic, s.power),
                                     fieldCasterFor(caster, db).healCastPct);
}

// Would the cast change anything for THIS target? A living member with room
// is healed (when the skill has power); a fallen one is raised (when the
// skill can revive). Mirrors Battle::useSkill's Heal branch.
inline bool fieldSkillHelps(const content::SkillDef& s, const Character& target) {
    if (target.hp > 0) {
        return s.power > 0 && target.hp < target.maxHp;
    }
    return s.reviveHpPct > 0;
}

// Why `target` cannot take the cast ("" = it can). Single-target skills only.
inline std::string fieldTargetRefusal(const content::SkillDef& s, const Character& target) {
    if (fieldSkillHelps(s, target)) {
        return "";
    }
    if (target.hp <= 0) {
        return "The fallen need a revive, not a mend.";
    }
    if (s.power <= 0) {
        return "Still standing - it reaches only the fallen.";
    }
    return "Already at full HP.";
}

// Why `caster` cannot cast `s` from the menu right now ("" = it can). The
// order is the order a player would fix things in.
inline std::string fieldSkillRefusal(const Party& party, int casterIndex,
                                     const content::SkillDef& s, bool inDungeon) {
    if (casterIndex < 0 || casterIndex >= static_cast<int>(party.members.size())) {
        return "Nobody to cast it.";
    }
    const Character& caster = party.members[static_cast<std::size_t>(casterIndex)];
    if (content::isSummonSkill(s)) {
        return "A summon answers only in battle.";
    }
    if (!isFieldSkill(s)) {
        return s.category == content::SkillCategory::Heal ? "Nothing to cleanse outside battle."
                                                          : "Its moment is in battle.";
    }
    if (!inDungeon) {
        return "Heals are cast in dungeons - the Inn mends you in town.";
    }
    if (caster.hp <= 0) {
        return "The fallen cast nothing.";
    }
    if (caster.mp < s.mpCost) {
        return "Not enough MP: " + std::to_string(s.mpCost) + " needed, " +
               std::to_string(caster.mp) + " left.";
    }
    for (const Character& target : party.members) {
        if (fieldSkillHelps(s, target)) {
            return "";
        }
    }
    return s.power > 0 ? "Nobody needs healing." : "Nobody has fallen.";
}

// True when the cast needs a target pick (every other field skill reaches
// the whole party at once).
inline bool fieldSkillNeedsTarget(const content::SkillDef& s) {
    return s.target == content::SkillTarget::SingleAlly;
}

// Casts an accepted skill (call only when fieldSkillRefusal returned empty -
// and, for a single-target skill, fieldTargetRefusal too). `targetIndex` is
// ignored by whole-party skills; a Self skill reaches the caster. Spends the
// MP once, applies the battle's arithmetic per target, and returns the line
// to show. A target the cast cannot help is skipped, never charged twice.
inline std::string applyFieldSkill(Party& party, int casterIndex, int targetIndex,
                                   const content::SkillDef& s,
                                   const content::ContentDatabase& db) {
    Character& caster = party.members[static_cast<std::size_t>(casterIndex)];
    const FieldCaster shares = fieldCasterFor(caster, db);
    const int amount = fieldHealAmount(caster, s, db);

    std::vector<int> targets;
    if (s.target == content::SkillTarget::AllAllies) {
        for (int i = 0; i < static_cast<int>(party.members.size()); ++i) {
            targets.push_back(i);
        }
    } else if (s.target == content::SkillTarget::Self) {
        targets.push_back(casterIndex);
    } else {
        targets.push_back(targetIndex);
    }

    caster.mp = std::max(0, caster.mp - s.mpCost);

    int healedTotal = 0;
    int healedCount = 0;
    std::string raised;
    std::string lastName;
    for (int ti : targets) {
        if (ti < 0 || ti >= static_cast<int>(party.members.size())) {
            continue;
        }
        Character& t = party.members[static_cast<std::size_t>(ti)];
        if (!fieldSkillHelps(s, t)) {
            continue;
        }
        if (t.hp > 0) {
            const int before = t.hp;
            t.hp = std::min(t.maxHp, t.hp + amount);
            healedTotal += t.hp - before;
            ++healedCount;
            lastName = t.name;
        } else {
            t.hp = battle::reviveHp(t.maxHp, s.reviveHpPct, shares.reviveAtPct);
            raised += (raised.empty() ? "" : ", ") + t.name;
        }
    }

    std::string line = caster.name + " casts " + s.name + ".";
    if (healedCount == 1) {
        line += " " + lastName + " recovers " + std::to_string(healedTotal) + " HP.";
    } else if (healedCount > 1) {
        line += " The party recovers " + std::to_string(healedTotal) + " HP.";
    }
    if (!raised.empty()) {
        line += " " + raised + " rises again!";
    }
    return line;
}

}  // namespace cd
