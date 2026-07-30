#pragma once

#include <cstdint>
#include <string>

#include "content/Enums.hpp"
#include "dungeon/DungeonModel.hpp"

// M55 per-theme rites: pure, raylib-free helpers shared by the generator, the
// resolution state, and the tests. One rite is guaranteed per dungeon of its
// theme (forced onto the first event slot) and never appears elsewhere.

namespace cd {
namespace content {
class ContentDatabase;
}
namespace dungeon {

// The rite guaranteed for a theme. RoomEventKind::None for an unknown/empty theme
// id, so the generator forces nothing there and empty-theme generation stays
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

}  // namespace dungeon
}  // namespace cd
