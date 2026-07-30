#pragma once

#include <array>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "game/Character.hpp"

// Class level milestones (M63): at levels 10/20/30 a character picks one of
// two permanent class bonuses (data/milestones.json). Pure, raylib-free
// helpers shared by the choice modal, refreshCharacter, buildBattle, and the
// party panel. The chosen ids persist on the Character (optional save
// fields); everything else derives.

namespace cd {

inline constexpr std::array<int, 3> kMilestoneTiers = {10, 20, 30};

// The character's stored choice slot for a tier (nullptr for a non-tier).
inline std::string* milestoneSlot(Character& c, int tier) {
    if (tier == 10) return &c.milestone10;
    if (tier == 20) return &c.milestone20;
    if (tier == 30) return &c.milestone30;
    return nullptr;
}
inline const std::string* milestoneSlot(const Character& c, int tier) {
    return milestoneSlot(const_cast<Character&>(c), tier);
}

// The first tier the character has REACHED but not chosen, provided the
// content actually ships a complete a/b pair for its class (a class without
// milestones never prompts). 0 = nothing pending.
inline int pendingMilestoneTier(const Character& c, const content::ContentDatabase& db) {
    for (int tier : kMilestoneTiers) {
        if (c.level < tier) {
            return 0;  // tiers are ascending; nothing further is reached either
        }
        const std::string* slot = milestoneSlot(c, tier);
        if (slot != nullptr && slot->empty()) {
            const auto pair = db.milestonePair(c.classId, tier);
            if (pair.first != nullptr && pair.second != nullptr) {
                return tier;
            }
        }
    }
    return 0;
}

// The chosen MilestoneDef for a tier, or nullptr (unchosen / unknown id — a
// stale save's id simply resolves to nothing and the tier re-asks).
inline const content::MilestoneDef* chosenMilestone(const Character& c, int tier,
                                                   const content::ContentDatabase& db) {
    const std::string* slot = milestoneSlot(c, tier);
    if (slot == nullptr || slot->empty()) {
        return nullptr;
    }
    return db.findMilestone(*slot);
}

// Visits the character's chosen milestone defs (up to three), oldest tier
// first. `fn` is called with each resolved MilestoneDef.
template <typename Fn>
inline void forEachChosenMilestone(const Character& c, const content::ContentDatabase& db,
                                   Fn&& fn) {
    for (int tier : kMilestoneTiers) {
        if (const content::MilestoneDef* m = chosenMilestone(c, tier, db)) {
            fn(*m);
        }
    }
}

// M63 (Cutpurse / Golden Goose): the extra battle-gold percent earned by the
// party's STANDING members — additive, applied by whichever state awards
// battle gold (the dungeon's victory credit). Pure and headless-testable.
inline int partyGoldBonusPct(const std::vector<Character>& members,
                             const content::ContentDatabase& db) {
    int pct = 0;
    for (const Character& c : members) {
        if (!c.isAlive()) {
            continue;
        }
        forEachChosenMilestone(c, db, [&](const content::MilestoneDef& m) {
            if (m.effect == content::MilestoneEffect::GoldBonusPct) {
                pct += m.magnitude;
            }
        });
    }
    return pct;
}

}  // namespace cd
