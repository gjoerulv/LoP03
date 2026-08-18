#pragma once

#include <functional>
#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"

namespace cd {

struct AppContext;

// M103: one small modal for the new events' picks — a member for the Level
// Altar or the stranger's gift, a piece of gear for the Sacrifice, a side of
// the Token Changer's trade. A titled row list over the dimmed dungeon:
// Confirm reports the picked row through onPick (which owns ALL effects and
// the event's resolved flag); Cancel steps away with nothing spent and the
// event unresolved. The DungeonState below outlives this modal, so callback
// captures into it stay valid (the ArmoryGhost pointer precedent).
class EventChoiceState : public GameState {
public:
    EventChoiceState(StateStack& stack, AppContext& context, std::string title,
                     std::vector<std::string> rows, std::function<void(int)> onPick);

    void handleInput(const Input& input) override;
    void render() override;
    bool rendersBelow() const override { return true; }

private:
    AppContext& context_;
    std::string title_;
    ui::Menu menu_;
    // Owner fix 2026-08-17: a long list (the Sacrifice over a deep bag) once
    // grew the box past the screen — rows now show through a fixed window
    // that follows the cursor (drawMenuScrolled's arrows mark the rest).
    ui::ScrollWindow scroll_;
    std::function<void(int)> onPick_;
};

}  // namespace cd
