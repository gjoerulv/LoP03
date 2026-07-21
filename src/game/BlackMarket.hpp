#pragma once

#include <cstdint>
#include <string>

// Pure, raylib-free black-market rules (M34). After a stakes-raising completed
// run in town >= 2, a seeded 20% roll may spawn a market NPC selling one
// legendary piece for gold or legendary tokens. Every outcome (whether it
// spawns, which tile, which item) is a deterministic function of the run's
// dungeon seed (plus the stakes), so reloading the entry autosave and replaying
// the same run cannot reroll a miss into a hit. Headless-tested.

namespace cd {

inline constexpr int kBlackMarketChancePct = 20;
inline constexpr int kBlackMarketBasePrice = 5000;    // gold floor (town 2)
inline constexpr int kBlackMarketPricePerTown = 750;  // + per town above the floor
inline constexpr int kBlackMarketTokenPrice = 3;      // legendary tokens
inline constexpr int kBlackMarketMinTown = 2;         // never spawns in town 1

// The market's current offer, held on Party and saved as optional fields.
struct BlackMarketOffer {
    bool present = false;
    int town = 0;
    std::string itemId;
    int priceGold = 0;
    int tileX = 0;
    int tileY = 0;
};

// Seeded free plaza tiles in the fixed 26x15 town layout (all open Ground,
// avoiding buildings, their doors, the road exits, and the player spawn 12,8).
struct MarketTile {
    int x;
    int y;
};
inline constexpr int kBlackMarketTileCount = 5;
inline constexpr MarketTile kBlackMarketTiles[kBlackMarketTileCount] = {
    {5, 7}, {20, 7}, {9, 8}, {16, 6}, {12, 6},
};

// SplitMix64-style finalizer for a well-mixed deterministic hash.
inline std::uint64_t blackMarketMix(std::uint64_t x) {
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return x;
}

inline std::uint64_t blackMarketHash(std::uint64_t seed, std::uint64_t salt) {
    return blackMarketMix(seed ^ (0x9E3779B97F4A7C15ull * (salt + 1)));
}

// Does a stakes-raising completed run at (town, depth) spawn a market? 20%,
// deterministic from the run seed and the stakes.
inline bool blackMarketRolls(std::uint64_t seed, int town, int depth) {
    const std::uint64_t salt =
        static_cast<std::uint64_t>(town) * 1000u + static_cast<std::uint64_t>(depth);
    return static_cast<int>(blackMarketHash(seed, salt) % 100) < kBlackMarketChancePct;
}

// All the M34 spawn conditions in one testable place: the market appears after a
// completed, scoring run (`completedWithScore`) that raised the stakes
// (`raisedStakes`), in town >= 2, that also wins the seeded 20% roll. This is the
// exact predicate DungeonState::completeDungeon uses to spawn the offer.
inline bool blackMarketShouldSpawn(bool completedWithScore, bool raisedStakes, int town,
                                   std::uint64_t seed, int depth) {
    return completedWithScore && raisedStakes && town >= kBlackMarketMinTown &&
           blackMarketRolls(seed, town, depth);
}

// Gold price for a market in `town`: 5000 floor + modest per-town scaling.
inline int blackMarketPriceGold(int town) {
    const int t = town < kBlackMarketMinTown ? kBlackMarketMinTown : town;
    return kBlackMarketBasePrice + (t - kBlackMarketMinTown) * kBlackMarketPricePerTown;
}

// Which plaza tile (index into kBlackMarketTiles) the NPC occupies, seeded.
inline int blackMarketTileIndex(std::uint64_t seed) {
    return static_cast<int>(blackMarketHash(seed, 0x7113u) % kBlackMarketTileCount);
}

// Which legendary (index into a sorted id list of size n) is offered, seeded.
inline int blackMarketItemIndex(std::uint64_t seed, int n) {
    if (n <= 0) {
        return 0;
    }
    return static_cast<int>(blackMarketHash(seed, 0x1737u) % static_cast<std::uint64_t>(n));
}

}  // namespace cd
