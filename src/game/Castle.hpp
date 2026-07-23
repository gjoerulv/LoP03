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
// multiplier (buildBattle scales boss/enemy stats by statScalePct/100).
//
// **The castle floor (owner decision, 2026-07-23).** Every castle challenge must
// START above the strongest thing a dungeon can produce. The dungeon ceiling is
// town 7 at the depth cap: `combineTownScale(100 + statScalePct(depth), 7)` =
// 190 x 3.00 = **570 %**. The castle is the place above the ladder, so no castle
// scale may sit under that — previously the Boss Rush (260 %) and the King
// (310 %) both did, which made a deep town-7 dungeon boss a bigger per-fight
// number than anything in the throne room. That is now a pinned invariant
// (`castleFloorScalePct`, asserted in test_castle.cpp against the shipped
// composition + ladder rather than against a literal).
//
// This deliberately supersedes the M44/M49 sim-tuned values. The challenges are
// no longer tuned to "what the simulator can beat" — the owner's call is that
// the castle is meant to be very hard, and the sim's scripted party is a floor
// on player skill, not a ceiling. What a maxed party actually achieves at these
// scales is RECORDED (`[castle-report]`, `[king-report]`) rather than asserted.
// Owner tuning pass (2026-07-23, after reviewing the 600/600/700 sim results):
// Boss Rush 580 %, King 500 %, endless from 500 % at +10 %pts/wave.
//
// Note that the King's and the endless opening scale now sit BELOW the 570 %
// ladder ceiling as raw multipliers. That is deliberate and it does not put
// those fights below the dungeons, because a multiplier is only half the story:
// the King's BASE stats tower over every dungeon boss (750 HP / 44 MAG / 26 SPD
// against the Dread Sovereign's 400 / 30 / 12), so at 500 % he is a 3750 HP
// fight where the deepest possible dungeon boss is 2280. The invariant that
// actually matters is therefore expressed in EFFECTIVE stats, not in percent —
// see `castle: the castle outclasses the deepest dungeon` in test_castle.cpp.
inline constexpr int kCastleBaselineScalePct = 500;  // endless wave-0 / reference

// Boss Rush: twelve bosses, each with the minions it brings in a dungeon, with
// no free healing between fights — and each of them scaled above the deepest
// dungeon boss as a raw multiplier too. Attrition on top of a scale that already
// exceeds the ladder.
inline constexpr int kBossRushScalePct = 580;

// The King: the hardest single fight in the game, and the hardest thing in the
// castle. His base stats already sit above every dungeon boss (750 HP / 44 MAG /
// 26 SPD against the Dread Sovereign's 400 / 30 / 12), he carries three passives
// and immunity to all three control statuses, his debuff-curses deal damage every
// turn (M40), and since M49 two Royal Guards brace him and return every five of
// his turns.
//
// History: M44 set 420 -> 340 and M49 340 -> 310, both tuned to the point where
// the simulator's scripted party could just win with the approved counterplay.
// The owner superseded that approach on 2026-07-23 (700 %), then tuned it back to
// **500 %** after seeing that nothing in the game could beat him above the floor.
// At 500 % he is a 3750 HP fight with 220 MAG and 130 SPD, plus two guards he
// revives — comfortably the largest single fight in the game even though the raw
// percentage is under the ladder's 570 % ceiling.
//
// Note for future tuning: `buildBattle` seeds the targeting tie-break from the
// combatants' NAMES, so renaming a unit in this fight shifts the sim's outcome
// at the margin. Re-run the sweep after any rename, not before.
inline constexpr int kKingScalePct = 500;

// The dungeon ceiling every castle scale must exceed: the town-ladder multiplier
// at town `kTownCount` applied to the depth-capped composition scaling. Pure and
// derived from the same rules the generator uses, so the invariant cannot drift
// if the ladder or the composition data changes. Defined in Castle.cpp to keep
// this header free of the generator's includes.
int castleFloorScalePct(const content::ContentDatabase& content);

// Endless wave W (0-based): starts at the baseline and escalates 10 %pts/wave,
// so the opening waves are already endgame and the climb passes the ladder's
// 570 % ceiling within a handful of waves and never stops.
inline int endlessWaveScalePct(int wave) {
    const int w = wave < 0 ? 0 : wave;
    return kCastleBaselineScalePct + 10 * w;
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
