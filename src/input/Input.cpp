#include "input/Input.hpp"

#include <cmath>

namespace cd {

namespace {

// raylib GAMEPAD_AXIS_LEFT_X / GAMEPAD_AXIS_LEFT_Y (kept as plain ints so this
// file stays raylib-free).
constexpr int kAxisLeftX = 0;
constexpr int kAxisLeftY = 1;

bool isMoveAction(InputAction a) {
    return a == InputAction::MoveUp || a == InputAction::MoveDown ||
           a == InputAction::MoveLeft || a == InputAction::MoveRight;
}

}  // namespace

bool Input::rawActionDown(InputAction a, bool padOk) const {
    for (int key : map_.keys(a)) {
        if (query_.keyDown && query_.keyDown(key)) {
            return true;
        }
    }
    if (padOk && query_.buttonDown) {
        for (int btn : map_.buttons(a)) {
            if (query_.buttonDown(query_.gamepad, btn)) {
                return true;
            }
        }
    }
    // Left-stick contribution for the movement actions.
    if (isMoveAction(a) && axisActive_[actionIndex(a)]) {
        return true;
    }
    return false;
}

void Input::update(float dt) {
    const bool padOk = query_.gamepadAvailable && query_.gamepadAvailable(query_.gamepad);

    // 1) Left-stick hysteresis: a direction engages past kAxisEnter and stays
    //    engaged until the axis falls back below kAxisRelease.
    float ax = 0.0f;
    float ay = 0.0f;
    if (padOk && query_.axisValue) {
        ax = query_.axisValue(query_.gamepad, kAxisLeftX);
        ay = query_.axisValue(query_.gamepad, kAxisLeftY);
    }
    auto hysteresis = [](bool active, float value) {
        return active ? value >= kAxisRelease : value >= kAxisEnter;
    };
    axisActive_[actionIndex(InputAction::MoveUp)] =
        hysteresis(axisActive_[actionIndex(InputAction::MoveUp)], -ay);
    axisActive_[actionIndex(InputAction::MoveDown)] =
        hysteresis(axisActive_[actionIndex(InputAction::MoveDown)], ay);
    axisActive_[actionIndex(InputAction::MoveLeft)] =
        hysteresis(axisActive_[actionIndex(InputAction::MoveLeft)], -ax);
    axisActive_[actionIndex(InputAction::MoveRight)] =
        hysteresis(axisActive_[actionIndex(InputAction::MoveRight)], ax);

    // 2) Per-action model: unified down state, own edges, suppression, repeat.
    bool keyboardEdge = false;
    bool gamepadEdge = false;
    for (std::size_t i = 0; i < kInputActionCount; ++i) {
        const InputAction a = static_cast<InputAction>(i);
        const bool downNow = rawActionDown(a, padOk);
        ActionFrame& f = frame_[i];
        f.down = downNow;
        f.pressed = downNow && !prevDown_[i];
        f.released = !downNow && prevDown_[i];

        // Device attribution for fresh presses.
        if (f.pressed) {
            bool fromKey = false;
            for (int key : map_.keys(a)) {
                if (query_.keyDown && query_.keyDown(key)) {
                    fromKey = true;
                    break;
                }
            }
            if (fromKey) {
                keyboardEdge = true;
            } else {
                gamepadEdge = true;
            }
        }

        // Suppression: swallow edges of anything held across a transition.
        if (suppressed_[i]) {
            if (!downNow) {
                suppressed_[i] = false;
            } else {
                f.pressed = false;
            }
        }

        // Hold-to-repeat for navigation.
        f.navFired = f.pressed;
        if (downNow && !suppressed_[i]) {
            if (f.pressed) {
                heldTime_[i] = 0.0f;
                repeatCount_[i] = 0;
            } else {
                heldTime_[i] += dt;
                if (heldTime_[i] >= kRepeatDelay) {
                    const int count =
                        static_cast<int>(std::floor((heldTime_[i] - kRepeatDelay) /
                                                    kRepeatInterval)) + 1;
                    if (count > repeatCount_[i]) {
                        repeatCount_[i] = count;
                        f.navFired = true;
                    }
                }
            }
        } else {
            heldTime_[i] = 0.0f;
            repeatCount_[i] = 0;
        }

        prevDown_[i] = downNow;
    }

    // 3) Active device: the most recent intentional input wins; keyboard wins
    //    a same-frame tie. Stick drift cannot flip it (edges require crossing
    //    the hysteresis enter threshold).
    if (keyboardEdge) {
        device_ = ActiveDevice::Keyboard;
    } else if (gamepadEdge) {
        device_ = ActiveDevice::Gamepad;
    }
}

int Input::nextChar() const { return query_.nextChar ? query_.nextChar() : 0; }

int Input::takeNextKey() const { return query_.nextKeyPressed ? query_.nextKeyPressed() : 0; }

int Input::takePressedButton() const {
    if (!query_.buttonPressed || !query_.gamepadAvailable ||
        !query_.gamepadAvailable(query_.gamepad)) {
        return -1;
    }
    for (int btn = 1; btn <= 17; ++btn) {  // raylib GAMEPAD_BUTTON_* range
        if (query_.buttonPressed(query_.gamepad, btn)) {
            return btn;
        }
    }
    return -1;
}

bool Input::gamepadAvailable() const {
    return query_.gamepadAvailable && query_.gamepadAvailable(query_.gamepad);
}

void Input::suppressUntilRelease() {
    for (std::size_t i = 0; i < kInputActionCount; ++i) {
        if (frame_[i].down) {
            suppressed_[i] = true;
            frame_[i].pressed = false;
            frame_[i].navFired = false;
            heldTime_[i] = 0.0f;
            repeatCount_[i] = 0;
        }
    }
}

}  // namespace cd
