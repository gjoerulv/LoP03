#pragma once

#include <string>
#include <vector>

#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// A generic "this opens later" location screen (shops, guild, training hall,
// scoreboard). Shows a title and a few lines, returns on Cancel/Confirm.
class PlaceholderLocationState : public GameState {
public:
    PlaceholderLocationState(StateStack& stack, AppContext& context, std::string title,
                             std::vector<std::string> lines);

    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
    std::string title_;
    std::vector<std::string> lines_;
};

}  // namespace cd
