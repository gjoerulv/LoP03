#pragma once

#include <vector>

#include "states/GameState.hpp"
#include "ui/ScrollWindow.hpp"

namespace cd {

struct AppContext;

// The town Scoreboard: lists recorded dungeon runs, best first. Up/Down
// scrolls when there are more entries than fit on screen. M82: 1-floor and
// 4-floor runs rank on SEPARATE boards, cycled with CyclePrev/CycleNext.
class ScoreboardState : public GameState {
public:
    ScoreboardState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M82): show the 4-floor board, so its header + empty/filled
    // states are overflow-checked.
    void captureShowFourFloorBoard();
#endif

private:
    void rebuildBoard();  // M82: refilter visible_ for boardFloors_

    AppContext& context_;
    ui::ScrollWindow scroll_;
    int boardFloors_ = 1;        // M82: 1 = classic board, 4 = the descent
    std::vector<int> visible_;   // indices into scoreboard entries, board-filtered
};

}  // namespace cd
