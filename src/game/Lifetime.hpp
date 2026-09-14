#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include "game/WorldLadder.hpp"  // kTownCount / clampTown (a leaf header - no Party.hpp cycle)

// M109: the persistent LIFETIME LEDGER of a save - cumulative, display-only
// statistics that keep growing for as long as the save is played. Every
// counter is 64-bit because nothing here ever resets; nothing here is ever
// read by battle resolution, generation, AI, or scoring (the ledger is a
// mirror, never an input). Recording happens at the authoritative seams
// (the battle observer, the gold/token ledger, the run exits, the play
// clock); zero-stakes surfaces (the sparring mirror, the Simulator, the
// editor, capture scenes, tests) never record.
//
// Per-member statistics are keyed by PARTY SLOT (0..3), never by name or
// class: slots are fixed for the life of a save (members are only appended at
// creation; Gooseform/Dragonform swap the Character AT a slot and restore
// it), so a renamed or transformed member keeps its history by construction.
//
// Persistence: one additive optional "lifetime" object on the save (schema
// stays v1). A save without the block starts every counter at zero, sets
// `migrated`, and backfills only what is genuinely derivable (the M42
// biggest-hit record). The field tables at the bottom keep the writer, the
// reader, and the tests in lockstep - a counter added to a struct is added
// to its table, nowhere else.

namespace cd {

using LifetimeCount = std::int64_t;

// == kMaxPartySize (game/Party.hpp); duplicated on the RunStats.hpp precedent
// so this header stays free of Party.hpp (which includes it).
inline constexpr std::size_t kLifetimeMemberSlots = 4;

struct MemberLifetime {
    LifetimeCount damageDealt = 0;
    LifetimeCount biggestHit = 0;
    LifetimeCount finishingBlows = 0;   // enemy units this member felled
    LifetimeCount timesKo = 0;
    LifetimeCount damageTaken = 0;
    LifetimeCount healingDone = 0;
    LifetimeCount healingReceived = 0;
    LifetimeCount revivesPerformed = 0;
    LifetimeCount skillsCast = 0;
    LifetimeCount statusesInflicted = 0;
    LifetimeCount scrollsLearned = 0;   // scroll-teach events (learnsets are derived, never counted)
};

struct CombatLifetime {
    LifetimeCount battlesWon = 0;
    LifetimeCount battlesLost = 0;
    LifetimeCount playerEscapes = 0;
    LifetimeCount battleTurns = 0;
    LifetimeCount enemiesKo = 0;        // raw enemy-unit KOs (clones included)
    LifetimeCount bossesDefeated = 0;   // boss ENCOUNTERS won (a clone never counts)
    LifetimeCount damageDealt = 0;
    LifetimeCount damageTaken = 0;
    LifetimeCount healing = 0;
    LifetimeCount statusesApplied = 0;
    LifetimeCount revives = 0;
    LifetimeCount summonsUsed = 0;
    LifetimeCount guardUses = 0;
    LifetimeCount highestHit = 0;
    int highestHitMember = -1;          // party slot, -1 = unknown (migrated saves)
};

struct EconomyLifetime {
    LifetimeCount goldEarned = 0;
    LifetimeCount goldSpent = 0;
    LifetimeCount goldLost = 0;         // the dungeon defeat halving - never "spent"
    LifetimeCount largestPurse = 0;
    LifetimeCount itemsBought = 0;
    LifetimeCount equipmentBought = 0;
    LifetimeCount innGold = 0;
    LifetimeCount passivesBought = 0;
    LifetimeCount scrollsLearned = 0;
    LifetimeCount levelUps = 0;
    LifetimeCount treasureFound = 0;    // items/equipment found (chests, caches, prizes, drops)
    LifetimeCount tokensEarned = 0;
    LifetimeCount tokensSpent = 0;
    LifetimeCount mapTreasures = 0;     // town treasure digs completed
    LifetimeCount curiosFound = 0;
};

struct TownLifetime {
    LifetimeCount attempts = 0;
    LifetimeCount clears = 0;
    LifetimeCount wipes = 0;
    LifetimeCount retreats = 0;
    LifetimeCount battlesWon = 0;
    LifetimeCount bossesDefeated = 0;
    LifetimeCount patrols = 0;
    LifetimeCount goldEarned = 0;
    LifetimeCount goldSpent = 0;
    LifetimeCount bestScore = 0;
};

struct PatrolLifetime {
    LifetimeCount total = 0;
    LifetimeCount normal = 0;
    LifetimeCount geeseMet = 0;
    LifetimeCount geeseDefeated = 0;
    LifetimeCount geeseEscaped = 0;
    LifetimeCount loreAttempted = 0;
    LifetimeCount loreCorrect = 0;
    LifetimeCount loreWrong = 0;
    LifetimeCount jesterPunish = 0;
    LifetimeCount aoePunish = 0;
    LifetimeCount rewardChests = 0;
    LifetimeCount mimicsRevealed = 0;
    LifetimeCount mimicsDefeated = 0;
    LifetimeCount emptyChests = 0;
    LifetimeCount strangerScenes = 0;
};

struct ExploreLifetime {
    LifetimeCount runsAttempted = 0;
    LifetimeCount runsCompleted = 0;
    LifetimeCount floorsCleared = 0;
    LifetimeCount tilesWalked = 0;      // dungeon tiles (the M93 counter's own unit)
    LifetimeCount chestsOpened = 0;
    LifetimeCount eventsResolved = 0;
    LifetimeCount eternalFloors = 0;    // cumulative Eternal floors (the best is Party.eternalBestFloors)
    LifetimeCount kingDefeats = 0;
    LifetimeCount duckDefeats = 0;
    LifetimeCount dragonDefeats = 0;
    LifetimeCount guildMasterDefeats = 0;
    LifetimeCount playSeconds = 0;      // active play only (focused, not paused in a menu)
};

struct LifetimeStats {
    std::array<MemberLifetime, kLifetimeMemberSlots> members{};
    CombatLifetime combat;
    EconomyLifetime economy;
    std::array<TownLifetime, kTownCount> towns{};  // index = town - 1
    PatrolLifetime patrols;
    ExploreLifetime explore;
    // Stable content id (enemy or boss) -> times defeated. Bosses count once
    // per encounter won; minions per unit felled; a summoned clone never.
    std::map<std::string, LifetimeCount> defeats;
    // True when this save predates the ledger: counters started at zero on
    // this version (the summary says so); only the biggest hit was backfilled.
    bool migrated = false;
};

inline TownLifetime& lifetimeTown(LifetimeStats& s, int town) {
    return s.towns[static_cast<std::size_t>(clampTown(town) - 1)];
}
inline const TownLifetime& lifetimeTown(const LifetimeStats& s, int town) {
    return s.towns[static_cast<std::size_t>(clampTown(town) - 1)];
}

inline bool validLifetimeSlot(int slot) {
    return slot >= 0 && slot < static_cast<int>(kLifetimeMemberSlots);
}

// Records a single hit dealt by `slot` (party member) for the per-member and
// global damage tallies, the member's biggest hit, and the save-wide highest
// hit with its author. Pure; the observer and the tests share it.
inline void recordLifetimeHit(LifetimeStats& s, int slot, LifetimeCount damage) {
    if (damage <= 0) {
        return;
    }
    s.combat.damageDealt += damage;
    if (damage > s.combat.highestHit) {
        s.combat.highestHit = damage;
        s.combat.highestHitMember = validLifetimeSlot(slot) ? slot : -1;
    }
    if (validLifetimeSlot(slot)) {
        MemberLifetime& m = s.members[static_cast<std::size_t>(slot)];
        m.damageDealt += damage;
        if (damage > m.biggestHit) {
            m.biggestHit = damage;
        }
    }
}

// The one derivable backfill for a save that predates the ledger: the M42
// biggest-hit record is the same fact as the lifetime highest hit (its author
// was never recorded, so the member stays unknown). Everything else starts at
// zero - history is never fabricated.
inline void migrateLifetime(LifetimeStats& s, int recordBiggestHit) {
    s = LifetimeStats{};
    s.migrated = true;
    s.combat.highestHit = recordBiggestHit > 0 ? recordBiggestHit : 0;
    s.combat.highestHitMember = -1;
}

// --- Field tables: the single source of truth for serialization ---------
// Every LifetimeCount member of every group appears exactly once with its
// JSON key. The save writer, the save reader, and the round-trip tests all
// iterate these, so the three can never disagree about a field.

template <typename Group>
struct LifetimeField {
    const char* key;
    LifetimeCount Group::*member;
};

inline constexpr std::array<LifetimeField<MemberLifetime>, 11> kMemberLifetimeFields = {{
    {"damageDealt", &MemberLifetime::damageDealt},
    {"biggestHit", &MemberLifetime::biggestHit},
    {"finishingBlows", &MemberLifetime::finishingBlows},
    {"timesKo", &MemberLifetime::timesKo},
    {"damageTaken", &MemberLifetime::damageTaken},
    {"healingDone", &MemberLifetime::healingDone},
    {"healingReceived", &MemberLifetime::healingReceived},
    {"revivesPerformed", &MemberLifetime::revivesPerformed},
    {"skillsCast", &MemberLifetime::skillsCast},
    {"statusesInflicted", &MemberLifetime::statusesInflicted},
    {"scrollsLearned", &MemberLifetime::scrollsLearned},
}};

inline constexpr std::array<LifetimeField<CombatLifetime>, 14> kCombatLifetimeFields = {{
    {"battlesWon", &CombatLifetime::battlesWon},
    {"battlesLost", &CombatLifetime::battlesLost},
    {"playerEscapes", &CombatLifetime::playerEscapes},
    {"battleTurns", &CombatLifetime::battleTurns},
    {"enemiesKo", &CombatLifetime::enemiesKo},
    {"bossesDefeated", &CombatLifetime::bossesDefeated},
    {"damageDealt", &CombatLifetime::damageDealt},
    {"damageTaken", &CombatLifetime::damageTaken},
    {"healing", &CombatLifetime::healing},
    {"statusesApplied", &CombatLifetime::statusesApplied},
    {"revives", &CombatLifetime::revives},
    {"summonsUsed", &CombatLifetime::summonsUsed},
    {"guardUses", &CombatLifetime::guardUses},
    {"highestHit", &CombatLifetime::highestHit},
}};

inline constexpr std::array<LifetimeField<EconomyLifetime>, 15> kEconomyLifetimeFields = {{
    {"goldEarned", &EconomyLifetime::goldEarned},
    {"goldSpent", &EconomyLifetime::goldSpent},
    {"goldLost", &EconomyLifetime::goldLost},
    {"largestPurse", &EconomyLifetime::largestPurse},
    {"itemsBought", &EconomyLifetime::itemsBought},
    {"equipmentBought", &EconomyLifetime::equipmentBought},
    {"innGold", &EconomyLifetime::innGold},
    {"passivesBought", &EconomyLifetime::passivesBought},
    {"scrollsLearned", &EconomyLifetime::scrollsLearned},
    {"levelUps", &EconomyLifetime::levelUps},
    {"treasureFound", &EconomyLifetime::treasureFound},
    {"tokensEarned", &EconomyLifetime::tokensEarned},
    {"tokensSpent", &EconomyLifetime::tokensSpent},
    {"mapTreasures", &EconomyLifetime::mapTreasures},
    {"curiosFound", &EconomyLifetime::curiosFound},
}};

inline constexpr std::array<LifetimeField<TownLifetime>, 10> kTownLifetimeFields = {{
    {"attempts", &TownLifetime::attempts},
    {"clears", &TownLifetime::clears},
    {"wipes", &TownLifetime::wipes},
    {"retreats", &TownLifetime::retreats},
    {"battlesWon", &TownLifetime::battlesWon},
    {"bossesDefeated", &TownLifetime::bossesDefeated},
    {"patrols", &TownLifetime::patrols},
    {"goldEarned", &TownLifetime::goldEarned},
    {"goldSpent", &TownLifetime::goldSpent},
    {"bestScore", &TownLifetime::bestScore},
}};

inline constexpr std::array<LifetimeField<PatrolLifetime>, 15> kPatrolLifetimeFields = {{
    {"total", &PatrolLifetime::total},
    {"normal", &PatrolLifetime::normal},
    {"geeseMet", &PatrolLifetime::geeseMet},
    {"geeseDefeated", &PatrolLifetime::geeseDefeated},
    {"geeseEscaped", &PatrolLifetime::geeseEscaped},
    {"loreAttempted", &PatrolLifetime::loreAttempted},
    {"loreCorrect", &PatrolLifetime::loreCorrect},
    {"loreWrong", &PatrolLifetime::loreWrong},
    {"jesterPunish", &PatrolLifetime::jesterPunish},
    {"aoePunish", &PatrolLifetime::aoePunish},
    {"rewardChests", &PatrolLifetime::rewardChests},
    {"mimicsRevealed", &PatrolLifetime::mimicsRevealed},
    {"mimicsDefeated", &PatrolLifetime::mimicsDefeated},
    {"emptyChests", &PatrolLifetime::emptyChests},
    {"strangerScenes", &PatrolLifetime::strangerScenes},
}};

inline constexpr std::array<LifetimeField<ExploreLifetime>, 12> kExploreLifetimeFields = {{
    {"runsAttempted", &ExploreLifetime::runsAttempted},
    {"runsCompleted", &ExploreLifetime::runsCompleted},
    {"floorsCleared", &ExploreLifetime::floorsCleared},
    {"tilesWalked", &ExploreLifetime::tilesWalked},
    {"chestsOpened", &ExploreLifetime::chestsOpened},
    {"eventsResolved", &ExploreLifetime::eventsResolved},
    {"eternalFloors", &ExploreLifetime::eternalFloors},
    {"kingDefeats", &ExploreLifetime::kingDefeats},
    {"duckDefeats", &ExploreLifetime::duckDefeats},
    {"dragonDefeats", &ExploreLifetime::dragonDefeats},
    {"guildMasterDefeats", &ExploreLifetime::guildMasterDefeats},
    {"playSeconds", &ExploreLifetime::playSeconds},
}};

// The defeat ledger's defensive load cap (a hand-edited file cannot balloon
// memory); the shipped roster is under a hundred ids.
inline constexpr std::size_t kLifetimeDefeatEntryCap = 8192;

}  // namespace cd
