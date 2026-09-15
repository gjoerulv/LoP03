#pragma once

#include "states/GameState.hpp"

// M118 — the event-marker specimen (capture-only). One screen that puts
// every event kind's 12×12 marker icon beside its flavor title, so the
// overflow lint and the owner's eye see the whole set at the size the room
// draws it. A real GameState the capture runner renders; nothing in the game
// pushes it. The body is guarded by CRYSTAL_CAPTURE (the FontSpecimenState
// precedent).

namespace cd {

class StateStack;
struct AppContext;
class Input;

class EventMarkerSpecimenState : public GameState {
public:
    EventMarkerSpecimenState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

private:
    AppContext& context_;
};

}  // namespace cd
