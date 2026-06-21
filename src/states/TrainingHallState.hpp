#pragma once

#include <string>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// The Training Hall: pay gold to level a party member up by one (stats recompute
// from the class growth, and the HP gained is restored).
class TrainingHallState : public GameState {
public:
    TrainingHallState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    void rebuild();
    int trainingCost(int level) const;

    AppContext& context_;
    ui::Menu menu_;
    std::string message_;
};

}  // namespace cd
