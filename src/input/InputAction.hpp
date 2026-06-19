#pragma once

#include <cstddef>

// Gameplay-facing input vocabulary. Game code reacts to these ACTIONS, never to
// raw key codes or gamepad buttons. The mapping from hardware to actions lives
// in InputMap.

namespace cd {

enum class InputAction {
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Confirm,
    Cancel,
    Menu,
    Pause,
    ToggleDebug,
    Quit,
};

inline constexpr std::size_t kInputActionCount = 10;

inline constexpr std::size_t actionIndex(InputAction a) { return static_cast<std::size_t>(a); }

}  // namespace cd
