#pragma once

#include <string>

#include "input/InputAction.hpp"
#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// Per-device binding editor (M13; keyboard side overhauled in M79). On the
// KEYBOARD the cursor addresses one of three direct slots per action
// (Primary / Alt 1 / Alt 2, chosen with Left/Right); Confirm listens for a
// key, and a key that is already in use raises an explicit steal-confirmation
// (the pure logic and the never-unbound invariant live in input/Remap). The
// GAMEPAD keeps the M13 replace-and-swap flow. Esc always cancels listening.
// Every successful change is saved immediately.
class RemapState : public GameState {
public:
    RemapState(StateStack& stack, AppContext& context, ActiveDevice device);

    void onEnter() override;
    void handleInput(const Input& input) override;
    bool pausesPlayClock() const override { return true; }  // M109: a menu, not play
    void update(float dt) override;
    void render() override;

private:
    void rebuild();
    void applyRemap(int code);
    void applySlotAssign(int key, bool confirmSteal);
    void raiseMessage(std::string text, bool isError);

    AppContext& context_;
    ActiveDevice device_;
    ui::Menu menu_;
    bool listening_ = false;
    int slotSel_ = 0;  // M79 (keyboard): which of the three slots is addressed
    // M79: a captured key waiting on the steal confirmation (keyboard only).
    bool confirmPending_ = false;
    int pendingKey_ = 0;
    std::string pendingOwnerLabel_;
    // M79 owner feedback: the banner rides the TOP of the screen and times
    // out, so it can never sit on the Back row; a plain no-conflict rebind
    // shows no banner at all (the slot cell updating is the feedback).
    std::string message_;
    bool messageIsError_ = false;
    float messageTimer_ = 0.0f;
};

}  // namespace cd
