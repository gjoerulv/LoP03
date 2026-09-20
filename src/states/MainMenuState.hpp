#pragma once

#include <string>
#include <vector>

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
    bool pausesPlayClock() const override { return true; }  // M109: a menu, not play
    void render() override;

private:
    void rebuild();

    AppContext& context_;
    ui::Menu menu_;
    // M124: which action each visible row is - the Hall of Shame row exists
    // only once somebody has fallen, so rows are no longer fixed indices.
    std::vector<int> rowIds_;
    std::string phrase_;  // M51: the comedic phrase picked for this title visit
};

}  // namespace cd
