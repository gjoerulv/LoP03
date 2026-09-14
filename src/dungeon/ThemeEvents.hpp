#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "content/Enums.hpp"
#include "dungeon/DungeonModel.hpp"

// Pure, raylib-free event helpers shared by the generator, the resolution
// states, and the tests. Since the owner's 2026-08-28 equal-weighting ruling
// (generation v23) the event system has exactly two tiers: the six STAPLE
// events keep the base roll's frequency, and every ENCOUNTER event — the
// theme's rite (own theme only) plus the ten global kinds — rolls the same
// kEncounterChancePct per floor via the registry below. The Royal Relic keeps
// its own (town, depth) table, and the Surveyor is a fog-gated utility, not a
// tier member.

namespace cd {
namespace content {
class ContentDatabase;
}
namespace dungeon {

// The rite belonging to a theme. RoomEventKind::None for an unknown/empty theme
// id, so the generator rolls nothing there and empty-theme generation stays
// byte-identical.
RoomEventKind themeEventKind(const std::string& themeId);

// The rarity one tier up. Legendary is the ceiling — it returns Legendary
// unchanged, which the Armory Ghost reads as its refusal.
content::Rarity nextRarityUp(content::Rarity r);

// The Armory Ghost's return: a seeded equippable item of the SAME slot as
// `tradedId` and the NEXT rarity tier up, chosen from `hash`. Empty when the
// trade cannot be honoured: `tradedId` is unknown or not equippable, it is
// legendary (refusal), or no eligible item of the target rarity+slot exists.
std::string armoryGhostUpgrade(const content::ContentDatabase& db,
                               const std::string& tradedId, std::uint64_t hash);

// Miner's Cache wound: one third of a member's max HP. The caller clamps the
// member's HP to >= 1, so the cache is never fatal.
inline int minersCacheWound(int maxHp) { return maxHp / 3; }

// Miner's Cache gold: strictly above the biggest possible trapped chest at this
// depth. A trapped chest is base gold rng(10,30)*depth plus the trapped add
// 25*depth+15, so its maximum is 55*depth+15; this stays above that for every
// depth (~1.5x the average trapped chest) and comes WITH a guaranteed item.
inline int minersCacheGold(int depth) {
    const int d = depth < 1 ? 1 : depth;
    return 68 * d + 25;  // > 55*d + 15 for all d >= 1
}

// The Elder Root's price (town-scaled, affordable against a run's clear gold) and
// the XP each party member gains. Battle XP is flat in this game, so "one elite
// battle" is only mildly depth/town-scaled here; sim-checked in the tests to land
// in the elite-battle band.
inline int elderRootPrice(int town, int depth) {
    const int t = town < 1 ? 1 : town;
    const int d = depth < 1 ? 1 : depth;
    return 60 + 40 * d + 50 * t;
}
inline int elderRootXp(int town, int depth) {
    const int t = town < 1 ? 1 : town;
    const int d = depth < 1 ? 1 : depth;
    return 90 + 6 * d + 6 * t;  // ~one elite battle (each member; XP is flat)
}

// A deterministic hash for resolution-time draws (the RoyalRelic precedent): a
// pure function of the dungeon seed, the room index, and a per-use salt, so a
// reload reproduces the same outcome rather than rerolling it.
std::uint64_t themeEventHash(std::uint64_t seed, int roomIndex, std::uint64_t salt);

// M76: the Duckling Peddler's wares (its appearance roll lives in the
// encounter registry below since v23). The peddler sells the Evil Duckling
// for a flat price and, per the owner's rule, will not deal while the party
// already owns one (checked at interaction time, so the dungeon a seed
// generates never depends on the party's bag).
inline constexpr const char* kEvilDucklingItemId = "evil_duckling";
inline constexpr int kDuckPeddlerPriceGold = 300;

// M93: the Surveyor — a utility purchase, deliberately NOT an encounter-tier
// member (owner ruling 2026-08-28). It exists only where the map starts
// fogged (FloorContext.fogged — the owner's check; today that is multi-floor
// descents) and keeps its own 25% so fog management stays purchasable. Since
// v23 it draws BEFORE the encounter tier instead of dead last, so its
// effective rate no longer starves (~15% pre-v23). Rolls from
// (runSeed, floorIndex) so every floor of a run answers independently.
inline constexpr int kSurveyorChancePct = 25;
inline constexpr int kSurveyorPriceGold = 20;  // the owner's number, flat
int surveyorSlot(std::uint64_t seed, int floorIndex, int eligibleCount);

// The encounter tier (owner direction 2026-08-28, generation v23): one shared
// chance for ALL encounter events. The pre-v23 chain of per-milestone
// percents (10/8/7/6) drawn in a fixed order gave the Duckling Peddler ~3x a
// gambling den's effective rate — accretion, not design. Every encounter
// still replaces only a plain STAPLE slot: never a relic, an elite challenge
// (whose team would be orphaned), or another encounter. All rolls are pure
// hashes of the floor seed — no generation-stream draw, reloads can never
// reroll, and each event keeps its own salt pair.
inline constexpr int kEncounterChancePct = 8;

struct EncounterDef {
    RoomEventKind kind = RoomEventKind::None;
    std::uint64_t saltAppears = 0;  // the appearance roll's salt
    std::uint64_t saltSlot = 0;     // the room pick's salt
};

// The ten GLOBAL encounter events (all towns, all themes), listed in the
// historical draw order — a stable identity order for tests and docs, NOT a
// priority (contention is shuffled). A future global event is one row here
// with a fresh salt pair, plus its content and resolution state.
const std::array<EncounterDef, 10>& encounterRegistry();

// The theme's rite as a registry-shaped entry (kind None for an unknown or
// empty theme id — encounterFires answers false for it).
EncounterDef themeRiteEncounter(const std::string& themeId);

// The pure appearance roll (kEncounterChancePct of 100) and room pick
// (uniform over `eligibleCount`, -1 when none) for one encounter.
bool encounterFires(std::uint64_t seed, const EncounterDef& e);
int encounterPick(std::uint64_t seed, const EncounterDef& e, int eligibleCount);

// The contention shuffle: a pure Fisher-Yates over the fired list. The
// generator takes survivors from the front while plain slots remain, so on a
// starved floor every fired encounter has the SAME survival chance — no kind
// outranks another.
void encounterContentionShuffle(std::uint64_t seed, std::vector<EncounterDef>& fired);

// M80: the content-layer flavor id for an event kind (data/event_flavor.json,
// content::kEventFlavorIds). Empty for None. A test holds this mapping and
// the content-side vocabulary in lockstep.
inline const char* eventFlavorId(RoomEventKind kind) {
    switch (kind) {
        case RoomEventKind::Shrine: return "shrine";
        case RoomEventKind::HealingSpring: return "healing_spring";
        case RoomEventKind::Merchant: return "merchant";
        case RoomEventKind::EliteChallenge: return "elite_challenge";
        case RoomEventKind::ScoreWager: return "score_wager";
        case RoomEventKind::RestToken: return "rest_token";
        case RoomEventKind::RoyalRelic: return "royal_relic";
        case RoomEventKind::ArmoryGhost: return "armory_ghost";
        case RoomEventKind::MinersCache: return "miners_cache";
        case RoomEventKind::ElderRoot: return "elder_root";
        case RoomEventKind::DuckPeddler: return "duck_peddler";
        case RoomEventKind::Surveyor: return "surveyor";      // M93
        case RoomEventKind::Dragonform: return "dragonform";  // M93
        case RoomEventKind::GoosePolymorph: return "goose_polymorph";  // M103
        case RoomEventKind::Sacrifice: return "sacrifice";             // M103
        case RoomEventKind::LevelAltar: return "level_altar";          // M103
        case RoomEventKind::StrangerStory: return "stranger_story";    // M103
        case RoomEventKind::TokenExchange: return "token_exchange";    // M103
        case RoomEventKind::PatrolReset: return "patrol_reset";        // M103
        case RoomEventKind::Reels: return "reels";                     // M104
        case RoomEventKind::Blackjack: return "blackjack";             // M104
        case RoomEventKind::GoosyFlock: return "goosy_flock";          // M106
        case RoomEventKind::None: break;
    }
    return "";
}

// M110 (debug tooling): the event kinds a dev-only one-shot may substitute
// for the next faced plain event AT INTERACTION TIME — exactly the kinds
// whose resolution reads nothing baked at generation. Shrine, Merchant,
// Elder Root, Surveyor, Miner's Cache and the Duck Peddler read
// RoomEvent.goldCost / itemId; the Elite Challenge needs its pre-generated
// team; the Royal Relic keeps its own table outside the shuffled kinds — none
// of those are here. A test pins this reading against the model.
inline const std::vector<RoomEventKind>& debugSubstitutableEventKinds() {
    static const std::vector<RoomEventKind> kinds = {
        RoomEventKind::HealingSpring, RoomEventKind::ScoreWager,   RoomEventKind::RestToken,
        RoomEventKind::ArmoryGhost,   RoomEventKind::Dragonform,   RoomEventKind::GoosePolymorph,
        RoomEventKind::Sacrifice,     RoomEventKind::LevelAltar,   RoomEventKind::StrangerStory,
        RoomEventKind::TokenExchange, RoomEventKind::PatrolReset,  RoomEventKind::Reels,
        RoomEventKind::Blackjack,     RoomEventKind::GoosyFlock,
    };
    return kinds;
}

}  // namespace dungeon
}  // namespace cd
