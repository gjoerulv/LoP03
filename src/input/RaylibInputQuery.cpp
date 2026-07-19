#include "input/InputMap.hpp"
#include "raylib.h"

// Production InputQuery: wires the resolver's callbacks to raylib polling.
// Isolated here so InputMap stays raylib-free and unit-testable.

namespace cd {

InputQuery makeRaylibInputQuery(int gamepad) {
    InputQuery q;
    q.gamepad = gamepad;
    q.keyDown = [](int key) { return IsKeyDown(key); };
    q.keyPressed = [](int key) { return IsKeyPressed(key); };
    q.keyReleased = [](int key) { return IsKeyReleased(key); };
    q.buttonDown = [](int pad, int button) { return IsGamepadButtonDown(pad, button); };
    q.buttonPressed = [](int pad, int button) { return IsGamepadButtonPressed(pad, button); };
    q.buttonReleased = [](int pad, int button) { return IsGamepadButtonReleased(pad, button); };
    q.gamepadAvailable = [](int pad) { return IsGamepadAvailable(pad); };
    q.axisValue = [](int pad, int axis) { return GetGamepadAxisMovement(pad, axis); };
    q.nextChar = []() { return GetCharPressed(); };
    q.nextKeyPressed = []() { return GetKeyPressed(); };
    return q;
}

}  // namespace cd
