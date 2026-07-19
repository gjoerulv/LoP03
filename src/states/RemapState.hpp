#pragma once

#include <string>

#include "input/InputAction.hpp"
#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// Per-device binding editor (M13). Confirm on an action starts a
// listen-for-input flow; conflicts swap with the previous owner (the pure
// logic and its invariants live in input/Remap). Esc always cancels
// listening. Every successful change is saved immediately.
class RemapState : public GameState {
public:
    RemapState(StateStack& stack, AppContext& context, ActiveDevice device);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    void rebuild();
    void applyRemap(int code);

    AppContext& context_;
    ActiveDevice device_;
    ui::Menu menu_;
    bool listening_ = false;
    std::string message_;
};

}  // namespace cd
