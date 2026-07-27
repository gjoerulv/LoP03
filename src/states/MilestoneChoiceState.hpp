#pragma once

#include "states/GameState.hpp"

// M63: the level-milestone choice modal. Pushed whenever a party member has
// REACHED a milestone tier (10/20/30) without a stored choice — at the
// level-up moment (owner decision): after a dungeon battle's result flow, at
// the Training Hall, or on returning to town with an old save. Presents the
// class's two authored options (data/milestones.json); Confirm chooses
// permanently, Cancel postpones (the same modal simply returns at the next
// opportunity — a choice is never lost, never forced mid-thought). One push
// drains EVERY pending choice across the party before popping.

namespace cd {

struct AppContext;
class StateStack;

class MilestoneChoiceState : public GameState {
public:
    MilestoneChoiceState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;
    bool rendersBelow() const override { return true; }

#ifdef CRYSTAL_CAPTURE
    // Deterministic capture: force the modal onto a specific member + tier.
    void captureSelect(int member, int tier);
#endif

private:
    bool findPending();  // -> member_/tier_; false when nothing is pending

    AppContext& context_;
    int member_ = -1;  // party index whose choice is on screen
    int tier_ = 0;     // 10 / 20 / 30
    int cursor_ = 0;   // 0 = option a, 1 = option b
};

// Pushes the modal when any party member has a pending milestone choice.
// Returns true when it pushed. Call sites: town resume, dungeon resume,
// the Training Hall after a level-up, the Elder Root's XP grant.
bool maybePushMilestoneChoice(StateStack& stack, AppContext& context);

}  // namespace cd
