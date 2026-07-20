#pragma once

#include <string>
#include <vector>

#include "content/Enums.hpp"
#include "content/Stats.hpp"

// Immutable content definitions loaded from JSON. Plain data; no behavior.
// Ids are lowercase string keys used for cross-references and lookups.

namespace cd::content {

struct SkillDef {
    std::string id;
    std::string name;
    SkillCategory category = SkillCategory::Physical;
    SkillTarget target = SkillTarget::SingleEnemy;
    Element element = Element::None;
    int power = 0;   // damage/heal magnitude (>= 0)
    int mpCost = 0;  // MP spent (>= 0)

    // Optional status applied to the skill's targets.
    StatusType statusEffect = StatusType::None;
    int statusMagnitude = 0;  // percent for buffs/debuffs, damage for poison
    int statusDuration = 0;   // turns

    // Enmity-control effect (M28): None for ordinary skills; Taunt/Fade/Intercept
    // manipulate the threat model instead of (or besides) dealing damage.
    SkillEffect controlEffect = SkillEffect::None;

    std::string description;
};

struct ClassDef {
    std::string id;
    std::string name;
    std::string role;
    StatBlock baseStats;
    StatGrowth growth;
    std::vector<std::string> startingSkills;  // skill ids
};

struct EnemyDef {
    std::string id;
    std::string name;
    StatBlock stats;
    EnemyTier tier = EnemyTier::Normal;
    EnemyRole role = EnemyRole::Bruiser;  // required in data (M20 taxonomy)
    std::vector<EnemyTag> tags;
    std::vector<std::string> skills;  // skill ids
    int xpReward = 0;
    int goldReward = 0;
};

// Externalized team-composition constraints (data/composition.json, M20).
// The generator derives every curve from these; it never hard-codes them.
struct CompositionDef {
    // Normal-team size: min (deepMinSize once depth >= deepMinDepth) up to
    // min(maxSizeCap, maxSizeBase + depth / maxSizePerDepths).
    int minSize = 2;
    int deepMinSize = 3;
    int deepMinDepth = 4;
    int maxSizeBase = 2;
    int maxSizePerDepths = 2;
    int maxSizeCap = 5;
    // Elite share: min(elitePctMax, elitePctPerDepth * depth) percent.
    int elitePctPerDepth = 9;
    int elitePctMax = 70;
    // Role rules per normal team.
    int maxSupport = 1;   // healers + buffers
    int minDamage = 1;    // bruisers + snipers
    // Boss minion count bounds (clamping the BossDef list).
    int minMinions = 0;
    int maxMinions = 3;
    // Enemy stat scaling: +pctPerDepth% per depth beyond startDepth, capped.
    int scaleStartDepth = 5;
    int scalePctPerDepth = 6;
    int scalePctMax = 90;

    int teamSizeMin(int depth) const { return depth >= deepMinDepth ? deepMinSize : minSize; }
    int teamSizeMax(int depth) const {
        const int grown = maxSizeBase + depth / (maxSizePerDepths < 1 ? 1 : maxSizePerDepths);
        return grown > maxSizeCap ? maxSizeCap : grown;
    }
    int eliteChancePct(int depth) const {
        const int pct = elitePctPerDepth * depth;
        return pct > elitePctMax ? elitePctMax : pct;
    }
    int statScalePct(int depth) const {
        if (depth <= scaleStartDepth) {
            return 0;
        }
        const int pct = (depth - scaleStartDepth) * scalePctPerDepth;
        return pct > scalePctMax ? scalePctMax : pct;
    }
};

struct ItemDef {
    std::string id;
    std::string name;
    ItemType type = ItemType::Consumable;
    EquipSlot slot = EquipSlot::None;  // for equipment/relics
    Rarity rarity = Rarity::Common;
    int value = 0;  // gold value (>= 0)

    // Consumable behavior.
    ConsumableEffect effect = ConsumableEffect::None;
    int effectAmount = 0;

    // Equipment/relic flat stat bonus.
    StatBlock statBonus;

    // Scroll: the skill id it teaches (empty for non-scrolls).
    std::string grantsSkill;

    std::string description;
};

struct BossDef {
    std::string id;
    std::string name;
    BossArchetype archetype = BossArchetype::Brute;
    StatBlock stats;
    std::vector<std::string> skills;   // skill ids
    std::vector<std::string> minions;  // enemy ids fighting alongside the boss
    std::string telegraph;             // flavor line shown when the battle begins
    int xpReward = 0;
    int goldReward = 0;
    std::string description;
};

struct DungeonThemeDef {
    std::string id;
    std::string name;
    std::vector<std::string> normalEnemies;  // enemy ids
    std::vector<std::string> eliteEnemies;   // enemy ids
    std::vector<std::string> bosses;         // boss ids
    std::string description;
};

}  // namespace cd::content
