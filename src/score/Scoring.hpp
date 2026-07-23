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
    // Score wager event (M20): accepted before the boss, pays +150 on a
    // completed no-death run, costs 100 on a completed run with deaths. The
    // stake is fixed and shown before accepting; unfinished runs still
    // score 0.
    bool wagerAccepted = false;
    // Town-ladder score bonus (M32): a percent applied to the whole subtotal for
    // clearing a higher town. 0 for town 1 / legacy runs -> no change.
    int townBonusPct = 0;
    // Stakes-escalation penalty (M33): a percent (0..90) subtracted from the
    // subtotal for a run that did not raise the stakes. 0 -> no change.
    int stakesPenaltyPct = 0;
    // Unlockable-class modifier (M45): the party's per-member `scoreModPct`
    // values added together (e.g. two Dragons and a Jester = -35). Applied to the
    // subtotal like the town bonus. 0 for any party of the six original classes,
    // so nothing changes for them.
    int classModPct = 0;
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
    int wager = 0;  // +150 / -100 / 0 (M20 wager event)
    int townBonus = 0;  // town-ladder bonus applied to the subtotal (M32)
    int stakesPenalty = 0;  // stakes penalty subtracted from the subtotal (M33, >= 0)
    int classMod = 0;  // unlockable-class modifier on the subtotal (M45, may be < 0)
    int total = 0;
};

ScoreBreakdown scoreBreakdown(const RunSummary& run);
int computeScore(const RunSummary& run);  // == scoreBreakdown(run).total

}  // namespace cd::score
