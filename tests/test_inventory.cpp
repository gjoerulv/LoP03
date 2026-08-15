#include <catch2/catch_test_macros.hpp>

#include "game/Inventory.hpp"
#include "game/ItemUse.hpp"  // M90

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

// --- M90: out-of-battle use (the pause menus' Items screen) -------------------

namespace {
content::ItemDef consumableWith(content::ConsumableEffect effect, int amount) {
    content::ItemDef d;
    d.id = "test_item";
    d.name = "Test Item";
    d.type = content::ItemType::Consumable;
    d.effect = effect;
    d.effectAmount = amount;
    return d;
}

Character member(int hp, int maxHp, int mp, int maxMp) {
    Character c;
    c.name = "Rolan";
    c.hp = hp;
    c.maxHp = maxHp;
    c.mp = mp;
    c.maxMp = maxMp;
    return c;
}
}  // namespace

TEST_CASE("item use: the M43 gating carries outside battle (M90)", "[game][itemuse]") {
    const content::ItemDef heal = consumableWith(content::ConsumableEffect::Heal, 30);
    const content::ItemDef mp = consumableWith(content::ConsumableEffect::RestoreMp, 20);
    const content::ItemDef revive = consumableWith(content::ConsumableEffect::Revive, 25);
    const content::ItemDef cure = consumableWith(content::ConsumableEffect::Cure, 0);

    Character hurt = member(10, 50, 5, 20);
    Character full = member(50, 50, 20, 20);
    Character fallen = member(0, 50, 8, 20);

    // Heals and MP reach only the living with room; revives only the fallen.
    CHECK(itemUseRefusal(hurt, heal).empty());
    CHECK_FALSE(itemUseRefusal(full, heal).empty());
    CHECK_FALSE(itemUseRefusal(fallen, heal).empty());
    CHECK(itemUseRefusal(hurt, mp).empty());
    CHECK_FALSE(itemUseRefusal(full, mp).empty());
    CHECK_FALSE(itemUseRefusal(fallen, mp).empty());
    CHECK(itemUseRefusal(fallen, revive).empty());
    CHECK_FALSE(itemUseRefusal(hurt, revive).empty());
    // Statuses are battle-scoped: a cure has nothing to reach out here.
    CHECK_FALSE(itemUseRefusal(hurt, cure).empty());

    // Equipment is never bag-usable.
    content::ItemDef gear;
    gear.type = content::ItemType::Equipment;
    CHECK_FALSE(itemUseRefusal(hurt, gear).empty());
}

TEST_CASE("item use: applications cap and revive at the authored percent (M90)",
          "[game][itemuse]") {
    const content::ItemDef heal = consumableWith(content::ConsumableEffect::Heal, 30);
    Character hurt = member(30, 50, 5, 20);
    CHECK_FALSE(applyItemUse(hurt, heal).empty());
    CHECK(hurt.hp == 50);  // capped at max, never past

    const content::ItemDef mp = consumableWith(content::ConsumableEffect::RestoreMp, 20);
    Character drained = member(30, 50, 15, 20);
    applyItemUse(drained, mp);
    CHECK(drained.mp == 20);

    const content::ItemDef revive = consumableWith(content::ConsumableEffect::Revive, 25);
    Character fallen = member(0, 50, 8, 20);
    CHECK_FALSE(applyItemUse(fallen, revive).empty());
    CHECK(fallen.hp == 12);  // 25% of 50, floored
    CHECK(fallen.mp == 8);   // MP untouched by a revive

    const content::ItemDef tinyRevive = consumableWith(content::ConsumableEffect::Revive, 1);
    Character gone = member(0, 50, 0, 20);
    applyItemUse(gone, tinyRevive);
    CHECK(gone.hp == 1);  // never rises at 0 HP
}
