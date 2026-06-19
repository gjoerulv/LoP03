#include "game/Party.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace cd {

int deriveMaxMp(int magic) {
    // Simple, deterministic placeholder; tuned in the combat/content milestones.
    return magic;
}

namespace {

int grow(int base, float perLevel, int level) {
    const float value = static_cast<float>(base) + perLevel * static_cast<float>(level - 1);
    return static_cast<int>(value);  // truncates toward zero; deterministic
}

}  // namespace

void recomputeDerivedStats(Character& character, const content::ClassDef& cls) {
    const int level = character.level < 1 ? 1 : character.level;
    character.level = level;

    const content::StatBlock& base = cls.baseStats;
    const content::StatGrowth& g = cls.growth;
    character.stats.maxHp = grow(base.maxHp, g.maxHp, level);
    character.stats.attack = grow(base.attack, g.attack, level);
    character.stats.magic = grow(base.magic, g.magic, level);
    character.stats.defense = grow(base.defense, g.defense, level);
    character.stats.speed = grow(base.speed, g.speed, level);

    character.maxHp = std::max(1, character.stats.maxHp);
    character.maxMp = std::max(0, deriveMaxMp(character.stats.magic));

    character.hp = std::clamp(character.hp, 0, character.maxHp);
    character.mp = std::clamp(character.mp, 0, character.maxMp);
}

Character createCharacter(const content::ClassDef& cls, std::string name, int level) {
    Character c;
    c.classId = cls.id;
    c.name = std::move(name);
    c.level = level < 1 ? 1 : level;
    c.xp = 0;
    c.hp = 1;  // bumped to full below
    c.mp = 0;
    recomputeDerivedStats(c, cls);
    c.hp = c.maxHp;
    c.mp = c.maxMp;
    return c;
}

void healFull(Party& party) {
    for (Character& c : party.members) {
        c.hp = c.maxHp;
        c.mp = c.maxMp;
    }
}

int highestLevel(const Party& party) {
    int best = 0;
    for (const Character& c : party.members) {
        best = std::max(best, c.level);
    }
    return best;
}

}  // namespace cd
