#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dungeon/DungeonModel.hpp"  // EnemyTeam
#include "game/BlackMarket.hpp"      // blackMarketHash (SplitMix64 stream)

// The castle (M40): a place ABOVE the town-7 ceiling — reached by a road from
// town 7 once any town-7 dungeon is cleared — that hosts the King and three
// challenges, with its own records and rewards kept entirely outside the dungeon
// scoreboard (M19 comparability preserved). `kCastleTown` is a distinct id that
// is NEVER assigned to `Party.currentTown` nor passed through
// `WorldLadder`/`clampTown` (which cap at town 7); the castle is its own
// `CastleState`, not a ladder town. All pure math is header-only and unit-tested;
// the content-driven team builders live in Castle.cpp.

namespace cd {

namespace content {
class ContentDatabase;
}

inline constexpr int kCastleTown = 8;  // distinct place, NOT a ladder town

enum class CastleChallenge { BossRush, Endless, King };

// --- Challenge scaling -----------------------------------------------------
// Every fight is a normal Battle whose team.statScalePct sets the enemy stat
// multiplier (buildBattle scales boss/enemy stats by statScalePct/100). These are
// SIM-TUNED endgame values (a t7/d8 dungeon boss is ~354 %; the party's real
// ceiling is well below the extreme t7/d20 ~570 %). The challenges' difficulty
// comes as much from ATTRITION (no free healing across a gauntlet) as from raw
// scale, so per-fight scales are moderate while the King — a solo boss with high
// base stats and a nasty passive/status kit — is the hardest single fight.
inline constexpr int kCastleBaselineScalePct = 300;  // endless wave-0 / reference

// Boss Rush: each solo boss at a moderate scale — the challenge is chaining all 12
// with no free healing.
inline constexpr int kBossRushScalePct = 330;

// The King: the hardest single fight (its base stats also sit above every dungeon
// boss). Sim-tuned to be beatable by a maxed party but a real war of attrition —
// its debuff-curses now deal damage every turn (M40 owner refinement), so the
// scale is lower than a pure-status King would need.
//
// M44 (owner decision, 2026-07-22): 420 -> 340. The King's BASE stats doubled,
// which compounds with this multiplier; at 420 % the fight demanded three copies
// of each 40 % relic plus the 15 % Dragon Crown. At 340 % the approved bar holds
// exactly — a maxed party with no counterplay still loses, while one Tax Sheets +
// one Evil Goose + Royal Snacks wins it. Evidence: `[king-report]`.
inline constexpr int kKingScalePct = 340;

// Endless wave W (0-based): starts at the baseline and escalates ~12 %pts/wave,
// so a maxed party pushes a meaningful distance before the scale overwhelms.
inline int endlessWaveScalePct(int wave) {
    const int w = wave < 0 ? 0 : wave;
    return kCastleBaselineScalePct + 12 * w;
}

// Endless team size grows with the wave, capped at 5.
inline int endlessWaveSize(int wave) {
    const int n = 2 + (wave < 0 ? 0 : wave) / 3;
    return n > 5 ? 5 : n;
}

// Fixed endless challenge seed: the wave sequence is the same each attempt, so a
// "best wave" record is a meaningful, reproducible measure of how far you pushed.
inline constexpr std::uint64_t kEndlessSeed = 0xCA57185EED1E5500ull;

// --- The King's content & reward identity ----------------------------------
inline constexpr const char* kKingBossId = "the_hollow_king";
inline constexpr const char* kKingLegendaryId = "sovereigns_regalia";  // King-only unique
inline constexpr const char* kKingTitle = "Breaker of the Hollow Throne";

// --- Records (persisted as optional Party save fields, NOT the scoreboard) ---
struct CastleRecords {
    int bossRushBestTurns = 0;  // 0 = never cleared; fewer is better
    int endlessBestWave = 0;    // 0 = never survived a wave; higher is better
    bool kingDefeated = false;
    int kingBestTurns = 0;      // 0 = never; fewer is better
    std::string kingTitle;      // the visible title earned for the first King kill

    bool bossRushCleared() const { return bossRushBestTurns > 0; }
    bool anyRecord() const {
        return bossRushBestTurns > 0 || endlessBestWave > 0 || kingDefeated;
    }
};

// A record "improves" on a first result or a better one; the caller records it,
// and pays the one-time reward only when it was the FIRST clear (guarded by the
// prior "never" state — see DungeonState/CastleChallengeState use). Pure.
inline bool bossRushImproved(const CastleRecords& r, int turns) {
    return turns > 0 && (r.bossRushBestTurns == 0 || turns < r.bossRushBestTurns);
}
inline bool endlessImproved(const CastleRecords& r, int wave) {
    return wave > r.endlessBestWave;
}
inline bool kingImproved(const CastleRecords& r, int turns) {
    return turns > 0 && (!r.kingDefeated || turns < r.kingBestTurns);
}

// --- One-time first-clear rewards ------------------------------------------
inline constexpr int kBossRushRewardTokens = 5;
inline constexpr int kBossRushRewardGold = 4000;
inline constexpr int kEndlessRewardTokens = 3;
inline constexpr int kEndlessRewardGold = 2500;
inline constexpr int kKingRewardTokens = 8;
inline constexpr int kKingRewardGold = 8000;

// --- Content-driven team builders (Castle.cpp) -----------------------------
// The Boss Rush order: every boss id EXCEPT the King, sorted (stable/deterministic).
std::vector<std::string> bossRushOrder(const content::ContentDatabase& content);
// The boss team for rush index i (empty bossId when i is past the end -> done).
// Solo bosses (no minions) so the no-heal gauntlet stays a clearable endurance
// test.
dungeon::EnemyTeam bossRushTeam(const content::ContentDatabase& content, int index);
// The escalating enemy team for endless wave W (deterministic from kEndlessSeed).
dungeon::EnemyTeam endlessWaveTeam(const content::ContentDatabase& content, int wave);
// The King's solo boss team at kKingScalePct.
dungeon::EnemyTeam kingTeam(const content::ContentDatabase& content);

}  // namespace cd
