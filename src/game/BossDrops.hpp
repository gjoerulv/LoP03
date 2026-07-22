#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/BlackMarket.hpp"  // reuse blackMarketHash (the SplitMix64 primitive)

// Boss legendary & token drops (M39). On a boss kill in town >= 3 AND depth >= 4,
// two independent seeded rolls (off the run's dungeon seed) award legendary tokens
// and/or a legendary equipment piece. Rates ramp linearly with a combined
// town+depth progress from a low floor at t3/d4 to the owner-fixed caps at t7/d20
// (75% token / 30% legendary); a town-7 token drop pays 2 tokens. Every outcome is
// a deterministic function of (seed, town, depth), so reloading the entry autosave
// and replaying the same run reproduces the same drops (no reroll-on-reload), and
// nothing here uses wall-clock or unseeded RNG. The pure math below is header-only
// and unit-tested; the content pool + assembled roll live in BossDrops.cpp.

namespace cd {

namespace content {
class ContentDatabase;
}

// Eligibility gate (owner rule): town >= 3 and depth >= 4.
inline constexpr int kBossDropMinTown = 3;
inline constexpr int kBossDropMinDepth = 4;

// Ramp anchors: progress is 0 at (town 3, depth 4) and 1 at (town 7, depth 20),
// clamped so deeper/later runs never exceed the caps.
inline constexpr int kBossDropTownLo = 3;
inline constexpr int kBossDropTownHi = 7;
inline constexpr int kBossDropDepthLo = 4;
inline constexpr int kBossDropDepthHi = 20;

inline constexpr int kBossTokenChanceLoPct = 15;      // token chance at t3/d4
inline constexpr int kBossTokenChanceHiPct = 75;      // cap at t7/d20 (owner rule)
inline constexpr int kBossLegendaryChanceLoPct = 5;   // legendary chance at t3/d4
inline constexpr int kBossLegendaryChanceHiPct = 30;  // cap at t7/d20 (owner rule)
inline constexpr int kBossDropDoubleTokenTown = 7;    // town 7 pays 2 tokens

// Distinct salt namespace so the two rolls and the item pick are independent of
// one another and of the black-market stream on the same seed.
inline constexpr std::uint64_t kBossTokenSalt = 0xB0550D0170CE11ADull;
inline constexpr std::uint64_t kBossLegendarySalt = 0xB0550D01E6EDDA11ull;
inline constexpr std::uint64_t kBossLegendaryIndexSalt = 0xB0550D011D0C1DEEull;

inline int bossDropClampInt(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// A boss kill at (town, depth) is drop-eligible.
inline bool bossDropEligible(int town, int depth) {
    return town >= kBossDropMinTown && depth >= kBossDropMinDepth;
}

// Combined town+depth progress in [0, 1000] (fixed-point per-mille, no floating
// point in the deterministic path): the average of a town progress (0 at town 3,
// 1000 at town 7) and a depth progress (0 at depth 4, 1000 at depth 20), each
// clamped to its anchor span. Monotonic non-decreasing in both town and depth.
inline int bossDropProgressPerMille(int town, int depth) {
    const int townSpan = kBossDropTownHi - kBossDropTownLo;     // 4
    const int depthSpan = kBossDropDepthHi - kBossDropDepthLo;  // 16
    const int t = bossDropClampInt(town - kBossDropTownLo, 0, townSpan);
    const int d = bossDropClampInt(depth - kBossDropDepthLo, 0, depthSpan);
    const int townPm = t * 1000 / townSpan;
    const int depthPm = d * 1000 / depthSpan;
    return (townPm + depthPm) / 2;
}

// Linear ramp between floor and cap over the combined progress.
inline int bossTokenChancePct(int town, int depth) {
    const int pm = bossDropProgressPerMille(town, depth);
    return kBossTokenChanceLoPct +
           (kBossTokenChanceHiPct - kBossTokenChanceLoPct) * pm / 1000;
}

inline int bossLegendaryChancePct(int town, int depth) {
    const int pm = bossDropProgressPerMille(town, depth);
    return kBossLegendaryChanceLoPct +
           (kBossLegendaryChanceHiPct - kBossLegendaryChanceLoPct) * pm / 1000;
}

// The two independent seeded rolls (each < its chance % hits). Deterministic in
// (seed, town, depth); reload-proof.
inline bool bossTokenRolls(std::uint64_t seed, int town, int depth) {
    return static_cast<int>(blackMarketHash(seed, kBossTokenSalt) % 100) <
           bossTokenChancePct(town, depth);
}

inline bool bossLegendaryRolls(std::uint64_t seed, int town, int depth) {
    return static_cast<int>(blackMarketHash(seed, kBossLegendarySalt) % 100) <
           bossLegendaryChancePct(town, depth);
}

// How many tokens a token drop pays (owner rule: town 7 pays 2, else 1).
inline int bossTokenCount(int town) {
    return town >= kBossDropDoubleTokenTown ? 2 : 1;
}

// Which legendary (index into a sorted id list of size n) drops, seeded.
inline int bossLegendaryIndex(std::uint64_t seed, int n) {
    if (n <= 0) {
        return 0;
    }
    return static_cast<int>(blackMarketHash(seed, kBossLegendaryIndexSalt) %
                            static_cast<std::uint64_t>(n));
}

// The assembled outcome of a boss kill's drop rolls.
struct BossDropResult {
    int tokens = 0;             // legendary tokens awarded (0, 1, or 2)
    bool legendary = false;     // a legendary piece dropped
    std::string legendaryId;    // which one (empty if none / pool empty)

    bool any() const { return tokens > 0 || legendary; }
};

// The shared legendary drop pool: every Legendary equipment/relic id, sorted. The
// black market draws from the identical set, so the two systems cannot diverge.
std::vector<std::string> legendaryDropPool(const content::ContentDatabase& content);

// Rolls the full boss-drop outcome for a kill at (seed, town, depth). Returns an
// empty result when the town/depth gate is not met.
BossDropResult rollBossDrops(std::uint64_t seed, int town, int depth,
                             const content::ContentDatabase& content);

}  // namespace cd
