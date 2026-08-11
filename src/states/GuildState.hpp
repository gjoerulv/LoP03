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
    void onResume() override;
    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M84): park the cursor on the Guild Boss row so its status
    // banner (locked/undefeated/defeated) is overflow-checked. The record
    // itself is capture fixture state on the party.
    void captureFocusGuildBoss();
#endif

private:
    void enterDungeon();
    void rebuild();  // composes Theme/Depth values into their menu labels (M25)
    std::string currentThemeName() const;

    AppContext& context_;
    ui::Menu menu_;
    std::vector<std::string> themeIds_;
    int themeIndex_ = 0;
    std::uint64_t seed_ = 1;
    int depth_ = 1;
    int floors_ = 1;  // M82: 1 or 4 — the run's floor count
};

}  // namespace cd
