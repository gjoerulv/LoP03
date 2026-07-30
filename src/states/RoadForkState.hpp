#pragma once

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// M61: the fork on town 7's north road. Until Goose Town opens, the road leads
// straight to the castle and this state is never pushed; afterwards it offers
// the two destinations over the town (rendersBelow), Cancel backing out onto
// the road. Picking one swaps this prompt for the destination hub.
class RoadForkState : public GameState {
public:
    RoadForkState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;
    bool rendersBelow() const override { return true; }

private:
    AppContext& context_;
    ui::Menu menu_;
};

}  // namespace cd
