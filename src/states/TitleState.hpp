#pragma once

#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// The initial placeholder screen. Demonstrates the foundation: it renders into
// the virtual screen, reacts to mapped input actions, pulls a (placeholder)
// texture from the ResourceManager, and drives state transitions.
class TitleState : public GameState {
public:
    TitleState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void update(float dt) override;
    void render() override;

private:
    AppContext& context_;
    float elapsed_ = 0.0f;
};

}  // namespace cd
