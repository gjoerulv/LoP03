#include <catch2/catch_test_macros.hpp>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "game/Party.hpp"

using namespace cd;

namespace {
content::ContentDatabase makeDb() {
    content::ContentDatabase db;
    content::ClassDef knight;
    knight.id = "knight";
    knight.name = "Knight";
    knight.baseStats = {120, 18, 4, 16, 8};
    knight.growth = {12.0f, 2.0f, 0.4f, 1.6f, 0.8f};
    db.addClass(knight);
    return db;
}
}  // namespace

TEST_CASE("leveling: xpToNext grows with level", "[leveling]") {
    REQUIRE(xpToNext(1) < xpToNext(2));
    REQUIRE(xpToNext(2) < xpToNext(5));
}

TEST_CASE("leveling: granting XP raises level and recomputes stats", "[leveling]") {
    const content::ContentDatabase db = makeDb();
    Character c = createCharacter(*db.findClass("knight"), "Hero");
    REQUIRE(c.level == 1);
    REQUIRE(c.stats.attack == 18);

    grantXp(c, xpToNext(1) + xpToNext(2), db);  // exactly enough for two levels
    REQUIRE(c.level == 3);
    REQUIRE(c.stats.attack == 18 + 4);    // +2.0 per level * 2
    REQUIRE(c.stats.maxHp == 120 + 24);   // +12 per level * 2
    REQUIRE(c.xp == 0);
}

TEST_CASE("leveling: a level-up heals the HP gained", "[leveling]") {
    const content::ContentDatabase db = makeDb();
    Character c = createCharacter(*db.findClass("knight"), "Hero");
    c.hp = 10;
    const int beforeMax = c.maxHp;
    grantXp(c, xpToNext(1), db);  // one level
    REQUIRE(c.level == 2);
    REQUIRE(c.hp == 10 + (c.maxHp - beforeMax));
    REQUIRE(c.hp <= c.maxHp);
}

TEST_CASE("leveling: level is capped", "[leveling]") {
    const content::ContentDatabase db = makeDb();
    Character c = createCharacter(*db.findClass("knight"), "Hero");
    grantXp(c, 100000000, db);
    REQUIRE(c.level == kMaxLevel);
}

TEST_CASE("leveling: party XP goes to every member", "[leveling]") {
    const content::ContentDatabase db = makeDb();
    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "A"));
    p.members.push_back(createCharacter(*db.findClass("knight"), "B"));
    grantPartyXp(p, xpToNext(1), db);
    REQUIRE(p.members[0].level == 2);
    REQUIRE(p.members[1].level == 2);
}
