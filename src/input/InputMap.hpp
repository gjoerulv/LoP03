#pragma once

#include <array>
#include <functional>
#include <vector>

#include "input/InputAction.hpp"

// Mapping from hardware to actions. Deliberately free of raylib so the
// resolution logic can be unit-tested by injecting a fake InputQuery. Key and
// button codes are plain ints (raylib's KEY_*/GAMEPAD_BUTTON_* are ints).

namespace cd {

// Callbacks the resolver uses to ask "is this raw input active?". Production
// fills these from raylib (see makeRaylibInputQuery); tests fill them with fakes.
struct InputQuery {
    std::function<bool(int key)> keyDown;
    std::function<bool(int key)> keyPressed;
    std::function<bool(int key)> keyReleased;
    std::function<bool(int gamepad, int button)> buttonDown;
    std::function<bool(int gamepad, int button)> buttonPressed;
    std::function<bool(int gamepad, int button)> buttonReleased;
    std::function<bool(int gamepad)> gamepadAvailable;
    int gamepad = 0;
};

class InputMap {
public:
    // Constructs with sensible default keyboard + gamepad bindings.
    InputMap();

    void clear(InputAction action);
    void bindKey(InputAction action, int key);
    void bindButton(InputAction action, int button);

    const std::vector<int>& keys(InputAction action) const;
    const std::vector<int>& buttons(InputAction action) const;

    // True if ANY bound key/button for the action satisfies the query.
    bool isDown(InputAction action, const InputQuery& q) const;
    bool isPressed(InputAction action, const InputQuery& q) const;
    bool isReleased(InputAction action, const InputQuery& q) const;

private:
    std::array<std::vector<int>, kInputActionCount> keys_;
    std::array<std::vector<int>, kInputActionCount> buttons_;
};

// Production InputQuery backed by raylib polling (defined in RaylibInputQuery.cpp).
InputQuery makeRaylibInputQuery(int gamepad = 0);

}  // namespace cd
