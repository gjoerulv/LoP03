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
