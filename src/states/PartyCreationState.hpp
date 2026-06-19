#pragma once

#include <array>
#include <vector>

#include "states/GameState.hpp"
#include "ui/TextInput.hpp"

namespace cd {

struct AppContext;
namespace content {
struct ClassDef;
}

// New-game party builder: 4 slots, each a class (cycled with Left/Right) and a
// name. Name editing is modal (Confirm enters edit mode) so movement keys do not
// fight typing. Slots start with sensible defaults so a gamepad-only player can
// proceed without typing.
class PartyCreationState : public GameState {
public:
    PartyCreationState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void update(float dt) override;
    void render() override;

private:
    struct Slot {
        int classIndex = 0;
        ui::TextInput name;
    };

    void cycleClass(int slotIndex, int direction);
    void begin();

    AppContext& context_;
    std::vector<const content::ClassDef*> classes_;
    std::array<Slot, 4> slots_;
    int cursor_ = 0;  // 0..3 = party slots, 4 = "Begin"
    bool editing_ = false;
    bool ready_ = false;
    float caretTimer_ = 0.0f;
};

}  // namespace cd
