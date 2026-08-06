#include "input/PromptLabels.hpp"

namespace cd::input {

namespace {

// raylib key codes (kept as ints so this stays raylib-free and testable).
constexpr int kSpace = 32;
constexpr int kEscape = 256;
constexpr int kEnter = 257;
constexpr int kTab = 258;
constexpr int kBackspace = 259;
constexpr int kRight = 262;
constexpr int kLeft = 263;
constexpr int kDown = 264;
constexpr int kUp = 265;
constexpr int kF1 = 290;
// M79: the party-cycling alternates are modifier keys; without names they
// would render as "Key#341" in every prompt.
constexpr int kLeftShift = 340;
constexpr int kLeftControl = 341;
constexpr int kLeftAlt = 342;
constexpr int kRightShift = 344;
constexpr int kRightControl = 345;
constexpr int kRightAlt = 346;

}  // namespace

std::string keyName(int key) {
    switch (key) {
        case kSpace: return "Space";
        case kEscape: return "Esc";
        case kEnter: return "Enter";
        case kTab: return "Tab";
        case kBackspace: return "Backspace";
        case kRight: return "Right";
        case kLeft: return "Left";
        case kDown: return "Down";
        case kUp: return "Up";
        case kLeftShift: return "Shift";
        case kLeftControl: return "Ctrl";
        case kLeftAlt: return "Alt";
        case kRightShift: return "RShift";
        case kRightControl: return "RCtrl";
        case kRightAlt: return "RAlt";
        // M79 owner feedback: the navigation cluster and the numpad used to
        // fall through to "Key#328"-style labels; every key a player can
        // plausibly bind deserves a real name.
        case 260: return "Insert";
        case 261: return "Delete";
        case 266: return "PgUp";
        case 267: return "PgDn";
        case 268: return "Home";
        case 269: return "End";
        case 280: return "CapsLock";
        case 281: return "ScrLock";
        case 282: return "NumLock";
        case 283: return "PrtScr";
        case 284: return "Pause";
        case 330: return "Num .";
        case 331: return "Num /";
        case 332: return "Num *";
        case 333: return "Num -";
        case 334: return "Num +";
        case 335: return "Num Enter";
        case 336: return "Num =";
        case 343: return "Super";
        case 347: return "RSuper";
        case 348: return "MenuKey";
        default: break;
    }
    if (key >= kF1 && key <= kF1 + 11) {
        return "F" + std::to_string(key - kF1 + 1);
    }
    if (key >= 320 && key <= 329) {  // the numpad digits
        return "Num " + std::to_string(key - 320);
    }
    // Printable ASCII range (raylib key codes match uppercase ASCII).
    if (key > 32 && key < 127) {
        return std::string(1, static_cast<char>(key));
    }
    return "Key#" + std::to_string(key);
}

std::string buttonName(int button) {
    switch (button) {
        case 1: return "D-Pad Up";
        case 2: return "D-Pad Right";
        case 3: return "D-Pad Down";
        case 4: return "D-Pad Left";
        case 5: return "Y";
        case 6: return "B";
        case 7: return "A";
        case 8: return "X";
        case 9: return "LB";
        case 10: return "LT";
        case 11: return "RB";
        case 12: return "RT";
        case 13: return "Select";
        case 14: return "Guide";
        case 15: return "Start";
        case 16: return "L3";
        case 17: return "R3";
        default: return "Btn#" + std::to_string(button);
    }
}

std::string primaryLabel(const InputMap& map, InputAction action, ActiveDevice device) {
    if (device == ActiveDevice::Keyboard) {
        const auto& keys = map.keys(action);
        return keys.empty() ? "-" : keyName(keys.front());
    }
    const auto& buttons = map.buttons(action);
    if (buttons.empty()) {
        // The stick handles movement even with no button bound.
        switch (action) {
            case InputAction::MoveUp: return "Stick Up";
            case InputAction::MoveDown: return "Stick Down";
            case InputAction::MoveLeft: return "Stick Left";
            case InputAction::MoveRight: return "Stick Right";
            default: return "-";
        }
    }
    return buttonName(buttons.front());
}

std::string allLabels(const InputMap& map, InputAction action, ActiveDevice device) {
    std::string out;
    if (device == ActiveDevice::Keyboard) {
        for (int key : map.keys(action)) {
            out += (out.empty() ? "" : " or ") + keyName(key);
        }
    } else {
        for (int btn : map.buttons(action)) {
            out += (out.empty() ? "" : " or ") + buttonName(btn);
        }
        switch (action) {
            case InputAction::MoveUp:
            case InputAction::MoveDown:
            case InputAction::MoveLeft:
            case InputAction::MoveRight:
                out += (out.empty() ? "" : " or ") + std::string("Stick");
                break;
            default:
                break;
        }
    }
    return out.empty() ? "-" : out;
}

std::string prompt(const InputMap& map, InputAction action, ActiveDevice device,
                   const std::string& verb) {
    return "[" + primaryLabel(map, action, device) + "] " + verb;
}

}  // namespace cd::input
