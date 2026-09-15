#pragma once

#include <vector>

#include "input/InputAction.hpp"
#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// The actions the Controls page lists (their labels come from the live
// bindings at render time). M117 (owner-directed 2026-09-14): ToggleDebug
// appears only where the debug overlay is compiled in — never in Release,
// where F1 does nothing — so a test can pin the list in both presets.
const std::vector<InputAction>& helpShownActions();

// A controls / help screen listing the keyboard and gamepad mappings.
class HelpState : public GameState {
public:
    HelpState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    bool pausesPlayClock() const override { return true; }  // M109: a menu, not play
    void render() override;

private:
    AppContext& context_;
};

}  // namespace cd
