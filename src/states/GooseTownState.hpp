#pragma once

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// The Goose Town (M61): the second place above the ladder, opened by felling
// the King with at least one Goose in the party. Works like the castle hub but
// hosts exactly one challenge — the Deadly Duck gauntlet — plus the Goofy
// Jester's legendary duck tale, an inn, and a save point. Its record (best
// gauntlet turns) lives on the castle records, never the dungeon scoreboard.
class GooseTownState : public GameState {
public:
    GooseTownState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void onResume() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    void applyMusic();

    AppContext& context_;
    ui::Menu menu_;
};

}  // namespace cd
