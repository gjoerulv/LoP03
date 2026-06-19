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
    std::vector<EnemyTag> tags;
    std::vector<std::string> skills;  // skill ids
    int xpReward = 0;
    int goldReward = 0;
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

}  // namespace cd::content
