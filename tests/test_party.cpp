#include <catch2/catch_test_macros.hpp>

#include "content/Definitions.hpp"
#include "game/Party.hpp"

using namespace cd;

namespace {
content::ClassDef knightClass() {
    content::ClassDef c;
    c.id = "knight";
    c.name = "Knight";
    c.baseStats = {120, 18, 4, 16, 8};
    c.growth = {12.0f, 2.0f, 0.4f, 1.6f, 0.8f};
    return c;
}
}  // namespace

TEST_CASE("party: createCharacter derives level-1 stats from the class", "[game]") {
    const content::ClassDef knight = knightClass();
    const Character c = createCharacter(knight, "Rolan");

    REQUIRE(c.classId == "knight");
    REQUIRE(c.name == "Rolan");
    REQUIRE(c.level == 1);
    REQUIRE(c.stats.maxHp == 120);
    REQUIRE(c.stats.attack == 18);
    REQUIRE(c.maxHp == 120);
    REQUIRE(c.hp == 120);  // starts full
    REQUIRE(c.maxMp == deriveMaxMp(4));
    REQUIRE(c.mp == c.maxMp);
}

TEST_CASE("party: growth applies per level (truncated, deterministic)", "[game]") {
    const content::ClassDef knight = knightClass();
    const Character c3 = createCharacter(knight, "X", 3);
    REQUIRE(c3.level == 3);
    REQUIRE(c3.stats.maxHp == 120 + 24);  // +12 * 2
    REQUIRE(c3.stats.attack == 18 + 4);   // +2 * 2
    REQUIRE(c3.stats.magic == 4);         // +0.4 * 2 = 0.8 -> truncates to 0
}

TEST_CASE("party: healFull and highestLevel", "[game]") {
    const content::ClassDef knight = knightClass();
    Party p;
    p.members.push_back(createCharacter(knight, "A", 1));
    p.members.push_back(createCharacter(knight, "B", 4));
    p.members[0].hp = 1;
    p.members[1].mp = 0;

    healFull(p);
    REQUIRE(p.members[0].hp == p.members[0].maxHp);
    REQUIRE(p.members[1].mp == p.members[1].maxMp);
    REQUIRE(highestLevel(p) == 4);
}
