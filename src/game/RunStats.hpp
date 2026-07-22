#pragma once

// Victory stats (M42): this-run combat tallies, accumulated live by BattleState
// and merged across a dungeon run by DungeonState. Display-only — derived from the
// same damage numbers the battle already produced, never fed back into any
// decision or RNG, so the pure Battle model and the Simulator stay untouched and
// battle resolution is byte-identical.

namespace cd {

inline constexpr int kRunStatsMembers = 4;  // == kMaxPartySize

struct RunStats {
    int totalDamage = 0;        // total damage the party dealt over the run
    int biggestHit = 0;         // largest single-target hit
    int statusesInflicted = 0;  // party skill-casts that carried a status
    int damageByMember[kRunStatsMembers] = {0, 0, 0, 0};  // MVP = the largest

    // Party index (0..3) of the member who dealt the most damage, or -1 if none.
    int mvpMember() const {
        int best = -1;
        int bestDmg = 0;
        for (int i = 0; i < kRunStatsMembers; ++i) {
            if (damageByMember[i] > bestDmg) {
                bestDmg = damageByMember[i];
                best = i;
            }
        }
        return best;
    }
};

}  // namespace cd
