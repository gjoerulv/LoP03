#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/Enums.hpp"
#include "content/LoadReport.hpp"
#include "states/EquipShopFilter.hpp"

// M31: the Equip Shop's "Buy Gear" list splits into Weapons / Armor /
// Accessories. The split is a pure slot filter (equipShopBuyIds); these tests
// pin its behaviour against both the shipped roster and hand-built content, so
// the category menu logic is guarded without a raylib/window dependency.

using cd::equipShopBuyIds;
using cd::isEquippableItem;
using namespace cd::content;

namespace {

ContentDatabase loadShippedContent() {
    ContentDatabase db;
    LoadReport report;
    REQUIRE(cd::content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, report));
    return db;
}

bool contains(const std::vector<std::string>& v, const std::string& id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

bool isSorted(const std::vector<std::string>& v) {
    return std::is_sorted(v.begin(), v.end());
}

}  // namespace

TEST_CASE("equipshop: shipped roster partitions cleanly into the three categories",
          "[equipshop]") {
    const ContentDatabase db = loadShippedContent();

    const std::vector<std::string> weapons = equipShopBuyIds(db, EquipSlot::Weapon);
    const std::vector<std::string> armor = equipShopBuyIds(db, EquipSlot::Armor);
    const std::vector<std::string> accessories = equipShopBuyIds(db, EquipSlot::Accessory);

    // Every category is sorted for a stable menu order.
    CHECK(isSorted(weapons));
    CHECK(isSorted(armor));
    CHECK(isSorted(accessories));

    // The three categories partition the equippable roster exactly: no item is
    // dropped, and none appears in two lists.
    std::size_t equippable = 0;
    for (const auto& [id, def] : db.items()) {
        (void)id;
        if (isEquippableItem(def)) {
            ++equippable;
        }
    }
    CHECK(weapons.size() + armor.size() + accessories.size() == equippable);

    // No consumable or scroll ever reaches a buy category.
    for (const std::vector<std::string>* list : {&weapons, &armor, &accessories}) {
        for (const std::string& id : *list) {
            const ItemDef* it = db.findItem(id);
            REQUIRE(it != nullptr);
            CHECK(isEquippableItem(*it));
            CHECK(it->type != ItemType::Consumable);
            CHECK(it->type != ItemType::Scroll);
        }
    }
}

TEST_CASE("equipshop: each id lands only in the category matching its slot",
          "[equipshop]") {
    const ContentDatabase db = loadShippedContent();
    for (EquipSlot slot : {EquipSlot::Weapon, EquipSlot::Armor, EquipSlot::Accessory}) {
        for (const std::string& id : equipShopBuyIds(db, slot)) {
            const ItemDef* it = db.findItem(id);
            REQUIRE(it != nullptr);
            CHECK(it->slot == slot);
        }
    }
}

TEST_CASE("equipshop: relics file under Accessories, not their own category",
          "[equipshop]") {
    const ContentDatabase db = loadShippedContent();
    const std::vector<std::string> accessories = equipShopBuyIds(db, EquipSlot::Accessory);
    const std::vector<std::string> weapons = equipShopBuyIds(db, EquipSlot::Weapon);
    const std::vector<std::string> armor = equipShopBuyIds(db, EquipSlot::Armor);

    int relics = 0;
    for (const auto& [id, def] : db.items()) {
        if (def.type == ItemType::Relic) {
            ++relics;
            CHECK(def.slot == EquipSlot::Accessory);  // data invariant the filter relies on
            CHECK(contains(accessories, id));
            CHECK_FALSE(contains(weapons, id));
            CHECK_FALSE(contains(armor, id));
        }
    }
    CHECK(relics > 0);  // the shipped roster has relics, so the test is meaningful
}

TEST_CASE("equipshop: filter ignores consumables and scrolls in hand-built content",
          "[equipshop]") {
    ContentDatabase db;

    ItemDef sword;
    sword.id = "sword";
    sword.name = "Sword";
    sword.type = ItemType::Equipment;
    sword.slot = EquipSlot::Weapon;
    db.addItem(sword);

    ItemDef relic;
    relic.id = "relic";
    relic.name = "Relic";
    relic.type = ItemType::Relic;
    relic.slot = EquipSlot::Accessory;
    db.addItem(relic);

    ItemDef potion;
    potion.id = "potion";
    potion.name = "Potion";
    potion.type = ItemType::Consumable;
    potion.slot = EquipSlot::None;
    db.addItem(potion);

    ItemDef scroll;
    scroll.id = "scroll";
    scroll.name = "Scroll";
    scroll.type = ItemType::Scroll;
    scroll.slot = EquipSlot::None;
    db.addItem(scroll);

    CHECK(equipShopBuyIds(db, EquipSlot::Weapon) == std::vector<std::string>{"sword"});
    CHECK(equipShopBuyIds(db, EquipSlot::Armor).empty());
    CHECK(equipShopBuyIds(db, EquipSlot::Accessory) == std::vector<std::string>{"relic"});
    // None-slot consumables/scrolls are never buyable gear.
    CHECK(equipShopBuyIds(db, EquipSlot::None).empty());
}
