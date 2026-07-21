#pragma once

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// The castle hub (M40): a distinct place above the town-7 ceiling, reached by the
// northern road from town 7 once any town-7 dungeon is cleared. Not a ladder town
// (never `currentTown`) — its own state with the King's three challenges, an inn,
// a save point, and the party's castle records. A menu-driven throne hall (the
// Jester spot is reserved here for M41). Leaving returns to town 7.
class CastleState : public GameState {
public:
    CastleState(StateStack& stack, AppContext& context);

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
