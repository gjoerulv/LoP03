#pragma once

#include "battle/Battle.hpp"
#include "game/Party.hpp"
#include "game/RunStats.hpp"
#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// M94: the Training Hall's sparring mirror — fight an exact echo of the
// current party (battle::buildSparBattle), either with the echoes driven by
// the enemy AI or, in manual mode, commanded by the player too. Zero stakes:
// the WHOLE party object (members, bag, gold, records, bestiary) is
// snapshotted before the fight and restored afterwards regardless of outcome,
// so a spar can never pay, cost, or record anything — the anti-farming rule,
// absolute by construction.
class SparState : public GameState {
public:
    SparState(StateStack& stack, AppContext& context, bool manual);

    void onEnter() override;   // snapshot + push the mirror battle
    void onResume() override;  // the battle ended: restore everything
    void handleInput(const Input& input) override;
    void render() override;
    bool rendersBelow() const override { return true; }  // the closing note is a modal

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M94): show the longest closing line without a battle.
    void captureShowClosing();
#endif

private:
    AppContext& context_;
    bool manual_ = false;
    bool fought_ = false;   // guards against re-pushing on later resumes
    bool restored_ = false;
    Party savedParty_;      // the complete pre-spar state
    battle::BattleResult result_;
    RunStats sparStats_;    // BattleState wants a stats sink; discarded with the rest
    std::string closing_;
};

}  // namespace cd
