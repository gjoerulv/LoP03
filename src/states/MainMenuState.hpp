#pragma once

#include <string>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// Entry screen: New Game, Continue (enabled only if a save exists), Quit.
class MainMenuState : public GameState {
public:
    MainMenuState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void onResume() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    void rebuild();

    AppContext& context_;
    ui::Menu menu_;
    std::string phrase_;  // M51: the comedic phrase picked for this title visit
};

}  // namespace cd
