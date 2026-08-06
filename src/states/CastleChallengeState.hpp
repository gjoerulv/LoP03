#pragma once

#include <string>

#include "battle/Battle.hpp"
#include "game/Castle.hpp"
#include "game/RunStats.hpp"
#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// Runs a castle challenge (M40) as a sequence of battles with PERSISTENT HP/MP —
// no free healing between fights (items/skills still work in battle). Boss Rush:
// the 12 sorted bosses; cleared = fewest total turns. Endless Rush: deterministic
// escalating waves; record = best wave survived. The King: one bespoke fight. On
// finish it updates the party's castle records, pays the one-time first-clear
// reward (the King also grants the unique regalia + a title), and shows a result
// overlay before returning to the castle. Records live outside the dungeon
// scoreboard entirely.
//
// M84: the town-guild gauntlet (CastleChallenge::GuildBoss + `guildTown`)
// rides this same runner — wave 1 is the seeded Guild Trial, wave 2 the
// town's Master. A first victory records the guild ledger and offers the
// permanent town-milestone pick before the toasts land.
class CastleChallengeState : public GameState {
public:
    CastleChallengeState(StateStack& stack, AppContext& context, CastleChallenge kind,
                         int guildTown = 0);

    void onEnter() override;
    void onResume() override;
    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M40): force the fullest result overlay (a King first-clear with
    // its longest reward text) for the overflow check, without running a battle or
    // mutating the party. Not present in shipping builds.
    void captureKingReward();
    // Capture-only (M84): the guild gauntlet's fullest first-victory overlay —
    // the longest Master name in the title plus the milestone invitation.
    void captureGuildResult();
#endif

private:
    void startNextFight();
    void finish(bool cleared);

    AppContext& context_;
    CastleChallenge kind_;
    int guildTown_ = 0;     // M84: which town's gauntlet (GuildBoss only)
    int wave_ = 0;          // 0-based fight/wave index
    int totalRounds_ = 0;   // summed battle turns (the Boss Rush / King record)
    int wavesWon_ = 0;
    bool done_ = false;     // the result overlay is showing
    std::string resultText_;
    battle::BattleResult result_;
    RunStats stats_;  // M71: per-challenge damage tallies, for the MVP pedestal
};

}  // namespace cd
