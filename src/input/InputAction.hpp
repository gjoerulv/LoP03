#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

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
    Details,  // M22: contextual help panels (stats, statuses, danger, score)
    TextBackspace,  // delete-one-char while editing text (never remapped)
    ToggleDebug,
    ReloadAssets,  // debug builds: re-resolve the asset manifest (never remapped)
    Quit,  // reserved for an explicit quit affordance; no default binding
};

inline constexpr std::size_t kInputActionCount = 12;

inline constexpr std::size_t actionIndex(InputAction a) { return static_cast<std::size_t>(a); }

// The device the player most recently used intentionally; drives prompt labels.
enum class ActiveDevice { Keyboard, Gamepad };

// Actions the player may rebind in the settings screen. TextBackspace,
// ToggleDebug, and Quit are fixed.
inline constexpr InputAction kRemappableActions[] = {
    InputAction::MoveUp,  InputAction::MoveDown, InputAction::MoveLeft,
    InputAction::MoveRight, InputAction::Confirm, InputAction::Cancel,
    InputAction::Menu, InputAction::Details,
};
inline constexpr std::size_t kRemappableActionCount = 8;

// Stable identifiers for settings serialization (do not rename: they are a
// public schema once shipped).
constexpr std::string_view actionName(InputAction a) {
    switch (a) {
        case InputAction::MoveUp: return "move_up";
        case InputAction::MoveDown: return "move_down";
        case InputAction::MoveLeft: return "move_left";
        case InputAction::MoveRight: return "move_right";
        case InputAction::Confirm: return "confirm";
        case InputAction::Cancel: return "cancel";
        case InputAction::Menu: return "menu";
        case InputAction::Details: return "details";
        case InputAction::TextBackspace: return "text_backspace";
        case InputAction::ToggleDebug: return "toggle_debug";
        case InputAction::ReloadAssets: return "reload_assets";
        case InputAction::Quit: return "quit";
    }
    return "";
}

constexpr std::optional<InputAction> actionFromName(std::string_view name) {
    for (std::size_t i = 0; i < kInputActionCount; ++i) {
        const InputAction a = static_cast<InputAction>(i);
        if (actionName(a) == name) {
            return a;
        }
    }
    return std::nullopt;
}

// Human-readable names for the remap screen and Help.
constexpr std::string_view actionDisplayName(InputAction a) {
    switch (a) {
        case InputAction::MoveUp: return "Move Up";
        case InputAction::MoveDown: return "Move Down";
        case InputAction::MoveLeft: return "Move Left";
        case InputAction::MoveRight: return "Move Right";
        case InputAction::Confirm: return "Confirm";
        case InputAction::Cancel: return "Cancel / Back";
        case InputAction::Menu: return "Menu / Pause";
        case InputAction::Details: return "Details / Info";
        case InputAction::TextBackspace: return "Delete (text entry)";
        case InputAction::ToggleDebug: return "Toggle debug overlay";
        case InputAction::ReloadAssets: return "Reload assets (debug)";
        case InputAction::Quit: return "Quit";
    }
    return "";
}

}  // namespace cd
