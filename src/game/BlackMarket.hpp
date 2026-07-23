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

// M52: a second, independent spawn path for top-end runs. Any beaten boss at
// town 7, depth >= 20 gets a 34 % roll — regardless of score, stakes, or a
// penalty that floors the score to 0. It rides a FRESH salt so it is a separate
// seeded stream from the 20 % stakes roll (a seed can win one and lose the
// other), and it is reload-proof the same way. The existing 20 % rule is
// unchanged and still applies alongside.
inline constexpr int kBlackMarketHighStakesChancePct = 34;
inline constexpr int kBlackMarketHighStakesTown = 7;
inline constexpr int kBlackMarketHighStakesDepth = 20;
// XOR salt constant that makes the high-stakes stream independent of the 20 %
// one (blackMarketHash mixes the salt, so a different salt is a different hash).
inline constexpr std::uint64_t kBlackMarketHighStakesSalt = 0xC0D4B7A552ull;

// The market's current offer, held on Party and saved as optional fields.
struct BlackMarketOffer {
    bool present = false;
    int town = 0;
    std::string itemId;
    int priceGold = 0;
    int tileX = 0;
    int tileY = 0;
};

// Seeded free plaza tiles in the compact 24x12 town layout (M50): all open
// Ground, clear of buildings, their doors, the road-trigger tiles, the bard
// spot, and the player spawn (11,5).
struct MarketTile {
    int x;
    int y;
};
inline constexpr int kBlackMarketTileCount = 5;
inline constexpr MarketTile kBlackMarketTiles[kBlackMarketTileCount] = {
    {5, 5}, {18, 5}, {9, 6}, {14, 6}, {5, 10},
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

// M52: the independent high-stakes roll (34%), on a fresh salt so it is a
// separate stream from the 20% one. Deterministic from the run seed, so it is
// reload-proof too.
inline bool blackMarketHighStakesRolls(std::uint64_t seed, int town, int depth) {
    const std::uint64_t salt =
        (static_cast<std::uint64_t>(town) * 1000u + static_cast<std::uint64_t>(depth)) ^
        kBlackMarketHighStakesSalt;
    return static_cast<int>(blackMarketHash(seed, salt) % 100) < kBlackMarketHighStakesChancePct;
}

// All the black-market spawn conditions in one testable place. The market
// appears after EITHER:
//   - M34: a completed, scoring run (`completedWithScore`) that raised the
//     stakes (`raisedStakes`), in town >= 2, that wins the seeded 20% roll; or
//   - M52: any completed run (`completed`, i.e. a beaten boss) at town 7,
//     depth >= 20 that wins the independent 34% roll — regardless of score,
//     stakes, or penalty.
// This is the exact predicate DungeonState::completeDungeon uses.
inline bool blackMarketShouldSpawn(bool completedWithScore, bool completed, bool raisedStakes,
                                   int town, std::uint64_t seed, int depth) {
    if (completedWithScore && raisedStakes && town >= kBlackMarketMinTown &&
        blackMarketRolls(seed, town, depth)) {
        return true;
    }
    if (completed && town >= kBlackMarketHighStakesTown && depth >= kBlackMarketHighStakesDepth &&
        blackMarketHighStakesRolls(seed, town, depth)) {
        return true;
    }
    return false;
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
