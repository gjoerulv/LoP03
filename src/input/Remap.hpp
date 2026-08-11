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

// --- M79: three direct keyboard slots (Primary / Alt 1 / Alt 2) -------------
//
// The keyboard remap screen edits one SLOT at a time instead of replacing the
// whole binding list. Slots pack left (no gaps): a slot index beyond the
// current count lands at the end. Assigning to an occupied slot replaces that
// slot's key. The M13 never-unbound rule stands: a steal that would leave its
// old owner with zero keyboard keys is Blocked. The gamepad flow keeps the
// M13 replace-and-swap engine above.

inline constexpr int kKeySlotCount = 3;

enum class SlotOutcome {
    Rebound,       // the slot holds the key; nobody else was involved
    Stolen,        // the slot holds the key, taken from `owner`/`ownerSlot`
    NeedsConfirm,  // the key belongs to another action; nothing changed —
                   // call again with confirmSteal after the player agrees
    Blocked,       // rejected (reserved Esc, non-remappable, stranding steal)
};

struct SlotResult {
    SlotOutcome outcome = SlotOutcome::Blocked;
    // For Stolen / NeedsConfirm (and the stranding Blocked): where the key
    // lives (or lived).
    InputAction owner = InputAction::Confirm;
    int ownerSlot = 0;
};

// Assigns `key` to `slot` (0..kKeySlotCount-1) of `action`'s keyboard list.
// Moving a key between an action's OWN slots needs no confirmation.
SlotResult assignKeySlot(InputMap& map, InputAction action, int slot, int key,
                         bool confirmSteal);

}  // namespace cd::input
