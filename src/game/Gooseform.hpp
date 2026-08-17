#pragma once

#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "game/Dragonform.hpp"  // dragonformMapVital — the shared vital mapper
#include "game/Party.hpp"

// M103 (owner event 1): the Goose Polymorph — ONE random non-goose member
// becomes a Goose (the M45 class) for the REMAINDER of the dungeon, +100
// score stated up front. Unlike Dragonform (whole party, one battle) the
// gooseform spans many battles, so XP and levels earned while goosed are
// carried back to the real member on restore; vitals map by percentage both
// ways (KO stays KO — the transformation never revives or executes anyone).
// The worn heirloom rides along (the owner's M96 carve-out: the Goose "equips
// nothing" joke is about arms, not memories); arms and armor do not — a bare
// goose, like the bare dragon. Restore happens at the run's single exit choke
// (DungeonState::onExit), so every ending — boss victory, retreat, wipe —
// returns the real member. Runtime-only: the entry autosave predates the
// event, so a reload simply never met it.

namespace cd {

inline constexpr const char* kGooseformClassId = "goose";

struct GooseformStash {
    int memberIndex = -1;        // -1 = no transform active
    Character original;          // the real member, exactly as they were
};

// True when the member is already a goose — born one (the M45 class) or
// already transformed. The event may not fire on them again (owner rule).
inline bool isGoose(const Character& c) { return c.classId == kGooseformClassId; }

// True when at least one member could still be goosed (the owner: "The event
// may not be completed if all are geese").
inline bool anyNonGoose(const Party& party) {
    for (const Character& m : party.members) {
        if (!isGoose(m)) {
            return true;
        }
    }
    return false;
}

// Transforms members[index] in place; returns the stash the restore needs.
// Refuses (memberIndex -1, party untouched) on a bad index, an already-goose
// target, or malformed content — the event can never corrupt a party.
inline GooseformStash enterGooseform(Party& party, int index,
                                     const content::ContentDatabase& db) {
    GooseformStash stash;
    if (index < 0 || index >= static_cast<int>(party.members.size())) {
        return stash;
    }
    Character& m = party.members[static_cast<std::size_t>(index)];
    const content::ClassDef* goose = db.findClass(kGooseformClassId);
    if (goose == nullptr || isGoose(m)) {
        return stash;
    }
    stash.memberIndex = index;
    stash.original = m;
    Character g = createCharacter(*goose, m.name, m.level);
    g.xp = m.xp;                              // XP continuity across the span
    g.equippedHeirloom = m.equippedHeirloom;  // memories stay (M96 carve-out)
    refreshCharacter(g, db);
    g.hp = dragonformMapVital(m.hp, m.maxHp, g.maxHp);
    g.mp = dragonformMapVital(m.mp, m.maxMp, g.maxMp);
    m = std::move(g);
    return stash;
}

// Restores the real member with everything the goose lived through carried
// back: level and XP earned while goosed, vitals by percentage, KO preserved.
// A no-op for an inactive stash.
inline void leaveGooseform(Party& party, const GooseformStash& stash,
                           const content::ContentDatabase& db) {
    if (stash.memberIndex < 0 ||
        stash.memberIndex >= static_cast<int>(party.members.size())) {
        return;
    }
    Character& goose = party.members[static_cast<std::size_t>(stash.memberIndex)];
    Character restored = stash.original;
    restored.level = goose.level;  // levels waddled for still count
    restored.xp = goose.xp;
    restored.equippedHeirloom = goose.equippedHeirloom;
    refreshCharacter(restored, db);
    restored.hp = dragonformMapVital(goose.hp, goose.maxHp, restored.maxHp);
    restored.mp = dragonformMapVital(goose.mp, goose.maxMp, restored.maxMp);
    party.members[static_cast<std::size_t>(stash.memberIndex)] = std::move(restored);
}

// M106 (the Goosy rite): the WHOLE party fights its NEXT battle as Geese for
// a flat +300 score — the Dragonform machinery verbatim, with feathers: the
// same stash struct, the same vital mapping, the same restore call
// (leaveDragonform), so the battle and the Simulator need nothing new.
// Heirlooms ride along (the M96 carve-out); arms and armor do not.
inline DragonformStash enterFlockGooseform(Party& party, const content::ContentDatabase& db) {
    DragonformStash stash;
    const content::ClassDef* goose = db.findClass(kGooseformClassId);
    if (goose == nullptr) {
        return stash;
    }
    for (Character& m : party.members) {
        stash.original.push_back(m);
        Character g = createCharacter(*goose, m.name, m.level);
        g.equippedHeirloom = m.equippedHeirloom;
        refreshCharacter(g, db);
        g.hp = dragonformMapVital(m.hp, m.maxHp, g.maxHp);
        g.mp = dragonformMapVital(m.mp, m.maxMp, g.maxMp);
        m = std::move(g);
    }
    return stash;
}

}  // namespace cd
