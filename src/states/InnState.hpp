#pragma once

#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// The Inn restores the whole party to full HP/MP on entry (free for now; a gold
// cost can come with the economy pass). Shows the rested party, returns on back.
class InnState : public GameState {
public:
    InnState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
};

}  // namespace cd
