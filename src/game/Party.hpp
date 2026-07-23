#pragma once

#include <cstddef>
#include <vector>

#include "content/Definitions.hpp"
#include "game/BlackMarket.hpp"
#include "game/Castle.hpp"
#include "game/Character.hpp"
#include "game/Inventory.hpp"
#include "game/StakesLadder.hpp"

namespace cd {

namespace content {
class ContentDatabase;
}


struct Party {
    std::vector<Character> members;
    Inventory inventory;
    int gold = 0;
    int restTokens = 0;  // free-rest tokens from dungeon events (M30)
    // Town ladder (M32). currentTown is where the party stands (1..kTownCount);
    // highestUnlockedTown is the furthest reachable town. Both saved as optional
    // fields, old saves load as 1/1. Rules live in game/WorldLadder.hpp.
    int currentTown = 1;
    int highestUnlockedTown = 1;
    // Stakes escalation (M33): the previous completed run's stakes + penalty
    // steps. Saved as optional fields (old saves -> fresh state). See
    // game/StakesLadder.hpp.
    StakesState stakes;
    // Black market (M34): a currency won from optional elite fights, and the
    // current legendary offer (if any). Both saved as optional fields (old saves
    // -> 0 / no offer). See game/BlackMarket.hpp.
    int legendaryTokens = 0;
    BlackMarketOffer blackMarket;
    // Castle (M40): whether the road from town 7 to the castle is open (set by a
    // town-7 dungeon clear), and the party's castle-challenge records + earned
    // King title. All optional save fields (old saves -> locked / no records);
    // kept entirely separate from the dungeon scoreboard. See game/Castle.hpp.
    bool castleUnlocked = false;
    CastleRecords castleRecords;
    // Story serial (M41): a 7-bit mask of which town installments have been heard
    // (see game/Story.hpp). Optional save field; old saves -> 0 (nothing heard).
    int storyMet = 0;
    // Enrichment (M42), all optional save fields (old saves -> empty / 0):
    // the set of enemy/boss ids this party has fought (the bestiary), and the
    // party's personal victory records (display-only, never ranked).
    std::vector<std::string> encountered;
    int recordBiggestHit = 0;
    int recordRunDamage = 0;

    bool empty() const { return members.empty(); }
    std::size_t size() const { return members.size(); }
};

inline constexpr std::size_t kMaxPartySize = 4;

// Provisional MP pool until the combat milestone refines it: scales with magic.
int deriveMaxMp(int magic);

// Recomputes stats/maxHp/maxMp from the class at the character's current level,
// then clamps hp/mp into range. Use after loading or leveling.
void recomputeDerivedStats(Character& character, const content::ClassDef& cls);

// Recomputes stats from the class AND equipped items (looked up in the db), then
// clamps hp/mp. Use after equipment changes or on load.
void refreshCharacter(Character& character, const content::ContentDatabase& db);

// Creates a fresh, full-health character of the given class.
Character createCharacter(const content::ClassDef& cls, std::string name, int level = 1);

// Restores every member to full HP/MP.
void healFull(Party& party);

// M47 — the castle's price of failure. A lost (or fled) castle challenge no
// longer heals the party: everyone who was still standing ends at exactly 1 HP,
// the fallen stay fallen, and MP is untouched. A total wipe leaves the FIRST
// member at 1 HP so the party can always limp to an Inn. No gold is taken (the
// castle never charged any) and no run is forfeited. Pure and unit-tested.
void clampCastleDefeat(Party& party);

// M45: the party's additive unlockable-class score modifier, summed over the
// members' `ClassDef::scoreModPct` (0 for any party of the six original classes).
// Pure and derived — never stored on the party, so it cannot drift from the
// roster that produced it.
int partyClassModPct(const Party& party, const content::ContentDatabase& db);

// M45: may this character equip into `slot`? False when its class bans the slot
// (the Dragon wears no armor, the Jester holds no weapon, the Goose is a goose).
// One definition, used by the equip shop and any future equip path.
bool canEquipSlot(const Character& character, content::EquipSlot slot,
                  const content::ContentDatabase& db);

// Paid rest (M30). The inn cost scales with the highest party level so a full
// rest stays a real decision as income grows: kBase + kPerLevel*(level-1),
// clamped to [kBase, kMax]. Constants live here for balance tuning.
inline constexpr int kRestCostBase = 20;
inline constexpr int kRestCostPerLevel = 12;
inline constexpr int kRestCostMax = 500;
int restCost(const Party& party);

// Highest level among living/all members (0 if empty) — used for save summaries.
int highestLevel(const Party& party);

inline constexpr int kMaxLevel = 50;

// XP required to advance FROM `level` to the next level.
int xpToNext(int level);

// Adds XP to a character, leveling up (recomputing stats and healing the maxHP
// gained) as needed. Grants to the whole party with grantPartyXp.
void grantXp(Character& character, int xp, const content::ContentDatabase& db);
void grantPartyXp(Party& party, int xp, const content::ContentDatabase& db);

}  // namespace cd
