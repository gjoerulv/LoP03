#include <catch2/catch_test_macros.hpp>

#include "input/InputMap.hpp"

using namespace cd;

namespace {

// Builds a query whose only "down/pressed/released" key is `activeKey`, with an
// optional active gamepad button. Unset callbacks default to "nothing active".
InputQuery fakeQuery(int activeKey, bool padAvailable = false, int activeButton = -1,
                     int gamepad = 0) {
    InputQuery q;
    q.gamepad = gamepad;
    q.keyDown = [activeKey](int k) { return k == activeKey; };
    q.keyPressed = [activeKey](int k) { return k == activeKey; };
    q.keyReleased = [activeKey](int k) { return k == activeKey; };
    q.gamepadAvailable = [padAvailable](int) { return padAvailable; };
    q.buttonDown = [activeButton](int, int b) { return b == activeButton; };
    q.buttonPressed = [activeButton](int, int b) { return b == activeButton; };
    q.buttonReleased = [activeButton](int, int b) { return b == activeButton; };
    return q;
}

}  // namespace

TEST_CASE("input: resolves a bound key for down/pressed/released", "[input]") {
    InputMap map;
    map.clear(InputAction::Confirm);
    map.bindKey(InputAction::Confirm, 4242);

    const InputQuery active = fakeQuery(4242);
    REQUIRE(map.isDown(InputAction::Confirm, active));
    REQUIRE(map.isPressed(InputAction::Confirm, active));
    REQUIRE(map.isReleased(InputAction::Confirm, active));

    const InputQuery other = fakeQuery(9999);
    REQUIRE_FALSE(map.isDown(InputAction::Confirm, other));
}

TEST_CASE("input: any of several bound keys activates the action", "[input]") {
    InputMap map;
    map.clear(InputAction::MoveUp);
    map.bindKey(InputAction::MoveUp, 100);
    map.bindKey(InputAction::MoveUp, 200);

    REQUIRE(map.isDown(InputAction::MoveUp, fakeQuery(200)));
    REQUIRE(map.isDown(InputAction::MoveUp, fakeQuery(100)));
    REQUIRE_FALSE(map.isDown(InputAction::MoveUp, fakeQuery(300)));
}

TEST_CASE("input: gamepad buttons count only when a pad is available", "[input]") {
    InputMap map;
    map.clear(InputAction::Confirm);
    map.bindButton(InputAction::Confirm, 7);

    // Button pressed but no pad connected -> inactive.
    REQUIRE_FALSE(map.isDown(InputAction::Confirm, fakeQuery(-1, /*pad=*/false, /*button=*/7)));
    // Button pressed and pad connected -> active.
    REQUIRE(map.isDown(InputAction::Confirm, fakeQuery(-1, /*pad=*/true, /*button=*/7)));
    // Pad connected but a different button -> inactive.
    REQUIRE_FALSE(map.isDown(InputAction::Confirm, fakeQuery(-1, /*pad=*/true, /*button=*/3)));
}

TEST_CASE("input: a cleared action has no bindings", "[input]") {
    InputMap map;
    map.clear(InputAction::Menu);
    REQUIRE(map.keys(InputAction::Menu).empty());
    REQUIRE(map.buttons(InputAction::Menu).empty());
    REQUIRE_FALSE(map.isDown(InputAction::Menu, fakeQuery(0, true, 0)));
}

TEST_CASE("input: default bindings include keyboard and gamepad", "[input]") {
    const InputMap map;  // default constructor sets defaults
    REQUIRE_FALSE(map.keys(InputAction::Confirm).empty());
    REQUIRE_FALSE(map.buttons(InputAction::Confirm).empty());
    REQUIRE_FALSE(map.keys(InputAction::MoveLeft).empty());
    // Quit is intentionally left unbound by default.
    REQUIRE(map.keys(InputAction::Quit).empty());
}

TEST_CASE("input: missing query callbacks are treated as inactive", "[input]") {
    InputMap map;
    map.clear(InputAction::Confirm);
    map.bindKey(InputAction::Confirm, 1);
    map.bindButton(InputAction::Confirm, 1);

    InputQuery empty;  // all std::function members are null
    REQUIRE_FALSE(map.isDown(InputAction::Confirm, empty));
    REQUIRE_FALSE(map.isPressed(InputAction::Confirm, empty));
    REQUIRE_FALSE(map.isReleased(InputAction::Confirm, empty));
}
