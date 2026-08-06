#pragma once

#include "states/GameState.hpp"

// M84: the town-milestone choice modal — the M63 level-milestone pattern at
// town scale. Pushed when a town's Guild Master has fallen without a stored
// perk choice: right after the gauntlet's first victory, and again on town
// resume for as long as the choice is postponed. Presents the town's two
// authored options (game/Guild.hpp's constexpr table); Confirm chooses
// permanently, Cancel postpones (a choice is never lost, never forced). One
// push drains EVERY pending town before popping.

namespace cd {

struct AppContext;
class StateStack;

class GuildPerkChoiceState : public GameState {
public:
    GuildPerkChoiceState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;
    bool rendersBelow() const override { return true; }

#ifdef CRYSTAL_CAPTURE
    // Deterministic capture: force the modal onto a specific town.
    void captureSelect(int town);
#endif

private:
    bool findPending();  // -> town_; false when nothing is pending

    AppContext& context_;
    int town_ = 0;     // whose milestone is on screen (0 = nothing pending)
    int cursor_ = 0;   // 0 = option a, 1 = option b
};

// Pushes the modal when any town's perk choice is pending. Returns true when
// it pushed. Call sites: the gauntlet's first victory, town resume.
bool maybePushGuildPerkChoice(StateStack& stack, AppContext& context);

}  // namespace cd
