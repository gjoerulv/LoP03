#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "input/Remap.hpp"

using cd::InputAction;
using cd::InputMap;
using namespace cd::input;

namespace {
constexpr int kKeyEnter = 257;
constexpr int kKeyEscape = 256;
constexpr int kKeyJ = 74;
constexpr int kButtonA = 7;
constexpr int kButtonB = 6;

bool hasKey(const InputMap& map, InputAction a, int key) {
    const auto& keys = map.keys(a);
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}
}  // namespace

TEST_CASE("remap: binding a free key rebinds cleanly") {
    InputMap map;
    const RemapResult r = remapKey(map, InputAction::Confirm, kKeyJ);
    CHECK(r.outcome == RemapOutcome::Rebound);
    REQUIRE(map.keys(InputAction::Confirm).size() == 1);
    CHECK(map.keys(InputAction::Confirm).front() == kKeyJ);
    // Gamepad bindings untouched.
    CHECK_FALSE(map.buttons(InputAction::Confirm).empty());
}

TEST_CASE("remap: conflicts swap with the previous owner") {
    InputMap map;
    // Tab is Menu's key; give it to Confirm.
    const int kKeyTab = 258;
    const RemapResult r = remapKey(map, InputAction::Confirm, kKeyTab);
    CHECK(r.outcome == RemapOutcome::Swapped);
    CHECK(r.swappedWith == InputAction::Menu);
    CHECK(hasKey(map, InputAction::Confirm, kKeyTab));
    // Menu got Confirm's old primary (Enter) and is not unbound.
    CHECK_FALSE(map.keys(InputAction::Menu).empty());
    CHECK(hasKey(map, InputAction::Menu, kKeyEnter));
    CHECK_FALSE(hasKey(map, InputAction::Menu, kKeyTab));
}

TEST_CASE("remap: no action ever ends up unbound after any swap") {
    InputMap map;
    remapKey(map, InputAction::Confirm, kKeyJ);
    remapKey(map, InputAction::Menu, kKeyJ);      // steals J from Confirm
    remapKey(map, InputAction::Cancel, kKeyJ);    // steals J from Menu
    for (InputAction a : cd::kRemappableActions) {
        CHECK_FALSE(map.keys(a).empty());
    }
}

TEST_CASE("remap: Esc is reserved and blocked") {
    InputMap map;
    const auto before = map.keys(InputAction::Confirm);
    const RemapResult r = remapKey(map, InputAction::Confirm, kKeyEscape);
    CHECK(r.outcome == RemapOutcome::Blocked);
    CHECK(map.keys(InputAction::Confirm) == before);
}

TEST_CASE("remap: non-remappable actions are blocked") {
    InputMap map;
    CHECK(remapKey(map, InputAction::ToggleDebug, kKeyJ).outcome == RemapOutcome::Blocked);
    CHECK(remapKey(map, InputAction::TextBackspace, kKeyJ).outcome == RemapOutcome::Blocked);
}

TEST_CASE("remap: buttons swap like keys and keys are untouched") {
    InputMap map;
    const RemapResult r = remapButton(map, InputAction::Confirm, kButtonB);  // B owned by Cancel
    CHECK(r.outcome == RemapOutcome::Swapped);
    CHECK(r.swappedWith == InputAction::Cancel);
    REQUIRE(map.buttons(InputAction::Confirm).size() == 1);
    CHECK(map.buttons(InputAction::Confirm).front() == kButtonB);
    // Cancel took Confirm's old A button.
    REQUIRE_FALSE(map.buttons(InputAction::Cancel).empty());
    CHECK(map.buttons(InputAction::Cancel).front() == kButtonA);
    // Keyboard side unchanged.
    CHECK(hasKey(map, InputAction::Confirm, kKeyEnter));
}

TEST_CASE("remap: reset restores the defaults") {
    InputMap map;
    remapKey(map, InputAction::Confirm, kKeyJ);
    remapButton(map, InputAction::Menu, kButtonA);
    resetBindings(map);
    const InputMap defaults;
    for (InputAction a : cd::kRemappableActions) {
        CHECK(map.keys(a) == defaults.keys(a));
        CHECK(map.buttons(a) == defaults.buttons(a));
    }
}
