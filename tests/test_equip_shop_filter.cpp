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
#include "states/ItemShopFilter.hpp"

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

    const std::vector<std::string> weapons = equipShopBuyIds(db, EquipSlot::Weapon, 7);
    const std::vector<std::string> armor = equipShopBuyIds(db, EquipSlot::Armor, 7);
    const std::vector<std::string> accessories = equipShopBuyIds(db, EquipSlot::Accessory, 7);

    // Every category is sorted for a stable menu order.
    CHECK(isSorted(weapons));
    CHECK(isSorted(armor));
    CHECK(isSorted(accessories));

    // The three categories partition the SHOP-STOCKED roster exactly: no stocked
    // item is dropped or listed twice. Legendary gear (M34) is not stocked (it is
    // black-market only), so it is excluded from the expected count.
    std::size_t stocked = 0;
    for (const auto& [id, def] : db.items()) {
        (void)id;
        if (isEquippableItem(def) && def.rarity != Rarity::Legendary) {
            ++stocked;
        }
    }
    CHECK(weapons.size() + armor.size() + accessories.size() == stocked);

    // No consumable, scroll, or legendary ever reaches a buy category.
    for (const std::vector<std::string>* list : {&weapons, &armor, &accessories}) {
        for (const std::string& id : *list) {
            const ItemDef* it = db.findItem(id);
            REQUIRE(it != nullptr);
            CHECK(isEquippableItem(*it));
            CHECK(it->type != ItemType::Consumable);
            CHECK(it->type != ItemType::Scroll);
            CHECK(it->rarity != Rarity::Legendary);
        }
    }
}

TEST_CASE("equipshop: legendary gear is black-market only, never stocked", "[equipshop]") {
    const ContentDatabase db = loadShippedContent();
    int legendaries = 0;
    for (const auto& [id, def] : db.items()) {
        if (def.rarity == Rarity::Legendary && isEquippableItem(def)) {
            ++legendaries;
            for (EquipSlot slot : {EquipSlot::Weapon, EquipSlot::Armor, EquipSlot::Accessory}) {
                CHECK_FALSE(contains(equipShopBuyIds(db, slot, 7), id));
            }
        }
    }
    CHECK(legendaries > 0);  // the shipped roster has legendaries (M34)
}

TEST_CASE("equipshop: each id lands only in the category matching its slot",
          "[equipshop]") {
    const ContentDatabase db = loadShippedContent();
    for (EquipSlot slot : {EquipSlot::Weapon, EquipSlot::Armor, EquipSlot::Accessory}) {
        for (const std::string& id : equipShopBuyIds(db, slot, 7)) {
            const ItemDef* it = db.findItem(id);
            REQUIRE(it != nullptr);
            CHECK(it->slot == slot);
        }
    }
}

TEST_CASE("equipshop: relics file under Accessories, not their own category",
          "[equipshop]") {
    const ContentDatabase db = loadShippedContent();
    const std::vector<std::string> accessories = equipShopBuyIds(db, EquipSlot::Accessory, 7);
    const std::vector<std::string> weapons = equipShopBuyIds(db, EquipSlot::Weapon, 7);
    const std::vector<std::string> armor = equipShopBuyIds(db, EquipSlot::Armor, 7);

    int relics = 0;
    for (const auto& [id, def] : db.items()) {
        if (def.type != ItemType::Relic) {
            continue;
        }
        CHECK(def.slot == EquipSlot::Accessory);  // data invariant the filter relies on
        if (def.rarity == Rarity::Legendary) {
            CHECK_FALSE(contains(accessories, id));  // legendary relics are black-market only
            continue;
        }
        ++relics;
        CHECK(contains(accessories, id));
        CHECK_FALSE(contains(weapons, id));
        CHECK_FALSE(contains(armor, id));
    }
    CHECK(relics > 0);  // the shipped roster has non-legendary relics
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

    CHECK(equipShopBuyIds(db, EquipSlot::Weapon, 7) == std::vector<std::string>{"sword"});
    CHECK(equipShopBuyIds(db, EquipSlot::Armor, 7).empty());
    CHECK(equipShopBuyIds(db, EquipSlot::Accessory, 7) == std::vector<std::string>{"relic"});
    // None-slot consumables/scrolls are never buyable gear.
    CHECK(equipShopBuyIds(db, EquipSlot::None, 7).empty());
}

TEST_CASE("equipshop: per-town gear is stocked only once its town is reached (M37)",
          "[equipshop]") {
    ContentDatabase db;
    ItemDef t1;  // town-1 blade (default minTown)
    t1.id = "t1_blade";
    t1.name = "T1 Blade";
    t1.type = ItemType::Equipment;
    t1.slot = EquipSlot::Weapon;
    db.addItem(t1);
    ItemDef t4;  // town-4 blade
    t4.id = "t4_blade";
    t4.name = "T4 Blade";
    t4.type = ItemType::Equipment;
    t4.slot = EquipSlot::Weapon;
    t4.minTown = 4;
    db.addItem(t4);

    CHECK(equipShopBuyIds(db, EquipSlot::Weapon, 1) == std::vector<std::string>{"t1_blade"});
    CHECK(equipShopBuyIds(db, EquipSlot::Weapon, 3) == std::vector<std::string>{"t1_blade"});
    // At town 4 both are stocked (sorted order).
    CHECK(equipShopBuyIds(db, EquipSlot::Weapon, 4) ==
          std::vector<std::string>{"t1_blade", "t4_blade"});
    CHECK(equipShopBuyIds(db, EquipSlot::Weapon, 7) ==
          std::vector<std::string>{"t1_blade", "t4_blade"});
}

TEST_CASE("equipshop: the shipped roster grows from town 1 to town 7 (M37)", "[equipshop]") {
    const ContentDatabase db = loadShippedContent();
    const std::size_t t1 = equipShopBuyIds(db, EquipSlot::Weapon, 1).size() +
                           equipShopBuyIds(db, EquipSlot::Armor, 1).size() +
                           equipShopBuyIds(db, EquipSlot::Accessory, 1).size();
    const std::size_t t7 = equipShopBuyIds(db, EquipSlot::Weapon, 7).size() +
                           equipShopBuyIds(db, EquipSlot::Armor, 7).size() +
                           equipShopBuyIds(db, EquipSlot::Accessory, 7).size();
    CHECK(t7 > t1);  // per-town gear (M37) unlocks as the ladder is climbed
}

// --- M43: the item shop gained the same kind of town window ------------------

TEST_CASE("itemshop: consumables are stocked inside their town window (M43)", "[itemshop]") {
    ContentDatabase db;
    ItemDef always;  // no window: every town
    always.id = "potion";
    always.name = "Potion";
    always.type = ItemType::Consumable;
    always.value = 25;
    db.addItem(always);
    ItemDef townOne;  // town-1 only
    townOne.id = "royal_snacks";
    townOne.name = "Royal Snacks";
    townOne.type = ItemType::Consumable;
    townOne.minTown = 1;
    townOne.maxTown = 1;
    townOne.value = 250;
    db.addItem(townOne);
    ItemDef late;  // town-5 and up
    late.id = "elixir";
    late.name = "Elixir";
    late.type = ItemType::Consumable;
    late.minTown = 5;
    late.value = 400;
    db.addItem(late);
    ItemDef gear;  // not a consumable: the item shop never stocks it
    gear.id = "blade";
    gear.name = "Blade";
    gear.type = ItemType::Equipment;
    gear.slot = EquipSlot::Weapon;
    db.addItem(gear);

    ItemDef granted;  // M44: a valueless item is granted, never sold
    granted.id = "evil_goose";
    granted.name = "Evil Goose";
    granted.type = ItemType::Consumable;
    granted.value = 0;
    db.addItem(granted);

    CHECK(cd::itemShopBuyIds(db, 1) == std::vector<std::string>{"potion", "royal_snacks"});
    CHECK(cd::itemShopBuyIds(db, 2) == std::vector<std::string>{"potion"});
    CHECK(cd::itemShopBuyIds(db, 5) == std::vector<std::string>{"elixir", "potion"});
}

TEST_CASE("itemshop: the shipped Royal Snacks are sold in town 1 and nowhere else (M43)",
          "[itemshop]") {
    const ContentDatabase db = loadShippedContent();
    CHECK(contains(cd::itemShopBuyIds(db, 1), "royal_snacks"));
    for (int town = 2; town <= 7; ++town) {
        CHECK_FALSE(contains(cd::itemShopBuyIds(db, town), "royal_snacks"));
    }
    // Every other consumable is unaffected by the new window.
    for (int town = 1; town <= 7; ++town) {
        CHECK(contains(cd::itemShopBuyIds(db, town), "potion"));
        CHECK(isSorted(cd::itemShopBuyIds(db, town)));
    }
}

TEST_CASE("itemshop: the shipped Royal Relics are never for sale (M44)", "[itemshop]") {
    const ContentDatabase db = loadShippedContent();
    for (int town = 1; town <= 7; ++town) {
        for (const char* relic : {"evil_goose", "tax_sheets", "dragon_crown", "deadly_spoon"}) {
            CHECK_FALSE(contains(cd::itemShopBuyIds(db, town), relic));
        }
    }
}
