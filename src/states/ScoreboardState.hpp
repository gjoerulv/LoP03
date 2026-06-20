#pragma once

#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// The town Scoreboard: lists recorded dungeon runs, best first.
class ScoreboardState : public GameState {
public:
    ScoreboardState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
};

}  // namespace cd
