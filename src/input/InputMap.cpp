#include "input/InputMap.hpp"

#include "raylib.h"  // for default KEY_*/GAMEPAD_BUTTON_* constants (compile-time only)

namespace cd {

InputMap::InputMap() {
    // Keyboard defaults: arrows + WASD, plus common confirm/cancel/menu keys.
    bindKey(InputAction::MoveUp, KEY_UP);
    bindKey(InputAction::MoveUp, KEY_W);
    bindKey(InputAction::MoveDown, KEY_DOWN);
    bindKey(InputAction::MoveDown, KEY_S);
    bindKey(InputAction::MoveLeft, KEY_LEFT);
    bindKey(InputAction::MoveLeft, KEY_A);
    bindKey(InputAction::MoveRight, KEY_RIGHT);
    bindKey(InputAction::MoveRight, KEY_D);

    bindKey(InputAction::Confirm, KEY_ENTER);
    bindKey(InputAction::Confirm, KEY_SPACE);
    bindKey(InputAction::Cancel, KEY_BACKSPACE);
    bindKey(InputAction::Cancel, KEY_ESCAPE);
    bindKey(InputAction::Menu, KEY_TAB);
    bindKey(InputAction::TextBackspace, KEY_BACKSPACE);  // text editing only
    bindKey(InputAction::ToggleDebug, KEY_F1);
    // Quit has no default key: Esc/Backspace map to Cancel, whose meaning is
    // contextual per state (resume an overlay, or leave the title). Quit stays
    // reserved for an explicit, unambiguous quit affordance later.
    // (The former Pause action was removed in M13: it was bound but consumed
    // nowhere — audit CTRL-021.)

    // Gamepad defaults (Xbox-style layout via raylib's generic mapping).
    // The left stick maps to Move* via axes in the Input facade, not here.
    bindButton(InputAction::MoveUp, GAMEPAD_BUTTON_LEFT_FACE_UP);
    bindButton(InputAction::MoveDown, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    bindButton(InputAction::MoveLeft, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    bindButton(InputAction::MoveRight, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    bindButton(InputAction::Confirm, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);       // A
    bindButton(InputAction::Cancel, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);       // B
    bindButton(InputAction::Menu, GAMEPAD_BUTTON_MIDDLE_RIGHT);             // Start
    bindButton(InputAction::TextBackspace, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);  // X
}

void InputMap::clear(InputAction action) {
    keys_[actionIndex(action)].clear();
    buttons_[actionIndex(action)].clear();
}

void InputMap::bindKey(InputAction action, int key) {
    keys_[actionIndex(action)].push_back(key);
}

void InputMap::bindButton(InputAction action, int button) {
    buttons_[actionIndex(action)].push_back(button);
}

const std::vector<int>& InputMap::keys(InputAction action) const {
    return keys_[actionIndex(action)];
}

const std::vector<int>& InputMap::buttons(InputAction action) const {
    return buttons_[actionIndex(action)];
}

bool InputMap::isDown(InputAction action, const InputQuery& q) const {
    for (int key : keys_[actionIndex(action)]) {
        if (q.keyDown && q.keyDown(key)) {
            return true;
        }
    }
    const bool padOk = q.gamepadAvailable && q.gamepadAvailable(q.gamepad);
    if (padOk && q.buttonDown) {
        for (int btn : buttons_[actionIndex(action)]) {
            if (q.buttonDown(q.gamepad, btn)) {
                return true;
            }
        }
    }
    return false;
}

bool InputMap::isPressed(InputAction action, const InputQuery& q) const {
    for (int key : keys_[actionIndex(action)]) {
        if (q.keyPressed && q.keyPressed(key)) {
            return true;
        }
    }
    const bool padOk = q.gamepadAvailable && q.gamepadAvailable(q.gamepad);
    if (padOk && q.buttonPressed) {
        for (int btn : buttons_[actionIndex(action)]) {
            if (q.buttonPressed(q.gamepad, btn)) {
                return true;
            }
        }
    }
    return false;
}

bool InputMap::isReleased(InputAction action, const InputQuery& q) const {
    for (int key : keys_[actionIndex(action)]) {
        if (q.keyReleased && q.keyReleased(key)) {
            return true;
        }
    }
    const bool padOk = q.gamepadAvailable && q.gamepadAvailable(q.gamepad);
    if (padOk && q.buttonReleased) {
        for (int btn : buttons_[actionIndex(action)]) {
            if (q.buttonReleased(q.gamepad, btn)) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace cd
