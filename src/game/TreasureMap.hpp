#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "game/BlackMarket.hpp"  // blackMarketMix (the shared SplitMix64 primitive)
#include "game/Castle.hpp"       // bossRushOrder (the 12 dungeon bosses, sorted)

// M65 — the town puzzle map (a Heroes of Might and Magic 2 homage). Secret
// Map Pieces hide in ~10% of dungeons (one per dungeon, generation v12);
// gathering FOUR reveals a treasure in the town of the run that yielded the
// fourth piece, guarded by a seeded roster boss at that dungeon's own boss
// scale. The dig pays one of six exclusive teaching scrolls (learned
// immediately), drawn without repetition; an exhausted pool pays a legendary
// token + gold instead. Repeatable cycles (owner decision). Pure state +
// rules; the states render and fight.

namespace cd {

inline constexpr int kMapPiecesNeeded = 4;

// The revealed treasure (an optional Party save record; inert by default).
struct TreasureReveal {
    bool active = false;
    int town = 1;         // where the dig spot appears
    std::string bossId;   // the seeded guard (a dungeon-roster boss)
    int scalePct = 100;   // the guard's stat scale — that dungeon's own boss scale
};

// The town dig-spot tile. Plaza ground like the black-market candidates
// (kBlackMarketTiles); its walkability is pinned by a unit test so the
// constant can never silently land inside a building.
inline constexpr int kDigTileX = 9;
inline constexpr int kDigTileY = 10;

// The six exclusive treasure scrolls, in award order (drawn without
// repetition — deterministic, no roll needed). value 0 keeps them out of
// every shop/chest/drop pool (the M44 valueless rule).
inline const std::array<const char*, 6>& treasureScrollPool() {
    static const std::array<const char*, 6> kPool = {
        "treasure_scroll_meteor",   "treasure_scroll_chain",  "treasure_scroll_mending",
        "treasure_scroll_wardrums", "treasure_scroll_doom",   "treasure_scroll_vanish",
    };
    return kPool;
}

// The next unawarded treasure scroll, or "" when the pool is spent (the dig
// then pays kTreasureTokenFallback + kTreasureGoldFallback instead).
inline std::string nextTreasureScroll(const std::vector<std::string>& awarded) {
    for (const char* id : treasureScrollPool()) {
        if (std::find(awarded.begin(), awarded.end(), id) == awarded.end()) {
            return id;
        }
    }
    return "";
}

inline constexpr int kTreasureTokenFallback = 1;
inline constexpr int kTreasureGoldFallback = 2500;

// The seeded treasure guard: a dungeon-roster boss (bossRushOrder — the 12
// sorted dungeon bosses; the King and the Duck keep their own arenas),
// picked by a pure hash of the dungeon seed that yielded the fourth piece.
inline std::string treasureGuardBossId(const content::ContentDatabase& content,
                                       std::uint64_t seed) {
    const std::vector<std::string> roster = bossRushOrder(content);
    if (roster.empty()) {
        return "";
    }
    constexpr std::uint64_t kSaltGuard = 0x4D415047ull;  // "MAPG"
    return roster[static_cast<std::size_t>(blackMarketMix(seed ^ kSaltGuard) % roster.size())];
}

}  // namespace cd
