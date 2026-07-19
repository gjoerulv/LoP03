#include <catch2/catch_test_macros.hpp>

#include "input/PromptLabels.hpp"

using cd::ActiveDevice;
using cd::InputAction;
using cd::InputMap;
using namespace cd::input;

TEST_CASE("labels: key names") {
    CHECK(keyName(257) == "Enter");
    CHECK(keyName(256) == "Esc");
    CHECK(keyName(32) == "Space");
    CHECK(keyName(259) == "Backspace");
    CHECK(keyName(87) == "W");
    CHECK(keyName(290) == "F1");
    CHECK(keyName(999) == "Key#999");
}

TEST_CASE("labels: button names") {
    CHECK(buttonName(7) == "A");
    CHECK(buttonName(6) == "B");
    CHECK(buttonName(15) == "Start");
    CHECK(buttonName(1) == "D-Pad Up");
    CHECK(buttonName(42) == "Btn#42");
}

TEST_CASE("labels: primary label reflects the live map per device") {
    InputMap map;
    CHECK(primaryLabel(map, InputAction::Confirm, ActiveDevice::Keyboard) == "Enter");
    CHECK(primaryLabel(map, InputAction::Confirm, ActiveDevice::Gamepad) == "A");
    CHECK(primaryLabel(map, InputAction::Menu, ActiveDevice::Keyboard) == "Tab");
    CHECK(primaryLabel(map, InputAction::Menu, ActiveDevice::Gamepad) == "Start");

    // After a rebind the label follows.
    map.clear(InputAction::Menu);
    map.bindKey(InputAction::Menu, 77);  // 'M'
    CHECK(primaryLabel(map, InputAction::Menu, ActiveDevice::Keyboard) == "M");
    CHECK(primaryLabel(map, InputAction::Menu, ActiveDevice::Gamepad) == "-");
}

TEST_CASE("labels: allLabels joins bindings; prompt composes") {
    InputMap map;
    CHECK(allLabels(map, InputAction::Confirm, ActiveDevice::Keyboard) == "Enter or Space");
    CHECK(prompt(map, InputAction::Cancel, ActiveDevice::Keyboard, "Back") == "[Backspace] Back");
    CHECK(prompt(map, InputAction::Confirm, ActiveDevice::Gamepad, "Buy") == "[A] Buy");
}

TEST_CASE("labels: movement falls back to the stick on an unbound gamepad") {
    InputMap map;
    map.clear(InputAction::MoveUp);
    CHECK(primaryLabel(map, InputAction::MoveUp, ActiveDevice::Gamepad) == "Stick Up");
    CHECK(allLabels(map, InputAction::MoveUp, ActiveDevice::Gamepad) == "Stick");
}
