#pragma once

#include <string>

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

    content::StatBlock stats;  // derived: base + growth * (level - 1)
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;

    bool isAlive() const { return hp > 0; }
};

}  // namespace cd
