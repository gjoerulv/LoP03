#pragma once

#include "states/GameState.hpp"

// M65: the "Maps" screen (town pause menu) — the HoMM2-style puzzle map.
// Four quadrants of an original hand-sketch fill in as Secret Map Pieces are
// gathered; the completed puzzle stamps the treasure's town and dig spot
// (the reveal itself happens the moment the fourth piece is TAKEN — this
// screen shows the progress and the standing reveal). M66 adds the curio
// collection beside it.

namespace cd {

struct AppContext;
class StateStack;

class MapsState : public GameState {
public:
    MapsState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
};

}  // namespace cd
