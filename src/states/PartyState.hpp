#pragma once

#include <string>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

// M64: the detailed party panel, reachable from BOTH pause menus. A
// bestiary-style member list + detail: derived stats with the gear share,
// equipment names, passives, level/XP, milestone choices (M63), and every
// known skill (learnset + scroll extras). Also the home of the Use-Scroll
// action: with a teaching scroll in the bag, Confirm opens a scroll picker
// and the shown member learns it permanently (game/Scrolls.hpp rules).

namespace cd {

struct AppContext;
class StateStack;

class PartyState : public GameState {
public:
    PartyState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    void captureSelect(int member) { cursor_ = member; }
#endif

private:
    enum class Phase { Browse, PickScroll };

    void rebuildScrolls();  // -> scrollMenu_ from the bag's teaching scrolls

    AppContext& context_;
    Phase phase_ = Phase::Browse;
    int cursor_ = 0;  // member index
    ui::Menu scrollMenu_;
    std::vector<std::string> scrollIds_;  // parallel to scrollMenu_ rows
    std::string message_;
    bool messageIsError_ = false;  // M67: banner kind for the feedback toast
};

}  // namespace cd
