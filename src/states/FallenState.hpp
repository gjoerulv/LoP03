#pragma once

#include <string>
#include <vector>

#include "game/IronMan.hpp"
#include "states/GameState.hpp"

// M124 - the Iron Man's send-off: the CelebrationState's unkind twin. The
// party that just wiped lies where it fell while geese and ducks take turns
// landing on it, more waterfowl jog past in the foreground, and the Hollow
// King laughs in the back. One dry line from kFallenPhrases; where the party
// fell and to whom. Presentation only (existing sprites, procedural motion -
// every position is a pure function of the state's own clock): it reads the
// party, writes nothing, and on Confirm hands over to the fallen run's
// summary (EndgameSummaryState's fallen form), which leads to the title.

namespace cd {

struct AppContext;

class FallenState : public GameState {
public:
    FallenState(StateStack& stack, AppContext& context, ironman::FallenInfo info);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void update(float dt) override;
    void render() override;
    bool pausesPlayClock() const override { return true; }  // a screen, not play

#ifdef CRYSTAL_CAPTURE
    // Capture-only: pin the scene's clock so the frame is byte-exact.
    void captureFreeze(float time) {
        time_ = time;
        frozen_ = true;
    }
#endif

private:
    AppContext& context_;
    ironman::FallenInfo info_;
    std::string punchline_;
    float time_ = 0.0f;
    bool frozen_ = false;
};

}  // namespace cd
