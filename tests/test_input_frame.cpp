#include <catch2/catch_test_macros.hpp>

#include <set>

#include "input/Input.hpp"

// The M13 Input facade frame model: unified down states, own edges, left-stick
// hysteresis, hold-to-repeat navigation, suppression across transitions, and
// active-device tracking — all against a fake InputQuery.

using cd::ActiveDevice;
using cd::Input;
using cd::InputAction;
using cd::InputQuery;

namespace {

struct Fake {
    std::set<int> keysDown;
    std::set<int> buttonsDown;
    float leftX = 0.0f;
    float leftY = 0.0f;
    bool padAvailable = true;

    InputQuery query() {
        InputQuery q;
        q.keyDown = [this](int key) { return keysDown.count(key) > 0; };
        q.keyPressed = [this](int key) { return keysDown.count(key) > 0; };
        q.keyReleased = [](int) { return false; };
        q.buttonDown = [this](int, int b) { return buttonsDown.count(b) > 0; };
        q.buttonPressed = [this](int, int b) { return buttonsDown.count(b) > 0; };
        q.buttonReleased = [](int, int) { return false; };
        q.gamepadAvailable = [this](int) { return padAvailable; };
        q.axisValue = [this](int, int axis) { return axis == 0 ? leftX : leftY; };
        return q;
    }
};

constexpr int kKeyEnter = 257;
constexpr int kKeyUp = 265;
constexpr int kButtonA = 7;

}  // namespace

TEST_CASE("input frame: press edge fires once, down persists, release fires") {
    Fake fake;
    Input input;
    input.setQuery(fake.query());

    fake.keysDown.insert(kKeyEnter);
    input.update(0.016f);
    CHECK(input.pressed(InputAction::Confirm));
    CHECK(input.down(InputAction::Confirm));

    input.update(0.016f);
    CHECK_FALSE(input.pressed(InputAction::Confirm));
    CHECK(input.down(InputAction::Confirm));

    fake.keysDown.clear();
    input.update(0.016f);
    CHECK(input.released(InputAction::Confirm));
    CHECK_FALSE(input.down(InputAction::Confirm));
}

TEST_CASE("input frame: left stick engages with hysteresis") {
    Fake fake;
    Input input;
    input.setQuery(fake.query());

    fake.leftY = -0.4f;  // below the 0.5 enter threshold
    input.update(0.016f);
    CHECK_FALSE(input.down(InputAction::MoveUp));

    fake.leftY = -0.6f;  // engage
    input.update(0.016f);
    CHECK(input.down(InputAction::MoveUp));
    CHECK(input.pressed(InputAction::MoveUp));

    fake.leftY = -0.4f;  // between release (0.35) and enter: stays engaged
    input.update(0.016f);
    CHECK(input.down(InputAction::MoveUp));

    fake.leftY = -0.2f;  // below release: disengages
    input.update(0.016f);
    CHECK_FALSE(input.down(InputAction::MoveUp));
}

TEST_CASE("input frame: stick does nothing without a gamepad") {
    Fake fake;
    fake.padAvailable = false;
    fake.leftY = -1.0f;
    Input input;
    input.setQuery(fake.query());
    input.update(0.016f);
    CHECK_FALSE(input.down(InputAction::MoveUp));
}

TEST_CASE("input frame: navPressed repeats after the initial delay") {
    Fake fake;
    Input input;
    input.setQuery(fake.query());

    fake.keysDown.insert(kKeyUp);
    input.update(0.016f);
    CHECK(input.navPressed(InputAction::MoveUp));  // initial press

    // Held below the 0.35s delay: no repeat.
    input.update(0.2f);
    CHECK_FALSE(input.navPressed(InputAction::MoveUp));

    // Crossing the delay fires a repeat.
    input.update(0.2f);
    CHECK(input.navPressed(InputAction::MoveUp));

    // Next repeat needs the 0.09s interval.
    input.update(0.03f);
    CHECK_FALSE(input.navPressed(InputAction::MoveUp));
    input.update(0.09f);
    CHECK(input.navPressed(InputAction::MoveUp));

    // Releasing resets everything.
    fake.keysDown.clear();
    input.update(0.016f);
    fake.keysDown.insert(kKeyUp);
    input.update(0.016f);
    CHECK(input.navPressed(InputAction::MoveUp));  // fresh press again
}

TEST_CASE("input frame: pressed does not repeat, only navPressed does") {
    Fake fake;
    Input input;
    input.setQuery(fake.query());
    fake.keysDown.insert(kKeyEnter);
    input.update(0.016f);
    CHECK(input.pressed(InputAction::Confirm));
    input.update(1.0f);
    CHECK_FALSE(input.pressed(InputAction::Confirm));
    // Confirm technically repeats in navPressed, but states never use
    // navPressed for Confirm (control standard).
}

TEST_CASE("input frame: suppression swallows held input until release") {
    Fake fake;
    Input input;
    input.setQuery(fake.query());

    fake.keysDown.insert(kKeyEnter);
    input.update(0.016f);
    CHECK(input.pressed(InputAction::Confirm));

    input.suppressUntilRelease();  // as if a new state appeared
    input.update(0.016f);
    CHECK_FALSE(input.pressed(InputAction::Confirm));
    input.update(1.0f);
    CHECK_FALSE(input.navPressed(InputAction::Confirm));

    fake.keysDown.clear();
    input.update(0.016f);
    fake.keysDown.insert(kKeyEnter);
    input.update(0.016f);
    CHECK(input.pressed(InputAction::Confirm));  // fresh press works again
}

TEST_CASE("input frame: active device follows the last intentional input") {
    Fake fake;
    Input input;
    input.setQuery(fake.query());
    CHECK(input.activeDevice() == ActiveDevice::Keyboard);  // default

    fake.buttonsDown.insert(kButtonA);
    input.update(0.016f);
    CHECK(input.activeDevice() == ActiveDevice::Gamepad);

    fake.buttonsDown.clear();
    input.update(0.016f);
    fake.keysDown.insert(kKeyEnter);
    input.update(0.016f);
    CHECK(input.activeDevice() == ActiveDevice::Keyboard);

    // Stick engagement counts as gamepad intent.
    fake.keysDown.clear();
    input.update(0.016f);
    fake.leftY = -0.9f;
    input.update(0.016f);
    CHECK(input.activeDevice() == ActiveDevice::Gamepad);
}

TEST_CASE("input frame: controller disconnect clears pad-held actions safely") {
    Fake fake;
    Input input;
    input.setQuery(fake.query());
    fake.buttonsDown.insert(kButtonA);
    input.update(0.016f);
    CHECK(input.down(InputAction::Confirm));

    fake.padAvailable = false;  // unplugged while held
    input.update(0.016f);
    CHECK_FALSE(input.down(InputAction::Confirm));
    CHECK(input.released(InputAction::Confirm));
}
