#pragma once

#include <cstdint>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// The Guild: pick/reroll a dungeon seed and enter. Entering autosaves the party
// (the dungeon-entry trigger for the M3 autosave), generates the dungeon, and
// hands off to the walkable DungeonState.
class GuildState : public GameState {
public:
    GuildState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    void enterDungeon();

    AppContext& context_;
    ui::Menu menu_;
    std::uint64_t seed_ = 1;
    int depth_ = 1;
};

}  // namespace cd
