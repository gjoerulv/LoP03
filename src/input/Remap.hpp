#pragma once

#include "input/InputMap.hpp"

// Pure remapping with conflict recovery (M13). Invariants enforced here so the
// UI can never strand the player:
//  - only actions in kRemappableActions can be rebound or donated from;
//  - no remappable action ever ends up with zero bindings on a device it had
//    bindings on (conflicts swap; a swap without a donor binding is Blocked);
//  - Esc is reserved (cancels the listen flow) and can never be bound.

namespace cd::input {

inline constexpr int kReservedKeyEscape = 256;  // raylib KEY_ESCAPE

enum class RemapOutcome {
    Rebound,   // action now uses the new input; no conflict
    Swapped,   // the input's previous owner took this action's old binding
    Blocked,   // rejected; the map is unchanged
};

struct RemapResult {
    RemapOutcome outcome = RemapOutcome::Blocked;
    // Valid when outcome == Swapped: the action that gave up the input.
    InputAction swappedWith = InputAction::Confirm;
};

bool isRemappable(InputAction action);

// Rebinds `action` to exactly {newKey} on the keyboard. On conflict with
// another remappable action, that owner receives this action's old primary
// key (swap); if there is nothing to donate, the remap is Blocked.
RemapResult remapKey(InputMap& map, InputAction action, int newKey);

// Same for a gamepad button.
RemapResult remapButton(InputMap& map, InputAction action, int newButton);

// Restores all default bindings (both devices).
void resetBindings(InputMap& map);

}  // namespace cd::input
