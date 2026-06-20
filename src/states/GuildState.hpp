#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// The Guild: choose theme, depth, and seed, then enter. Entering autosaves the
// party (the dungeon-entry trigger for the M3 autosave), generates the dungeon,
// and hands off to the walkable DungeonState. Infinite runs via seed + depth.
class GuildState : public GameState {
public:
    GuildState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    void enterDungeon();
    std::string currentThemeName() const;

    AppContext& context_;
    ui::Menu menu_;
    std::vector<std::string> themeIds_;
    int themeIndex_ = 0;
    std::uint64_t seed_ = 1;
    int depth_ = 1;
};

}  // namespace cd
