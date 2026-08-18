#pragma once

#include <cstdint>
#include <string>

#include "content/Enums.hpp"
#include "dungeon/DungeonModel.hpp"

// M55 per-theme rites: pure, raylib-free helpers shared by the generator, the
// resolution state, and the tests. A rite appears only in dungeons of its own
// theme; since the owner's 2026-08-17 leveling (generation v22) it ROLLS at
// kThemeRiteChancePct per floor like every other special event, instead of
// being forced onto the first event slot.

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

// M76: the Duckling Peddler. A rare event that REPLACES one plain rolled event
// (never a rite, the relic, or an elite challenge), decided by a PURE hash of
// the dungeon seed so no rng draw is consumed and every other roll of a seed
// stays byte-identical — generation stays v14 (the M52 additive precedent; the
// program's one generation bump is reserved for M82's floors). The peddler
// sells the Evil Duckling for a flat price and, per the owner's rule, will not
// deal while the party already owns one (checked at interaction time, so the
// dungeon a seed generates never depends on the party's bag).
inline constexpr const char* kEvilDucklingItemId = "evil_duckling";
inline constexpr int kDuckPeddlerChancePct = 10;
inline constexpr int kDuckPeddlerPriceGold = 300;

// Which of the `eligibleCount` plain rolled event slots the peddler takes for
// this seed, or -1 for none (the common case). Pure.
int duckPeddlerSlot(std::uint64_t seed, int eligibleCount);

// M93 (generation v17): two more pure-hash plain-event replacements on the
// peddler's exact contract (never a rite, the relic, or an elite challenge;
// no rng draw consumed). Dragonform rolls per DUNGEON inside generate();
// the Surveyor rolls per FLOOR inside generateFloors (multi-floor runs
// only — a 1-floor map has no fog to sell away).
inline constexpr int kDragonformChancePct = 8;
inline constexpr int kSurveyorChancePct = 25;
inline constexpr int kSurveyorPriceGold = 20;  // the owner's number, flat

// Slot picks (or -1 for none). Pure; each rides its own salt.
int dragonformSlot(std::uint64_t seed, int eligibleCount);
int surveyorSlot(std::uint64_t seed, int floorIndex, int eligibleCount);

// M103 (generation v19): six more per-dungeon replacements on the exact same
// contract, drawn sequentially after the peddler and dragonform (each from
// the plain slots the earlier draws left standing).
inline constexpr int kGoosePolymorphChancePct = 6;
inline constexpr int kSacrificeChancePct = 8;
inline constexpr int kLevelAltarChancePct = 6;
inline constexpr int kStrangerStoryChancePct = 8;
inline constexpr int kTokenExchangeChancePct = 8;
inline constexpr int kPatrolResetChancePct = 6;
int goosePolymorphSlot(std::uint64_t seed, int eligibleCount);
int sacrificeSlot(std::uint64_t seed, int eligibleCount);
int levelAltarSlot(std::uint64_t seed, int eligibleCount);
int strangerStorySlot(std::uint64_t seed, int eligibleCount);
int tokenExchangeSlot(std::uint64_t seed, int eligibleCount);
int patrolResetSlot(std::uint64_t seed, int eligibleCount);

// M104 (generation v20): the two gambling dens, same contract again.
inline constexpr int kReelsChancePct = 7;
inline constexpr int kBlackjackChancePct = 7;
int reelsSlot(std::uint64_t seed, int eligibleCount);
int blackjackSlot(std::uint64_t seed, int eligibleCount);

// Owner direction 2026-08-17 (generation v22): the theme rites are no longer
// FORCED onto every floor's first event slot — each floor rolls its theme's
// rite on the same pure-hash replacement contract as every other special
// event. 8% is the top of the band (dragonform/sacrifice), befitting a
// theme's signature, and the rite draws FIRST in the replacement pass so it
// gets first pick of the plain slots. The Royal Relic's (town, depth) chance
// table is deliberately untouched.
inline constexpr int kThemeRiteChancePct = 8;
int themeRiteSlot(std::uint64_t seed, int eligibleCount);

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

}  // namespace dungeon
}  // namespace cd
