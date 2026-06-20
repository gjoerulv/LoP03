#pragma once

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// Transparent in-dungeon pause overlay: Resume, or Retreat to Town (which leaves
// the dungeon — 0 dungeon score, but that scoring lands in Milestone 6).
class DungeonMenuState : public GameState {
public:
    DungeonMenuState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

    bool rendersBelow() const override { return true; }
    bool updatesBelow() const override { return false; }

private:
    AppContext& context_;
    ui::Menu menu_;
};

}  // namespace cd
