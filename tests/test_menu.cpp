#include <catch2/catch_test_macros.hpp>

#include "ui/Menu.hpp"

using namespace cd::ui;

TEST_CASE("menu: navigation wraps and skips disabled items", "[ui]") {
    Menu m({{"A", true}, {"B", false}, {"C", true}});
    REQUIRE(m.cursor() == 0);

    m.moveDown();  // skip disabled B
    REQUIRE(m.cursor() == 2);
    m.moveDown();  // wrap to A
    REQUIRE(m.cursor() == 0);
    m.moveUp();  // wrap up to C, skipping B
    REQUIRE(m.cursor() == 2);
}

TEST_CASE("menu: setCursor settles on an enabled item", "[ui]") {
    Menu m({{"A", true}, {"B", false}, {"C", true}});
    m.setCursor(1);  // B disabled -> nudges forward to C
    REQUIRE(m.cursor() == 2);
    REQUIRE(m.currentEnabled());
}

TEST_CASE("menu: all-disabled leaves the cursor put and reports not enabled", "[ui]") {
    Menu m({{"X", false}, {"Y", false}});
    m.moveDown();
    REQUIRE(m.cursor() == 0);
    REQUIRE_FALSE(m.currentEnabled());
    REQUIRE(m.current() != nullptr);
}

TEST_CASE("menu: empty menu has no current item", "[ui]") {
    Menu m;
    REQUIRE(m.empty());
    REQUIRE(m.current() == nullptr);
    REQUIRE_FALSE(m.currentEnabled());
    m.moveDown();  // must not crash
    REQUIRE(m.cursor() == 0);
}

// --- M121: focusable disabled rows (the skill lists) ---------------------------

TEST_CASE("menu: a focusable-disabled list lets the cursor rest on greyed rows (M121)",
          "[ui][m121]") {
    cd::ui::Menu menu;
    menu.setFocusDisabled(true);
    menu.setItems({{"Strike", true}, {"Fireball", false}, {"Mend", false}, {"Guard", true}});
    CHECK(menu.cursor() == 0);
    menu.moveDown();
    CHECK(menu.cursor() == 1);  // greyed, and the cursor stays on it
    CHECK_FALSE(menu.currentEnabled());  // ...but Confirm still knows better
    menu.moveDown();
    CHECK(menu.cursor() == 2);
    menu.moveDown();
    menu.moveDown();
    CHECK(menu.cursor() == 0);  // wraps like any list
    menu.moveUp();
    CHECK(menu.cursor() == 3);
    menu.setCursor(2);
    CHECK(menu.cursor() == 2);  // no nudge to an enabled neighbour

    // The mode belongs to the list, not to its rows: a rebuild keeps it.
    menu.setItems({{"Only", false}});
    CHECK(menu.focusDisabled());
    CHECK(menu.cursor() == 0);
    CHECK_FALSE(menu.currentEnabled());

    // Every other menu keeps skipping, exactly as before.
    cd::ui::Menu classic({{"A", true}, {"B", false}, {"C", true}});
    classic.moveDown();
    CHECK(classic.cursor() == 2);
}
