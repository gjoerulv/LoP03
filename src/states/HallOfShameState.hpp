#pragma once

#include <string>

#include "states/GameState.hpp"
#include "ui/ScrollWindow.hpp"

// M124 - the Hall of Shame: the title screen's list of Iron Man runs that
// ended in a wipe (game/FallenRuns.hpp), newest first. A row names the party
// (its leader and highest level), its play time, and where it fell and to
// whom; Confirm parses the row's party snapshot back through the slot codec
// and opens the same six-page summary the send-off showed
// (EndgameSummaryState's fallen form). Read-only: nothing here can be
// continued, deleted or changed.

namespace cd {

struct AppContext;

class HallOfShameState : public GameState {
public:
    HallOfShameState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    void captureSelect(int row);
#endif

private:
    void open();
    int count() const;

    AppContext& context_;
    int cursor_ = 0;
    ui::ScrollWindow scroll_;
    std::string message_;
};

}  // namespace cd
