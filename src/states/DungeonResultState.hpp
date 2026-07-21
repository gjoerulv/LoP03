#pragma once

#include "game/BossDrops.hpp"
#include "score/Scoring.hpp"
#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// Shown when a dungeon is cleared: the final score and its breakdown, plus any
// M39 boss drops (legendary tokens / a legendary piece). Confirm returns the
// party to town (pops itself and the dungeon beneath it).
class DungeonResultState : public GameState {
public:
    DungeonResultState(StateStack& stack, AppContext& context, score::RunSummary summary,
                       int score, BossDropResult drops = {});

    void onEnter() override;  // first-result tutorial beat
    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
    score::RunSummary summary_;
    int score_;
    BossDropResult drops_;  // M39: boss legendary/token drops (empty if none)
};

}  // namespace cd
