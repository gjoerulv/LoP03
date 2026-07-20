#pragma once

#include "score/Scoring.hpp"
#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// Shown when a dungeon is cleared: the final score and its breakdown. Confirm
// returns the party to town (pops itself and the dungeon beneath it).
class DungeonResultState : public GameState {
public:
    DungeonResultState(StateStack& stack, AppContext& context, score::RunSummary summary,
                       int score);

    void onEnter() override;  // first-result tutorial beat
    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
    score::RunSummary summary_;
    int score_;
};

}  // namespace cd
