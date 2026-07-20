#pragma once

#include <string>

#include "input/InputMap.hpp"

// Human-readable labels for the ACTIVE bindings (M13). All prompts and the
// Help screen derive their key/button names from the InputMap through these —
// hard-coded prompt strings are prohibited (audit CTRL-007). Pure; raylib key
// codes are treated as plain ints.

namespace cd::input {

// "Enter", "Esc", "Space", "W", "F1", "Key#301"...
std::string keyName(int key);
// "A", "B", "X", "Y", "Start", "Select", "D-Pad Up", "LB", "Btn#5"...
std::string buttonName(int button);

// First binding's label for the device ("Enter" / "A"); "-" when unbound.
std::string primaryLabel(const InputMap& map, InputAction action, ActiveDevice device);

// All bindings joined with " or " ("Enter or Space"); "-" when unbound.
std::string allLabels(const InputMap& map, InputAction action, ActiveDevice device);

// "[Enter] Confirm"-style prompt fragment for footers: label in brackets,
// then the verb.
std::string prompt(const InputMap& map, InputAction action, ActiveDevice device,
                   const std::string& verb);

}  // namespace cd::input
