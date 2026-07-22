#pragma once

#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// The achievements screen (M42): the full fixed roster in two columns, each shown
// locked or unlocked (from the global store), with the cursor's description
// centered below and an unlocked count. Presentation only.
class AchievementsState : public GameState {
public:
    AchievementsState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
    int cursor_ = 0;
};

}  // namespace cd
