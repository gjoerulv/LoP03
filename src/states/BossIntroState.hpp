#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "audio/AudioRoles.hpp"
#include "battle/Battle.hpp"
#include "game/RunStats.hpp"
#include "render/BattleBackdrop.hpp"
#include "states/BossIntroTimeline.hpp"
#include "states/GameState.hpp"

// M56 — the Crystal Shatter boss intro. Pushed on top of the caller (never
// replaceState, whose queued Pop fires a spurious onResume below) carrying the
// full BattleState launch payload. It freezes the scene (rendersBelow, no
// updatesBelow), plays a short dt-driven timeline, then pushes BattleState on top
// of itself; when the battle pops, its onResume pops it. Presentation-only: it
// reads no battle state during the intro and never touches rollCursor. Always
// skippable with Confirm/Cancel.

namespace cd {

struct AppContext;
class StateStack;
class Input;

class BossIntroState : public GameState {
public:
    BossIntroState(StateStack& stack, AppContext& context, battle::Battle battle,
                   battle::BattleResult* resultSlot, MusicTrack music, RunStats* statsSlot,
                   bool castleChallenge, render::BackdropStage stage, std::uint64_t introSeed);

    void update(float dt) override;
    void handleInput(const Input& input) override;
    void render() override;
    void onResume() override;
    bool rendersBelow() const override { return true; }
    bool updatesBelow() const override { return false; }

#ifdef CRYSTAL_CAPTURE
    // Capture-only: freeze the timeline at an absolute time so a phase renders
    // deterministically for the overflow check.
    void captureSetTime(float t);
#endif

private:
    void launchBattle();

    AppContext& context_;
    battle::Battle battle_;
    battle::BattleResult* resultSlot_;
    MusicTrack music_;
    RunStats* statsSlot_;
    bool castleChallenge_;
    render::BackdropStage stage_;
    std::uint64_t introSeed_;
    BossIntroTimeline timeline_;
    std::vector<IntroShard> shards_;
    bool launched_ = false;   // BattleState pushed; onResume now pops self
    // Capture-only freeze (declared always; set only under CRYSTAL_CAPTURE) so the
    // harness's settle-updates do not advance past the frozen frame.
    bool captureFrozen_ = false;
    std::string bossName_;
    std::string telegraph_;
};

}  // namespace cd
