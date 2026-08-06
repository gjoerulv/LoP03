#pragma once

#include <string>

#include "game/BossDrops.hpp"
#include "game/RunStats.hpp"
#include "score/Scoring.hpp"
#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// Shown when a dungeon is cleared: the final score and its breakdown, plus any
// M39 boss drops (legendary tokens / a legendary piece) and — M83 — the map
// economy's one-line story (a piece dropped / banked / the reveal). The
// Details key opens the M42 run stats + personal records. Confirm returns the
// party to town.
class DungeonResultState : public GameState {
public:
    DungeonResultState(StateStack& stack, AppContext& context, score::RunSummary summary,
                       int score, BossDropResult drops = {}, RunStats stats = {},
                       std::string mapLine = "");

    void onEnter() override;  // first-result tutorial beat
    void handleInput(const Input& input) override;
    void render() override;

private:
    std::string runStatsText() const;  // M42: this-run stats + personal records

    AppContext& context_;
    score::RunSummary summary_;
    int score_;
    BossDropResult drops_;  // M39: boss legendary/token drops (empty if none)
    RunStats stats_;        // M42: this-run victory tallies
    std::string mapLine_;   // M83: the 4-floor map-drop story ("" = no roll hit)
};

}  // namespace cd
