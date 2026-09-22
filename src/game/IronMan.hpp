#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Ledger.hpp"
#include "game/Party.hpp"

// M123 - Iron Man, as pure rules and pure text.
//
// Iron Man is a New Game mode (`Party::ironMan`, runtime only - such a party
// never reaches a slot): no saving and no autosaving, so the whole run is one
// sitting; every REAL wipe ends the run for good (dungeons, castle challenges,
// treasure digs - never the sparring mirror, and fleeing is not a wipe); and
// the battle Escape command has a price; and (M131) the Dragon class may
// not join the party. Everything a state needs to say or apply lives here
// exactly once, raylib-free (the QuitPrompt precedent), so the headless
// suite pins the rules and the screens only draw them.

namespace cd::ironman {

// ---------------------------------------------------------------- text ----

inline constexpr const char* kNormalName = "Normal";
inline constexpr const char* kIronManName = "Iron Man";

inline constexpr const char* kNormalBlurb =
    "The game as it has always been: save at any Save Point, autosave on "
    "entering a dungeon, and a wipe costs gold and the run - never the party.";

// The rules page shown while Iron Man is highlighted at New Game. One
// paragraph per rule; the owner's ruling (2026-09-18) is that ALL of it is
// explained before the mode can be chosen. M131 added the Dragon bar as the
// fourth paragraph (owner ruling 2026-09-22).
inline constexpr std::array<const char*, 5> kRules = {
    "No saves and no autosaves: one sitting.",
    "A party wipe ends the run for good - in dungeons, castle challenges and "
    "treasure digs. Sparring is safe.",
    "Escaping a battle forfeits ALL gold and the whole bag (worn gear and "
    "heirlooms stay). Everyone standing drops to 1 HP and 0 MP; the fallen "
    "stay fallen.",
    "No Dragons: the Dragon class cannot join the party.",
    "Fell the King, the Last Dragon or the Deadly Duck for an Iron "
    "accomplishment.",
};

inline constexpr const char* kBeginTitle = "Begin an Iron Man run?";
inline constexpr const char* kBeginBody =
    "Nothing will be saved, and one wipe ends the run for good.";
inline constexpr const char* kBeginConfirm = "Begin";
inline constexpr const char* kBeginCancel = "Back";

inline constexpr const char* kTag = "IRON MAN";

// Every Save entry point answers with this instead of the slot list.
inline constexpr const char* kSaveRefusal = "Iron Man: this run cannot be saved.";
// The Guild's entry line (it normally promises the autosave).
inline constexpr const char* kGuildEntryLine = "Choose a dungeon - Iron Man: nothing is saved.";
// The pause menus' quit warning (both town and dungeon: there is no save to
// fall back on, so quitting IS the end of the run).
inline constexpr const char* kQuitBody =
    "Iron Man: nothing is saved. Quitting ends this run for good.";

inline constexpr const char* kEscapeTitle = "Escape?";
inline constexpr const char* kEscapeBody =
    "Iron Man: fleeing forfeits ALL gold and the whole bag. Worn gear and "
    "heirlooms stay. Everyone standing drops to 1 HP and 0 MP.";
inline constexpr const char* kEscapeConfirm = "Escape";
inline constexpr const char* kEscapeCancel = "Keep fighting";
inline constexpr const char* kEscapeLine =
    "The party flees - the gold and the bag are left behind!";
inline constexpr const char* kEscapedOutcome =
    "Escaped - with nothing but what you wear.";
inline constexpr const char* kDefeatOutcome =
    "The party has fallen... Iron Man: the run ends here.";

inline constexpr const char* kFallTitle = "The run ends here";
inline constexpr const char* kFallBody =
    "Iron Man keeps no saves. This party's story is over.";

// ------------------------------------------------------- the Dragon bar ----

// M131 (owner ruling 2026-09-22): the Dragon class is not allowed in an Iron
// Man party. Party creation bars it outright (listed and greyed like a
// locked class, never begun with), and the three Iron accomplishments
// additionally refuse a party with a Dragon in it - so nothing that could
// ever slip one in later earns them. A Normal party is untouched, and so is
// every save from an older build (an Iron Man party is never saved, so none
// can be loaded with a Dragon in it).
inline constexpr const char* kBarredClassId = "dragon";
inline constexpr const char* kBarredClassSuffix = " (Not allowed)";  // the class capsule
inline constexpr const char* kBarredClassNote =
    "Iron Man: the Dragon class is not allowed - choose another.";

inline bool classBarred(const std::string& classId) { return classId == kBarredClassId; }

inline bool hasBarredMember(const Party& party) {
    for (const Character& m : party.members) {
        if (classBarred(m.classId)) {
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------ the escape price ----

// Heirlooms are carried memories, not loot (M96): they survive in the bag as
// they do when worn. Everything else in the bag is forfeit - including an id
// the content no longer knows.
inline bool keptOnEscape(const std::string& itemId, const content::ContentDatabase& db) {
    const content::ItemDef* item = db.findItem(itemId);
    return item != nullptr && item->type == content::ItemType::Heirloom;
}

struct EscapeForfeit {
    int gold = 0;   // gold lost
    int items = 0;  // bag items lost (stack counts summed)
};

// Gold to zero (a LOSS in the M109 ledger, never "spent") and the bag emptied
// of everything but heirlooms. Worn equipment is on the members, not in the
// bag, so it is untouched by construction; so are the counters that are not
// bag items (rest and legendary tokens, map pieces, curios).
inline EscapeForfeit forfeitOnEscape(Party& party, const content::ContentDatabase& db) {
    EscapeForfeit lost;
    lost.gold = party.gold > 0 ? party.gold : 0;
    loseGold(party, lost.gold);
    std::vector<ItemStack> kept;
    for (const ItemStack& stack : party.inventory.stacks) {
        if (keptOnEscape(stack.itemId, db)) {
            kept.push_back(stack);
        } else {
            lost.items += stack.count > 0 ? stack.count : 0;
        }
    }
    party.inventory.stacks = std::move(kept);
    return lost;
}

// Everyone standing drops to 1 HP and 0 MP; the fallen are left exactly as
// they fell. Idempotent - the dungeon re-applies it after a dragonform or
// flock restore, whose percentage mapping could otherwise round 1 HP up.
inline void clampEscapeVitals(Party& party) {
    for (Character& m : party.members) {
        if (m.hp > 0) {
            m.hp = 1;
            m.mp = 0;
        }
    }
}

inline EscapeForfeit applyEscape(Party& party, const content::ContentDatabase& db) {
    const EscapeForfeit lost = forfeitOnEscape(party, db);
    clampEscapeVitals(party);
    return lost;
}

// ------------------------------------------------------- where and who ----

// Where the run ended and who ended it - shown on the fall notice (and, from
// M124, recorded with the fallen run).
struct FallenInfo {
    std::string place;
    std::string foes;
};

// `floor` is 1-based.
inline std::string fallenPlaceDungeon(int town, const std::string& themeName, int floor,
                                      int floorCount, bool eternal) {
    std::string out = "Town " + std::to_string(town);
    if (!themeName.empty()) {
        out += " - " + themeName;
    }
    if (eternal) {
        out += ", Eternal floor " + std::to_string(floor);
    } else if (floorCount > 1) {
        out += ", floor " + std::to_string(floor) + " of " + std::to_string(floorCount);
    }
    return out;
}

// A team's display name; else its boss; else its first enemy ("and company"
// when it did not come alone). Never empty.
inline std::string fallenFoes(const dungeon::EnemyTeam& team, const content::ContentDatabase& db) {
    if (!team.name.empty()) {
        return team.name;
    }
    if (!team.bossId.empty()) {
        if (const content::BossDef* boss = db.findBoss(team.bossId)) {
            return boss->name;
        }
    }
    for (const std::string& id : team.enemyIds) {
        if (const content::EnemyDef* enemy = db.findEnemy(id)) {
            return team.enemyIds.size() > 1 ? enemy->name + " and company" : enemy->name;
        }
    }
    return "Something unseen";
}

}  // namespace cd::ironman
