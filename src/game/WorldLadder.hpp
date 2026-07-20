#pragma once

#include <algorithm>

// Pure, raylib-free town-ladder rules (M32): the seven-town difficulty/score
// ladder, the travel-unlock rule, and the (depth, town) stat combination.
// Headless-tested and the single source of the ladder curve, so the generator,
// scoring, save, and UI never hard-code it. Town 1 is the identity everywhere
// (x1.00 stats, +0% score) so pre-M32 behaviour and seeds are unchanged.

namespace cd {

inline constexpr int kTownCount = 7;

// Clamp any stored/loaded/derived town index into the valid 1..kTownCount range.
inline int clampTown(int town) {
    return town < 1 ? 1 : (town > kTownCount ? kTownCount : town);
}

// Enemy-stat multiplier as a percent, composed on top of depth scaling.
// Owner ladder (towns 1..7): +0 / 25 / 50 / 75 / 100 / 150 / 200 %.
inline int townScalePct(int town) {
    static constexpr int kPct[kTownCount] = {100, 125, 150, 175, 200, 250, 300};
    return kPct[clampTown(town) - 1];
}

// Score bonus as a percent of the run subtotal. Owner ladder (towns 1..7):
// +0 / 10 / 20 / 30 / 40 / 50 / 100 %.
inline int townScoreBonusPct(int town) {
    static constexpr int kPct[kTownCount] = {0, 10, 20, 30, 40, 50, 100};
    return kPct[clampTown(town) - 1];
}

// Final enemy stat-scale percent for a team, given the depth-derived base
// (100 + composition depth scaling) and the town. Multiplicative; town 1 leaves
// the base untouched, so town-1 generation is byte-identical to pre-M32.
inline int combineTownScale(int depthScaledPct, int town) {
    return depthScaledPct * townScalePct(town) / 100;
}

// Completing a run in `town` unlocks the exit to town+1 (capped at kTownCount).
// Returns the new highestUnlockedTown given the previous value; never lowers it.
inline int unlockAfterClear(int highestUnlocked, int town) {
    const int next = clampTown(town) + 1;
    return std::min(kTownCount, std::max(clampTown(highestUnlocked), next));
}

}  // namespace cd
