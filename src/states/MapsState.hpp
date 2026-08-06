#pragma once

#include "states/GameState.hpp"

// M65: the "Maps" screen (town pause menu) — the HoMM2-style puzzle map.
// Four quadrants of an original hand-sketch fill in as Secret Map Pieces are
// gathered; the completed puzzle stamps the treasure's town and dig spot
// (the reveal itself happens the moment the fourth piece is TAKEN — this
// screen shows the progress and the standing reveal). M66 adds the curio
// collection beside it; M85 makes owned curios INSPECTABLE — a cursor walks
// the grid and Confirm opens a lore panel (data/curio_lore.json, with the
// curio's own description as the defensive fallback).

namespace cd {

struct AppContext;
class StateStack;

class MapsState : public GameState {
public:
    MapsState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M85): force the lore panel open on a curio index.
    void captureInspect(int index);
#endif

private:
    AppContext& context_;
    int cursor_ = 0;       // M85: the curio grid cursor (4 cols x 3 rows)
    bool loreOpen_ = false;  // M85: the inspect panel is showing
};

}  // namespace cd
