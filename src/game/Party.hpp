#pragma once

#include <cstddef>
#include <vector>

#include "content/Definitions.hpp"
#include "game/BlackMarket.hpp"
#include "game/Castle.hpp"
#include "game/Character.hpp"
#include "game/Guild.hpp"
#include "game/Inventory.hpp"
#include "game/StakesLadder.hpp"
#include "game/TreasureMap.hpp"  // M65: TreasureReveal + the puzzle-map rules

namespace cd {

namespace content {
class ContentDatabase;
}


struct Party {
    std::vector<Character> members;
    Inventory inventory;
    int gold = 0;
    int restTokens = 0;  // free-rest tokens from dungeon events (M30)
    // M103 (Sacrifice event): the NEXT battle's spoils XP doubles, once.
    // RUNTIME ONLY, never saved — the entry autosave predates the event, and
    // DungeonState::onExit clears a leftover so it cannot leak into town.
    bool doubleXpNext = false;
    // M95 (rules v17): summons cast this run — RUNTIME ONLY, never saved
    // (the entry autosave restarts a reload with a fresh ledger, like every
    // other run-runtime state). Reset at dungeon/challenge/treasure entry;
    // the spar's whole-party restore covers itself. Copied into each Battle
    // at build, appended by the shared cast rule, written back with HP/MP.
    std::vector<std::string> usedSummons;
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
    // M61: the Goose Town — opened by felling the King with at least one Goose
    // in the party. Optional save field (old saves -> locked).
    bool gooseTownUnlocked = false;
    // M65: the town puzzle map — pieces held this cycle (0..3; the fourth
    // converts into the reveal and resets), the revealed treasure, and the
    // exclusive scrolls already dug up (drawn without repetition). All
    // optional save fields (old saves -> nothing found yet).
    int mapPieces = 0;
    TreasureReveal treasure;
    std::vector<std::string> treasureScrollsAwarded;
    // M83: map pieces EARNED while a treasure already stood revealed (the
    // 4-floor completion drops) — banked IOUs, cap 3, paid out right after
    // the treasure-dig guardian falls. Optional save field (old saves -> 0).
    int mapPiecesOwed = 0;
    // M105: the Eternal descent's record — floors fully beaten in one run
    // (a felled floor-boss each). Optional save field; old saves -> 0.
    int eternalBestFloors = 0;
    // M66: dungeon curios dug up via the single-use treasure maps (see
    // game/Curios.hpp). Optional save field; the Curator achievement fires at
    // the full dozen.
    std::vector<std::string> ownedCurios;
    // M84: the per-town Guild Master ladder — the unlock (a 4-floor clear in
    // that town), the best-turns record, and the chosen permanent town perk.
    // All optional save fields (old saves -> every audience still locked).
    GuildRecords guild;
    // Story serial (M41): a 7-bit mask of which town installments have been heard
    // (see game/Story.hpp). Optional save field; old saves -> 0 (nothing heard).
    int storyMet = 0;
    // M97: the Hooded Goose cutscenes — scene ids already played (marked
    // BEFORE the scene pushes, so a later save can never replay it) and the
    // recorded heirloom choices as "scene:heirloom" strings (one per scene;
    // the grant fires only while a scene has no recorded choice). Both
    // optional save fields; old saves -> fresh story. See game/Cutscenes.hpp.
    std::vector<std::string> seenCutscenes;
    std::vector<std::string> heirloomChoices;
    // M100: how many post-finale jokes the stranger has told (drives the
    // deterministic joke cycle). Optional save field; old saves -> 0.
    int strangerJokesTold = 0;
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

// The level cap. Raised 50 -> 99 (owner decision, 2026-07-23): an overpowered
// ceiling so a fully-levelled party can actually answer the raised castle
// challenges. Stats grow linearly with level (base + growth x (level-1)), so a
// level-99 party earns roughly double the level-earned stats of a level-50 one;
// nothing in the growth curves comes near integer overflow at 99. `level` is
// already an int save field, so no save-schema change: old saves (level <= 50)
// load unchanged, and a level > 50 character round-trips like any other.
inline constexpr int kMaxLevel = 99;

// XP required to advance FROM `level` to the next level.
int xpToNext(int level);

// Adds XP to a character, leveling up (recomputing stats and healing the maxHP
// gained) as needed. Grants to the whole party with grantPartyXp.
void grantXp(Character& character, int xp, const content::ContentDatabase& db);
void grantPartyXp(Party& party, int xp, const content::ContentDatabase& db);

}  // namespace cd
