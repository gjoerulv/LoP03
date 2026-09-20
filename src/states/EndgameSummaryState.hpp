#pragma once

#include <optional>
#include <string>
#include <vector>

#include "game/IronMan.hpp"
#include "game/Party.hpp"
#include "game/Summary.hpp"
#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"

// M116 — the End-game Summary: six tabbed pages of the save's lifetime
// ledger (game/Summary.hpp builds the rows; this state only lays them out).
// Unlocks once the finale's keepsake choice is recorded, shows itself once
// (TownState::onResume; Party::summaryShown), and is revisitable through
// THE STRANGER "P"'s roadside pick. Pages cycle with CyclePrev/CycleNext
// (the scoreboard's board-cycling idiom), rows scroll with Up/Down, Cancel
// leaves. The first showing plays the victory stinger and then the Result
// loop (AudioManager::setMusicThen); revisits play the Result loop.

namespace cd {

class StateStack;
struct AppContext;
class Input;

class EndgameSummaryState : public GameState {
public:
    EndgameSummaryState(StateStack& stack, AppContext& context, bool firstShow);
    // M124 - the FALLEN form: the same six pages over a party SNAPSHOT (an
    // Iron Man run that wiped - the send-off's own copy, or a Hall of Shame
    // record parsed back through the slot codec) instead of the live party,
    // the Overview leading with where it fell and to whom. `leaveToTitle`:
    // the send-off has nothing beneath it, so leaving (Cancel or Confirm)
    // starts the title over; a Hall of Shame visit simply pops.
    EndgameSummaryState(StateStack& stack, AppContext& context, Party snapshot,
                        ironman::FallenInfo fallen, bool leaveToTitle);

    void onEnter() override;
    void handleInput(const Input& input) override;
    bool pausesPlayClock() const override { return true; }  // M109: a screen, not play
    void render() override;

#ifdef CRYSTAL_CAPTURE
    void captureShowPage(int page);  // park on a page, cursor at the top
#endif

private:
    void rebuildPage();
    void leave();
    const Party& source() const;  // the snapshot when there is one, else the live party

    AppContext& context_;
    bool firstShow_ = false;
    std::optional<Party> snapshot_;               // M124
    std::optional<ironman::FallenInfo> fallen_;   // M124
    bool leaveToTitle_ = false;                   // M124
    int page_ = 0;
    std::vector<SummaryRow> rows_;
    ui::Menu menu_;
    ui::ScrollWindow scroll_;
};

}  // namespace cd
