#include <catch2/catch_test_macros.hpp>

#include "game/Inventory.hpp"

using namespace cd;

TEST_CASE("inventory: add stacks, count, and remove", "[game]") {
    Inventory inv;
    REQUIRE(inv.empty());
    inv.add("potion", 3);
    inv.add("potion", 2);
    REQUIRE(inv.count("potion") == 5);
    REQUIRE(inv.stacks.size() == 1);

    REQUIRE(inv.remove("potion", 4));
    REQUIRE(inv.count("potion") == 1);

    REQUIRE_FALSE(inv.remove("potion", 5));  // not enough
    REQUIRE(inv.count("potion") == 1);

    REQUIRE(inv.remove("potion", 1));
    REQUIRE(inv.count("potion") == 0);
    REQUIRE(inv.empty());  // emptied stacks are erased

    REQUIRE_FALSE(inv.remove("ether", 1));  // missing item
}

TEST_CASE("inventory: preserves insertion order", "[game]") {
    Inventory inv;
    inv.add("potion", 1);
    inv.add("ether", 1);
    inv.add("antidote", 1);
    REQUIRE(inv.stacks.size() == 3);
    REQUIRE(inv.stacks[0].itemId == "potion");
    REQUIRE(inv.stacks[1].itemId == "ether");
    REQUIRE(inv.stacks[2].itemId == "antidote");
}
