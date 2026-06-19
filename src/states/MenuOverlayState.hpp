#pragma once

#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// A transparent overlay pushed on top of the title. Demonstrates the stack's
// transparency: the title still renders behind it, but does not update while the
// overlay is open. Closing it returns to the title.
class MenuOverlayState : public GameState {
public:
    MenuOverlayState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

    bool rendersBelow() const override { return true; }   // see the title behind
    bool updatesBelow() const override { return false; }  // freeze the title

private:
    AppContext& context_;
};

}  // namespace cd
