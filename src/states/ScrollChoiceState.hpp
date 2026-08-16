#pragma once

#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// M92: the Guild's trove — the pick-one skill-scroll reward for a scoring,
// stakes-raising 20-floor clear (game/ScrollTrove.hpp decides eligibility and
// the seeded offers). Pushed UNDER the result screen, so it surfaces on the
// way back to town (the M67 unwind order). Choosing adds the scroll ITEM to
// the bag — teaching stays the Party panel's M64 moment; "(Leave it)" declines
// deliberately (Cancel does nothing: a one-shot reward deserves a real answer).
class ScrollChoiceState : public GameState {
public:
    ScrollChoiceState(StateStack& stack, AppContext& context, std::vector<std::string> offers);

    void handleInput(const Input& input) override;
    void render() override;
    bool rendersBelow() const override { return true; }  // modal over the town/result

private:
    AppContext& context_;
    std::vector<std::string> offers_;  // item ids
    ui::Menu menu_;
};

}  // namespace cd
