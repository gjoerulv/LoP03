#pragma once

#include "dungeon/DungeonModel.hpp"
#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// Shows a visible enemy team's details (name, count, members, tags). Battle
// itself arrives in Milestone 5, so this only previews and returns.
class EncounterPreviewState : public GameState {
public:
    EncounterPreviewState(StateStack& stack, AppContext& context, dungeon::EnemyTeam team);

    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
    dungeon::EnemyTeam team_;
};

}  // namespace cd
