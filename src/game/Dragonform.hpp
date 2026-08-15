#pragma once

#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "game/Party.hpp"

// M93 (owner decision 6): the Dragonform event — the party fights its NEXT
// battle as Dragons (the M45 class: enormous stats, no skills, no gear, the
// all-target basic with its riders), for a flat -100 score stated on the
// panel. Pure and raylib-free: enter swaps each member for a Dragon of the
// same name and level with HP/MP carried BY PERCENTAGE; leave restores the
// stashed originals with the battle's outcome mapped back the same way
// (KO stays KO — the transformation never revives or executes anyone).
// buildBattle then reads the transformed members like any party, so the
// battle needs no special cases and the Simulator agrees by construction.
// The M45 class score modifier is NOT applied for the borrowed form (the
// event carries its own price; owner decision 6).

namespace cd {

inline constexpr const char* kDragonformClassId = "dragon";

struct DragonformStash {
    std::vector<Character> original;  // the real members, exactly as they were
};

// Maps `value` from one 1..fromMax scale onto 1..toMax, preserving zero (KO)
// and never rounding a survivor down to 0.
inline int dragonformMapVital(int value, int fromMax, int toMax) {
    if (value <= 0 || fromMax <= 0 || toMax <= 0) {
        return value <= 0 ? 0 : (value > 0 && toMax > 0 ? 1 : 0);
    }
    const long long scaled = static_cast<long long>(value) * toMax / fromMax;
    return static_cast<int>(scaled < 1 ? 1 : (scaled > toMax ? toMax : scaled));
}

// Transforms the party in place; returns the stash `leaveDragonform` needs.
// Unknown dragon class (malformed content) -> empty stash, party untouched,
// so the event can never corrupt a save.
inline DragonformStash enterDragonform(Party& party, const content::ContentDatabase& db) {
    DragonformStash stash;
    const content::ClassDef* dragon = db.findClass(kDragonformClassId);
    if (dragon == nullptr) {
        return stash;
    }
    for (Character& m : party.members) {
        stash.original.push_back(m);
        Character d = createCharacter(*dragon, m.name, m.level);
        d.hp = dragonformMapVital(m.hp, m.maxHp, d.maxHp);
        d.mp = dragonformMapVital(m.mp, m.maxMp, d.maxMp);
        m = std::move(d);  // bare-clawed: a Dragon equips nothing (M45)
    }
    return stash;
}

// Restores the stashed members with the battle's outcome carried back by
// percentage. A no-op for an empty stash (enter refused).
inline void leaveDragonform(Party& party, const DragonformStash& stash) {
    if (stash.original.size() != party.members.size()) {
        return;
    }
    for (std::size_t i = 0; i < party.members.size(); ++i) {
        const Character& dragon = party.members[i];
        Character restored = stash.original[i];
        restored.hp = dragonformMapVital(dragon.hp, dragon.maxHp, restored.maxHp);
        restored.mp = dragonformMapVital(dragon.mp, dragon.maxMp, restored.maxMp);
        party.members[i] = std::move(restored);
    }
}

}  // namespace cd
