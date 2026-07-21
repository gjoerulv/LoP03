#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/BlackMarket.hpp"

// M34 black market: the pure, deterministic spawn rules (seeded roll, price,
// tile/item picks) and the shipped legendary roster. Save round-trip lives in
// test_save_system; the purchase screen and token award are UI/integration,
// covered by the manual matrix.

using namespace cd;

TEST_CASE("black market: the spawn roll is deterministic and about 20%", "[blackmarket]") {
    CHECK(blackMarketRolls(12345u, 3, 5) == blackMarketRolls(12345u, 3, 5));  // reload-proof
    int hits = 0;
    for (std::uint64_t s = 1; s <= 4000; ++s) {
        if (blackMarketRolls(s * 2654435761u, 3, 5)) {
            ++hits;
        }
    }
    // 20% of 4000 = 800; a generous band around it.
    CHECK(hits > 650);
    CHECK(hits < 950);
}

TEST_CASE("black market: gold price has a 5000 floor and modest town scaling",
          "[blackmarket]") {
    CHECK(blackMarketPriceGold(1) == 5000);  // clamped to the floor (never town 1 anyway)
    CHECK(blackMarketPriceGold(2) == 5000);
    CHECK(blackMarketPriceGold(3) == 5750);
    CHECK(blackMarketPriceGold(7) == 8750);
    CHECK(kBlackMarketTokenPrice == 3);
    CHECK(kBlackMarketMinTown == 2);
}

TEST_CASE("black market: tile and item picks are in range and deterministic",
          "[blackmarket]") {
    for (std::uint64_t s : {1ull, 42ull, 999999ull, 18446744073709551615ull}) {
        const int tile = blackMarketTileIndex(s);
        CHECK(tile >= 0);
        CHECK(tile < kBlackMarketTileCount);
        CHECK(blackMarketTileIndex(s) == blackMarketTileIndex(s));

        const int item = blackMarketItemIndex(s, 5);
        CHECK(item >= 0);
        CHECK(item < 5);
        CHECK(blackMarketItemIndex(s, 5) == blackMarketItemIndex(s, 5));
    }
    CHECK(blackMarketItemIndex(7u, 0) == 0);  // empty-roster guard
}

TEST_CASE("black market: candidate tiles are interior plaza cells, not the spawn",
          "[blackmarket]") {
    for (const MarketTile& t : kBlackMarketTiles) {
        CHECK(t.x > 0);
        CHECK(t.x < 25);
        CHECK(t.y > 0);
        CHECK(t.y < 14);
        CHECK_FALSE((t.x == 12 && t.y == 8));  // not the player spawn tile
    }
}

TEST_CASE("black market: the shipped roster has legendary gear to sell", "[blackmarket]") {
    content::ContentDatabase db;
    content::LoadReport rep;
    REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep));

    int legendaries = 0;
    for (const auto& [id, def] : db.items()) {
        (void)id;
        if (def.rarity != content::Rarity::Legendary) {
            continue;
        }
        ++legendaries;
        CHECK((def.type == content::ItemType::Equipment || def.type == content::ItemType::Relic));
        CHECK(def.slot != content::EquipSlot::None);
    }
    CHECK(legendaries >= 5);
}

TEST_CASE("black market: legendaries never drop from chests", "[blackmarket]") {
    content::ContentDatabase db;
    content::LoadReport rep;
    REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep));
    const char* themes[] = {"ruined_keep", "crystal_mine", "hollow_forest"};

    int chestsWithItems = 0;
    for (std::uint64_t seed = 1; seed <= 60; ++seed) {
        for (int depth : {1, 4, 8, 12}) {
            for (int town : {1, 4, 7}) {
                const cd::dungeon::Dungeon d = cd::dungeon::generate(
                    seed * 6151u, depth, db, themes[seed % 3], town);
                for (const cd::dungeon::Room& r : d.rooms) {
                    if (!r.chest.present || r.chest.itemId.empty()) {
                        continue;
                    }
                    const content::ItemDef* it = db.findItem(r.chest.itemId);
                    REQUIRE(it != nullptr);
                    ++chestsWithItems;
                    CHECK(it->rarity != content::Rarity::Legendary);  // black-market only
                }
            }
        }
    }
    CHECK(chestsWithItems > 100);  // the sweep really did see item chests
}

TEST_CASE("black market: the spawn predicate captures every trigger condition",
          "[blackmarket]") {
    // A seed that wins the roll for (town 3, depth 5).
    std::uint64_t hitSeed = 0;
    for (std::uint64_t s = 1; s < 1000; ++s) {
        if (blackMarketRolls(s, 3, 5)) {
            hitSeed = s;
            break;
        }
    }
    REQUIRE(hitSeed != 0);

    // All conditions must hold together: a scoring completion, a stakes raise,
    // town >= 2, and the roll.
    CHECK(blackMarketShouldSpawn(true, true, 3, hitSeed, 5));
    CHECK_FALSE(blackMarketShouldSpawn(false, true, 3, hitSeed, 5));  // not completed
    CHECK_FALSE(blackMarketShouldSpawn(true, false, 3, hitSeed, 5));  // did not raise stakes
    CHECK_FALSE(blackMarketShouldSpawn(true, true, 1, hitSeed, 5));   // town 1 never spawns

    // A qualifying run still needs the roll: about 20% of seeds spawn.
    int hits = 0;
    for (std::uint64_t s = 1; s <= 3000; ++s) {
        if (blackMarketShouldSpawn(true, true, 3, s * 2654435761u, 5)) {
            ++hits;
        }
    }
    CHECK(hits > 450);
    CHECK(hits < 750);
}
