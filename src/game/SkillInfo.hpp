#pragma once

#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "content/Enums.hpp"
#include "game/Character.hpp"
#include "game/Milestones.hpp"

// M121: what the UI says about a skill — pure and raylib-free, so the battle
// list, the details sheet and the party panel read ONE answer and a test pins
// it:
//   - the minimal kind line ("Fire damage." / "Debuff - no damage." ...);
//   - the description AS THIS CHARACTER CASTS IT: a held milestone that
//     changes the skill by name supplies an authored, adjusted description
//     (MilestoneDef::skillTexts); a held milestone that boosts the skill
//     generically contributes its own sentence instead. Either way the skill
//     wears the milestone mark — and only on the member who holds the
//     milestone.
// The "does this milestone touch this skill" predicates mirror the conditions
// Battle::useSkill applies the matching Combatant fields under, so the text
// can never promise a bonus the battle does not pay.

namespace cd {

// "Fire damage." / "Non-elemental damage." / "Healing - no damage." ...
// A summon says what its creature does.
inline std::string skillKindLine(const content::SkillDef& s) {
    const auto damageWords = [&s]() -> std::string {
        if (s.element == content::Element::None) {
            return "Non-elemental damage";
        }
        return std::string(content::elementDisplayName(s.element)) + " damage";
    };
    switch (content::skillKindFor(s)) {
        case content::SkillKind::Summon:
            if (s.category == content::SkillCategory::Heal) {
                return "Summon - healing, no damage.";
            }
            return content::skillDealsDamage(s) ? "Summon - " + damageWords() + "."
                                                : "Summon - no damage.";
        case content::SkillKind::Heal:
            return "Healing - no damage.";
        case content::SkillKind::Buff:
            return "Buff - no damage.";
        case content::SkillKind::Debuff:
            return "Debuff - no damage.";
        case content::SkillKind::Fire:
        case content::SkillKind::Ice:
        case content::SkillKind::Lightning:
        case content::SkillKind::Earth:
        case content::SkillKind::Holy:
        case content::SkillKind::Dark:
        case content::SkillKind::NonElemental:
            break;
    }
    return damageWords() + ".";
}

// The M62 "pure cleanse": a heal-category skill with no power whose only job
// is the cleanse (Purify) — it heals nothing unless the caster holds
// Purifying Light.
inline bool isPureCleanse(const content::SkillDef& s) {
    return s.category == content::SkillCategory::Heal && s.power == 0 &&
           s.controlEffect == content::SkillEffect::Cleanse;
}

// Does holding `m` change how `s` resolves? `holderPurifyHeals` is whether the
// same character also holds Purifying Light (it turns a pure cleanse into a
// heal, which Devotion then boosts). Effects that key on the FOE's state
// (execute, vs-afflicted, weakness, first strike), on basic attacks, on stats
// or on passives are deliberately absent: they do not change a skill, they
// change every blow.
inline bool milestoneTouchesSkill(const content::MilestoneDef& m, const content::SkillDef& s,
                                  bool holderPurifyHeals) {
    using E = content::MilestoneEffect;
    switch (m.effect) {
        case E::MagicSkillPct:
            return s.category == content::SkillCategory::Magic;
        case E::AoeSpellPct:
            return s.category == content::SkillCategory::Magic &&
                   s.target == content::SkillTarget::AllEnemies;
        case E::HealCastPct:
            return s.category == content::SkillCategory::Heal &&
                   (!isPureCleanse(s) || holderPurifyHeals);
        case E::StatusTurnsBonus:
            // Lingering Hex never extends a turn-control status (addStatus).
            return s.statusEffect != content::StatusType::None &&
                   s.statusEffect != content::StatusType::Terrified &&
                   s.statusEffect != content::StatusType::Stunned;
        case E::TauntDebuffPct:
            return s.controlEffect == content::SkillEffect::Taunt;
        case E::ReviveAtPct:
            // The caster's share wins only when HIGHER, and never turns a
            // non-revive heal into one.
            return s.reviveHpPct > 0 && m.magnitude > s.reviveHpPct;
        case E::PurifyHeals:
            return isPureCleanse(s);
        case E::NoEnemyBuff:
            return s.alsoBuffsEnemies && s.statusEffect != content::StatusType::None;
        default:
            return false;
    }
}

// The skill as one character casts it.
struct SkillTextFor {
    // The description to show: an authored adjustment when a held milestone
    // carries one for this skill (the LATEST tier wins if several do), else
    // the skill's own text.
    std::string description;
    // True when `description` is an authored adjustment, not the base text.
    bool adjusted = false;
    // Every held milestone that touches the skill, oldest tier first. A
    // non-empty list is what earns the milestone mark.
    std::vector<const content::MilestoneDef*> milestones;
    // The held milestones whose own sentence should be shown with the skill:
    // the ones that touch it WITHOUT having supplied the adjusted description
    // (an adjusted description already says what changed).
    std::vector<const content::MilestoneDef*> noted;

    bool marked() const { return !milestones.empty(); }
};

inline SkillTextFor skillTextFor(const Character& c, const content::SkillDef& s,
                                 const content::ContentDatabase& db) {
    SkillTextFor out;
    out.description = s.description;

    bool holderPurifyHeals = false;
    forEachChosenMilestone(c, db, [&](const content::MilestoneDef& m) {
        holderPurifyHeals = holderPurifyHeals || m.effect == content::MilestoneEffect::PurifyHeals;
    });

    const content::MilestoneDef* adjustedBy = nullptr;
    forEachChosenMilestone(c, db, [&](const content::MilestoneDef& m) {
        const content::MilestoneSkillText* authored = nullptr;
        for (const content::MilestoneSkillText& t : m.skillTexts) {
            if (t.skill == s.id) {
                authored = &t;
                break;
            }
        }
        if (authored == nullptr && !milestoneTouchesSkill(m, s, holderPurifyHeals)) {
            return;
        }
        out.milestones.push_back(&m);
        if (authored != nullptr) {
            out.description = authored->description;
            out.adjusted = true;
            adjustedBy = &m;
        }
    });
    for (const content::MilestoneDef* m : out.milestones) {
        if (m != adjustedBy) {
            out.noted.push_back(m);
        }
    }
    return out;
}

// The details-sheet body for a skill as `caster` casts it (nullptr = nobody in
// particular: an enemy's skill, a scroll on the shelf). `mpCost` is the cost
// to show — the battle passes its cursed-caster cost, menus the authored one.
// `blockLine` (optional) is a reason the skill cannot be cast right now.
inline std::string skillDetailsBody(const content::SkillDef& s, const Character* caster,
                                    const content::ContentDatabase& db, int mpCost,
                                    const std::string& blockLine = "") {
    std::string body = "MP cost " + std::to_string(mpCost) + ".  " + skillKindLine(s);
    if (!blockLine.empty()) {
        body += "\n" + blockLine;
    }
    SkillTextFor text;
    if (caster != nullptr) {
        text = skillTextFor(*caster, s, db);
    } else {
        text.description = s.description;
    }
    if (!text.description.empty()) {
        body += "\n\n" + text.description;
    }
    if (text.marked()) {
        body += "\n";
        for (const content::MilestoneDef* m : text.milestones) {
            body += "\nMilestone - " + m->name;
            const bool isNoted = [&]() {
                for (const content::MilestoneDef* n : text.noted) {
                    if (n == m) {
                        return true;
                    }
                }
                return false;
            }();
            if (isNoted && !m->description.empty()) {
                body += ": " + m->description;
            }
        }
    }
    return body;
}

}  // namespace cd
