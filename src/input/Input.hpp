#pragma once

#include <array>
#include <utility>

#include "input/InputMap.hpp"

// Per-frame input facade handed to states. Holds the bindings and the current
// frame's query; states ask in action terms: input.pressed(InputAction::Confirm).
//
// M13: the facade computes a per-frame action model in update(dt) —
// key/button/axis down states unified, its own press/release edges, left-stick
// hysteresis, hold-to-repeat for menu navigation (navPressed), suppression of
// buffered presses across state transitions, and active-device tracking.
// All logic is pure against the injected InputQuery, so it tests headlessly.

namespace cd {

class Input {
public:
    // Tuning (seconds / axis fractions); fixed constants, documented in
    // docs/control_standard.md.
    static constexpr float kRepeatDelay = 0.35f;
    static constexpr float kRepeatInterval = 0.09f;
    static constexpr float kAxisEnter = 0.5f;
    static constexpr float kAxisRelease = 0.35f;

    void setQuery(InputQuery query) { query_ = std::move(query); }

    // Computes this frame's model. Call once per frame after setQuery.
    void update(float dt);

    bool down(InputAction a) const { return frame_[actionIndex(a)].down; }
    bool pressed(InputAction a) const { return frame_[actionIndex(a)].pressed; }
    bool released(InputAction a) const { return frame_[actionIndex(a)].released; }
    // pressed OR hold-repeat — use for menu/list navigation, never for
    // Confirm/Cancel.
    bool navPressed(InputAction a) const { return frame_[actionIndex(a)].navFired; }

    // Drains one queued text codepoint (0 = none); loop until 0 per frame.
    int nextChar() const;
    // Remap-listening helpers: drain one pressed key code (0 = none) / scan
    // for one pressed gamepad button (-1 = none).
    int takeNextKey() const;
    int takePressedButton() const;

    bool gamepadAvailable() const;
    ActiveDevice activeDevice() const { return device_; }

    // Swallow presses of everything currently held until it is released;
    // called by the application when the state stack changes so a buffered
    // press cannot activate something in the next screen.
    void suppressUntilRelease();

    InputMap& map() { return map_; }
    const InputMap& map() const { return map_; }

private:
    struct ActionFrame {
        bool down = false;
        bool pressed = false;
        bool released = false;
        bool navFired = false;
    };

    bool rawActionDown(InputAction a, bool padOk) const;

    InputMap map_;
    InputQuery query_;
    std::array<ActionFrame, kInputActionCount> frame_{};
    std::array<bool, kInputActionCount> prevDown_{};
    std::array<bool, kInputActionCount> suppressed_{};
    std::array<float, kInputActionCount> heldTime_{};
    std::array<int, kInputActionCount> repeatCount_{};
    // Left-stick hysteresis, indexed like MoveUp..MoveRight.
    std::array<bool, 4> axisActive_{};
    ActiveDevice device_ = ActiveDevice::Keyboard;
};

}  // namespace cd
