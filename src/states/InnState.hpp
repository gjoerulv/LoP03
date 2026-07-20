#pragma once

#include <string>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// The Inn restores the whole party to full HP/MP for a gold cost that scales
// with party level (M30), or for free by spending a rest token earned from a
// dungeon event. A menu offers the options; declining (Cancel) is never a
// soft-lock. No auto-heal.
class InnState : public GameState {
public:
    InnState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    void rebuild();  // recompose the menu from current gold/tokens/rest state

    AppContext& context_;
    ui::Menu menu_;
    std::string message_;
};

}  // namespace cd
