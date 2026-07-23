#pragma once

#include <array>
#include <string>
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

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M45): put `classId` in a slot so the locked/unlocked states of
    // the reward classes render deterministically. Not present in shipping builds.
    void captureSelectClass(int slot, const std::string& classId);
#endif
    void update(float dt) override;
    void render() override;

private:
    struct Slot {
        int classIndex = 0;
        ui::TextInput name;
    };

    void cycleClass(int slotIndex, int direction);
    // M45: is this class still behind the King? Locked classes stay listed (so the
    // goal is visible) but cannot start a run.
    bool classLocked(const content::ClassDef& cls) const;
    bool anyLockedSelected() const;
    void begin();

    AppContext& context_;
    std::vector<const content::ClassDef*> classes_;
    std::array<Slot, 4> slots_;
#ifdef CRYSTAL_CAPTURE
    // Capture-only: selections requested before the state was pushed, applied at
    // the end of onEnter (which the stack runs after the push and which otherwise
    // resets every slot).
    std::array<std::string, 4> capturePending_;
#endif
    int cursor_ = 0;  // 0..3 = party slots, 4 = "Begin"
    bool editing_ = false;
    bool ready_ = false;
    float caretTimer_ = 0.0f;
};

}  // namespace cd
