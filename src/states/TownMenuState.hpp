#pragma once

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// Transparent overlay opened from town (Menu/Cancel): Resume, Bestiary,
// Achievements, Settings, or Quit to Title (which asks a Yes/No
// ConfirmPromptState). The town renders behind it (rendersBelow).
class TownMenuState : public GameState {
public:
    TownMenuState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

    bool rendersBelow() const override { return true; }
    bool updatesBelow() const override { return false; }

private:
    AppContext& context_;
    ui::Menu menu_;
};

}  // namespace cd
