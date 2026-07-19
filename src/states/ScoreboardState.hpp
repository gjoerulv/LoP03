#pragma once

#include "states/GameState.hpp"
#include "ui/ScrollWindow.hpp"

namespace cd {

struct AppContext;

// The town Scoreboard: lists recorded dungeon runs, best first. Up/Down
// scrolls when there are more entries than fit on screen.
class ScoreboardState : public GameState {
public:
    ScoreboardState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
    ui::ScrollWindow scroll_;
};

}  // namespace cd
