#pragma once

#include "states/GameState.hpp"

// M114 — the font specimen (capture-only). One screen that puts the
// readability redesign in front of the overflow lint and the owner's eye:
// the confusable groups (I l 1, O 0, C G, c e, r n m, u v), paired quotes
// and punctuation, the accented sets and the M87 Latin pangram at the three
// exact sizes (9 / 10 / 20). It is a real GameState so the capture runner
// can render it; nothing in the game pushes it. The whole body is guarded by
// CRYSTAL_CAPTURE (the DebugMenuState/CRYSTAL_DEBUG_OVERLAY precedent).

namespace cd {

class StateStack;
struct AppContext;
class Input;

class FontSpecimenState : public GameState {
public:
    FontSpecimenState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
};

}  // namespace cd
