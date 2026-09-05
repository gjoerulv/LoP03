#pragma once

#include <array>
#include <cstdint>

#include "game/BlackMarket.hpp"  // blackMarketHash: the project's pure-hash idiom

// M110: the patrol dispatcher. The M93 danger counter still rouses a patrol
// every 100 walked tiles; what answers is now a seeded MIXTURE (owner
// decision): 65% an ordinary patrol team, 10% the Golden Goose, 5% the
// Jester's lore question, 15% the treasure chests, 5% a Stranger "P" scene.
// The kind of the Nth patrol of a run is a PURE HASH of (runSeed, N) under
// its own salt - never an rng-stream draw - so a reload replays the same
// category, no other roll of the seed moves, and a debug override for one
// patrol can never perturb the underlying sequence (it simply replaces the
// resolved kind for that trigger; the hash is untouched).

namespace cd::dungeon {

enum class PatrolKind { Normal, GoldenGoose, Lore, Chests, StrangerP };
inline constexpr int kPatrolKindCount = 5;

// The owner's mixture, in percent, in enum order. Sums to exactly 100.
inline constexpr std::array<int, kPatrolKindCount> kPatrolKindPct = {65, 10, 5, 15, 5};

// Distinct from the M93 team salt (kSaltPatrol in DungeonGenerator.cpp): the
// kind and the team of the same patrol are independent hashes.
inline constexpr std::uint64_t kSaltPatrolKind = 0x5A7301D15A7C44E1ull;

// The kind for a roll in [0, 100): cumulative thresholds over kPatrolKindPct.
inline PatrolKind patrolKindForRoll(int roll) {
    int upper = 0;
    for (int i = 0; i < kPatrolKindCount; ++i) {
        upper += kPatrolKindPct[static_cast<std::size_t>(i)];
        if (roll < upper) {
            return static_cast<PatrolKind>(i);
        }
    }
    return PatrolKind::Normal;  // roll >= 100 is impossible; defensive
}

// The kind of the `patrolIndex`-th patrol (0-based) of the run seeded by
// `runSeed`. Deterministic and reload-honest by construction.
inline PatrolKind patrolKindFor(std::uint64_t runSeed, int patrolIndex) {
    const std::uint64_t h =
        blackMarketHash(runSeed, kSaltPatrolKind + static_cast<std::uint64_t>(patrolIndex));
    return patrolKindForRoll(static_cast<int>(h % 100));
}

// How many of the patrols BEFORE `patrolIndex` (indices 0..patrolIndex-1)
// were of `kind` - the per-kind ordinal that drives the lore pool walk and
// the P-scene cycle, derived so it is reload-honest like everything else.
inline int patrolKindCount(std::uint64_t runSeed, int patrolIndex, PatrolKind kind) {
    int n = 0;
    for (int i = 0; i < patrolIndex; ++i) {
        if (patrolKindFor(runSeed, i) == kind) {
            ++n;
        }
    }
    return n;
}

inline const char* patrolKindName(PatrolKind kind) {
    switch (kind) {
        case PatrolKind::Normal: return "Normal";
        case PatrolKind::GoldenGoose: return "Golden Goose";
        case PatrolKind::Lore: return "Lore";
        case PatrolKind::Chests: return "Chests";
        case PatrolKind::StrangerP: return "P";
    }
    return "Normal";
}

}  // namespace cd::dungeon
