#pragma once

#include <algorithm>

// Pure, raylib-free stakes-escalation rules (M33). A run's stakes = (townIndex,
// depth), compared lexicographically town-first against the previous completed
// run. A completed run that fails to raise the stakes accrues a score penalty
// (-15% per step, capped at -90%); one that raises them clears it. Headless-
// tested and the single source of the rule, so scoring, save, and the Guild
// forewarning always agree. All-zero is the fresh state: the first real run
// (town >= 1, depth >= 1) is strictly higher than (0,0), so it always raises and
// is unpenalized, and pre-M33 saves (fields absent -> 0) behave identically.

namespace cd {

inline constexpr int kStakesPenaltyPerStep = 15;   // percent per unraised run
inline constexpr int kStakesPenaltyMaxSteps = 6;   // 6 steps
inline constexpr int kStakesPenaltyCapPct =
    kStakesPenaltyPerStep * kStakesPenaltyMaxSteps;  // 90

// The previous completed run's stakes and how many consecutive completed runs
// have failed to raise them.
struct StakesState {
    int prevTown = 0;
    int prevDepth = 0;
    int penaltySteps = 0;
};

// Is (town, depth) strictly higher than the previous completed stakes,
// lexicographically with town most significant?
inline bool stakesRaised(const StakesState& s, int town, int depth) {
    if (town != s.prevTown) {
        return town > s.prevTown;
    }
    return depth > s.prevDepth;
}

// The penalty percent (0..90) a completed run at (town, depth) would incur:
// 0 if it raises the stakes, else (steps + 1) * 15, capped at 90.
inline int stakesPenaltyPct(const StakesState& s, int town, int depth) {
    if (stakesRaised(s, town, depth)) {
        return 0;
    }
    const int steps = std::min(kStakesPenaltyMaxSteps, s.penaltySteps + 1);
    return steps * kStakesPenaltyPerStep;
}

// The state after a completed (score > 0) run at (town, depth): the penalty step
// resets on a raise or grows one step (capped) otherwise, and the baseline moves
// to this run's stakes. Not called for score-0 runs (they neither grow the
// penalty nor move the baseline).
inline StakesState afterCompletedRun(const StakesState& s, int town, int depth) {
    StakesState next;
    next.prevTown = town;
    next.prevDepth = depth;
    next.penaltySteps = stakesRaised(s, town, depth)
                            ? 0
                            : std::min(kStakesPenaltyMaxSteps, s.penaltySteps + 1);
    return next;
}

// Clamp a loaded/derived penalty-step count into range.
inline int clampStakesSteps(int steps) {
    return steps < 0 ? 0 : (steps > kStakesPenaltyMaxSteps ? kStakesPenaltyMaxSteps : steps);
}

}  // namespace cd
