#pragma once

#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/ScrollWindow.hpp"

namespace cd {

struct AppContext;
class BattleLog;

// M52 — the in-battle action log overlay. Transparent (the battle renders
// below, dimmed), a Raised panel via the M46 kit, the recent action results
// wrapped and scrollable with Up/Down (newest at the bottom). Opened AND closed
// by the Menu/Pause action (owner decision); Cancel also closes. It snapshots
// the log's lines at construction and never touches the battle, so opening it
// cannot change how the fight resolves.
class BattleLogState : public GameState {
public:
    BattleLogState(StateStack& stack, AppContext& context, const BattleLog& log);

    void handleInput(const Input& input) override;
    void render() override;

    bool rendersBelow() const override { return true; }

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M52): scroll up from the default bottom by `rows` so both the
    // more-above and more-below carets render for the overflow check.
    void captureScrollUp(int rows);
#endif

private:
    AppContext& context_;
    std::vector<std::string> lines_;  // wrapped, oldest -> newest
    ui::ScrollWindow scroll_;
};

}  // namespace cd
