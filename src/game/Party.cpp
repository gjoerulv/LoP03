#include "game/Party.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "content/ContentDatabase.hpp"

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

void refreshCharacter(Character& character, const content::ContentDatabase& db) {
    const content::ClassDef* cls = db.findClass(character.classId);
    if (cls == nullptr) {
        return;
    }
    recomputeDerivedStats(character, *cls);  // base stats (clamps hp/mp to base)

    content::StatBlock bonus;
    for (const std::string& id : {character.weapon, character.armor, character.accessory}) {
        if (id.empty()) {
            continue;
        }
        if (const content::ItemDef* item = db.findItem(id)) {
            bonus.maxHp += item->statBonus.maxHp;
            bonus.attack += item->statBonus.attack;
            bonus.magic += item->statBonus.magic;
            bonus.defense += item->statBonus.defense;
            bonus.speed += item->statBonus.speed;
        }
    }
    character.stats.maxHp += bonus.maxHp;
    character.stats.attack += bonus.attack;
    character.stats.magic += bonus.magic;
    character.stats.defense += bonus.defense;
    character.stats.speed += bonus.speed;

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

int restCost(const Party& party) {
    const int level = std::max(1, highestLevel(party));
    long cost = static_cast<long>(kRestCostBase) +
                static_cast<long>(kRestCostPerLevel) * (level - 1);
    cost = std::clamp<long>(cost, kRestCostBase, kRestCostMax);
    return static_cast<int>(cost);
}

int highestLevel(const Party& party) {
    int best = 0;
    for (const Character& c : party.members) {
        best = std::max(best, c.level);
    }
    return best;
}

int xpToNext(int level) {
    const int lv = level < 1 ? 1 : level;
    return 30 + (lv - 1) * 20;
}

void grantXp(Character& character, int xp, const content::ContentDatabase& db) {
    if (xp <= 0) {
        return;
    }
    character.xp += xp;
    bool leveled = false;
    while (character.level < kMaxLevel && character.xp >= xpToNext(character.level)) {
        character.xp -= xpToNext(character.level);
        ++character.level;
        leveled = true;
    }
    if (character.level >= kMaxLevel) {
        character.xp = 0;
    }
    if (leveled) {
        const int beforeMaxHp = character.maxHp;
        refreshCharacter(character, db);
        character.hp = std::min(character.maxHp, character.hp + std::max(0, character.maxHp - beforeMaxHp));
    }
}

void grantPartyXp(Party& party, int xp, const content::ContentDatabase& db) {
    for (Character& c : party.members) {
        grantXp(c, xp, db);
    }
}

}  // namespace cd
