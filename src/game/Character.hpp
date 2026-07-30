#pragma once

#include <string>
#include <vector>

#include "content/Stats.hpp"

// A runtime party member. Persistent fields (classId/name/level/xp and current
// hp/mp) are saved; `stats`, maxHp, and maxMp are DERIVED from the class on load
// so they never go stale.

namespace cd {

struct Character {
    std::string classId;
    std::string name;
    int level = 1;
    int xp = 0;

    content::StatBlock stats;  // derived: base + growth * (level - 1) + equipment
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;

    // Equipped item ids (empty = nothing in that slot).
    std::string weapon;
    std::string armor;
    std::string accessory;

    // Passive skills (M36): own many, equip one. Both persist as optional save
    // fields (old saves -> empty); unknown ids are dropped on load.
    std::vector<std::string> ownedPassives;
    std::string equippedPassive;

    // Level-milestone choices (M63): the chosen MilestoneDef id per tier, or
    // empty = not chosen yet (the choice modal prompts at the next
    // opportunity). Optional save fields; unknown ids are dropped on load, so
    // a stale save simply re-asks. See game/Milestones.hpp.
    std::string milestone10;
    std::string milestone20;
    std::string milestone30;

    // Skills learned from scrolls (M64), beyond the class learnset — permanent
    // and class-agnostic (owner decision). Optional save field; unknown ids
    // are dropped on load. buildBattle unions these with knownSkillsFor.
    std::vector<std::string> extraSkills;

    bool isAlive() const { return hp > 0; }
};

}  // namespace cd
