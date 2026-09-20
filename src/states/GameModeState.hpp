#pragma once

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// M123 - the New Game mode page: Normal or Iron Man.
//
// Sits between the title's "New Game" row and party creation. The panel
// beside the two rows explains the highlighted mode - for Iron Man that is
// the whole rule set (game/IronMan.hpp: no saves, permadeath, the escape
// price), so nobody can start such a run without having had it spelled out.
// Iron Man then asks once more through ConfirmPromptState (cursor on Back).
class GameModeState : public GameState {
public:
    GameModeState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only: highlight Iron Man (the rules panel), optionally with the
    // final confirm raised on top.
    void captureIronMan(bool askBegin);
#endif

private:
    void choose();
    void askBeginIronMan();

    AppContext& context_;
    ui::Menu menu_;
};

}  // namespace cd
