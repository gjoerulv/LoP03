#include "score/Scoring.hpp"

#include <algorithm>

namespace cd::score {

namespace {
// Coefficients (tunable in the balance pass). The turn penalty is large relative
// to the bonuses so fewer battle turns dominates the ranking.
constexpr int kBase = 1000;
constexpr int kBossBonus = 500;
constexpr int kTurnPenalty = 12;     // per round
constexpr int kChestBonus = 120;     // per chest opened
constexpr int kDangerBonus = 60;     // per danger weight defeated
constexpr int kTreasureDivisor = 5;  // gold -> points
constexpr int kNoDeathBonus = 300;
constexpr int kEscapePenalty = 40;   // per escaped battle
constexpr int kWagerWin = 150;       // M20 wager event: completed, no deaths
constexpr int kWagerLoss = 100;      // completed with deaths
}  // namespace

ScoreBreakdown scoreBreakdown(const RunSummary& run) {
    ScoreBreakdown b;
    if (!run.completed) {
        return b;  // an unfinished run scores 0
    }
    b.base = kBase;
    b.bossBonus = kBossBonus;
    b.turnPenalty = kTurnPenalty * std::max(0, run.battleTurns);
    b.chestBonus = kChestBonus * std::max(0, run.chestsOpened);
    b.dangerBonus = kDangerBonus * std::max(0, run.dangerDefeated);
    b.treasureBonus = std::max(0, run.treasureGold) / kTreasureDivisor;
    b.noDeathBonus = run.noDeath ? kNoDeathBonus : 0;
    b.escapePenalty = kEscapePenalty * std::max(0, run.escapes);
    if (run.wagerAccepted) {
        b.wager = run.noDeath ? kWagerWin : -kWagerLoss;
    }

    // Town-ladder bonus (M32) and stakes penalty (M33): both are percentages of
    // the (non-negative) subtotal; the bonus is added and the penalty subtracted,
    // penalty after the bonus. pct 0 on both (town 1 / legacy / a stakes-raising
    // run) => total identical to pre-M32/M33.
    const int subtotal = b.base + b.bossBonus - b.turnPenalty + b.chestBonus + b.dangerBonus +
                         b.treasureBonus + b.noDeathBonus - b.escapePenalty + b.wager;
    const int posSubtotal = std::max(0, subtotal);
    b.townBonus = posSubtotal * std::max(0, run.townBonusPct) / 100;
    b.stakesPenalty = posSubtotal * std::max(0, run.stakesPenaltyPct) / 100;
    b.total = std::max(0, subtotal + b.townBonus - b.stakesPenalty);
    return b;
}

int computeScore(const RunSummary& run) { return scoreBreakdown(run).total; }

}  // namespace cd::score
