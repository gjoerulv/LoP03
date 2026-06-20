#pragma once

// Deterministic dungeon scoring. Ranking priority (per the design): completed?
// -> fewest battle turns -> most danger defeated -> most treasure. Score is 0
// for an unfinished run, and the per-round turn penalty keeps "fewest turns" the
// dominant factor. Pure; unit-tested.

namespace cd::score {

struct RunSummary {
    bool completed = false;   // the boss was defeated
    int battleTurns = 0;      // total rounds across all battles
    int dangerDefeated = 0;   // sum of defeated team danger weights
    int chestsOpened = 0;
    int treasureGold = 0;
    bool noDeath = true;      // no party member was KO'd during the run
    int escapes = 0;          // battles escaped from
};

struct ScoreBreakdown {
    int base = 0;
    int bossBonus = 0;
    int turnPenalty = 0;
    int chestBonus = 0;
    int dangerBonus = 0;
    int treasureBonus = 0;
    int noDeathBonus = 0;
    int escapePenalty = 0;
    int total = 0;
};

ScoreBreakdown scoreBreakdown(const RunSummary& run);
int computeScore(const RunSummary& run);  // == scoreBreakdown(run).total

}  // namespace cd::score
