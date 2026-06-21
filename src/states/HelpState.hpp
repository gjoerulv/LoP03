#pragma once

#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// A controls / help screen listing the keyboard and gamepad mappings.
class HelpState : public GameState {
public:
    HelpState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
};

}  // namespace cd
