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
    db.addClass(knight);

    content::ItemDef sword;
    sword.id = "iron_sword";
    sword.name = "Iron Sword";
    sword.type = content::ItemType::Equipment;
    sword.slot = content::EquipSlot::Weapon;
    sword.statBonus = {0, 6, 0, 0, 0};
    db.addItem(sword);

    content::ItemDef charm;
    charm.id = "vitality_charm";
    charm.name = "Vitality Charm";
    charm.type = content::ItemType::Equipment;
    charm.slot = content::EquipSlot::Accessory;
    charm.statBonus = {30, 0, 0, 0, 0};  // +30 max HP
    db.addItem(charm);
    return db;
}
}  // namespace

TEST_CASE("equipment: bonuses fold into derived stats", "[equipment]") {
    const content::ContentDatabase db = makeDb();
    Character c = createCharacter(*db.findClass("knight"), "Hero");
    REQUIRE(c.stats.attack == 18);
    REQUIRE(c.maxHp == 120);

    c.weapon = "iron_sword";
    refreshCharacter(c, db);
    REQUIRE(c.stats.attack == 24);  // 18 + 6

    c.accessory = "vitality_charm";
    refreshCharacter(c, db);
    REQUIRE(c.maxHp == 150);  // 120 + 30
    REQUIRE(c.stats.attack == 24);
}

TEST_CASE("equipment: unequipping restores base stats", "[equipment]") {
    const content::ContentDatabase db = makeDb();
    Character c = createCharacter(*db.findClass("knight"), "Hero");
    c.weapon = "iron_sword";
    refreshCharacter(c, db);
    REQUIRE(c.stats.attack == 24);

    c.weapon.clear();
    refreshCharacter(c, db);
    REQUIRE(c.stats.attack == 18);
}

TEST_CASE("equipment: unknown equipment ids are ignored", "[equipment]") {
    const content::ContentDatabase db = makeDb();
    Character c = createCharacter(*db.findClass("knight"), "Hero");
    c.weapon = "does_not_exist";
    refreshCharacter(c, db);
    REQUIRE(c.stats.attack == 18);  // no crash, no bonus
}
