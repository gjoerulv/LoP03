#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/RoomLayout.hpp"
#include "dungeon/ThemeEvents.hpp"

// M55 per-theme rites: the generator guarantees exactly one theme rite per themed
// dungeon and never a cross-theme one; the pure helpers (tier-up, cache, root)
// behave as designed and deterministically. Content-loading cases need the real
// data (CRYSTAL_TEST_DATA_DIR); the pure-math cases run anywhere.

using namespace cd;
using namespace cd::dungeon;

TEST_CASE("theme rites: generation version bumped to 11", "[theme-events]") {
    CHECK(kGenerationVersion == 11);
}

TEST_CASE("theme rites: one rite per theme; empty/unknown themes force nothing",
          "[theme-events]") {
    CHECK(themeEventKind("ruined_keep") == RoomEventKind::ArmoryGhost);
    CHECK(themeEventKind("crystal_mine") == RoomEventKind::MinersCache);
    CHECK(themeEventKind("hollow_forest") == RoomEventKind::ElderRoot);
    CHECK(themeEventKind("") == RoomEventKind::None);
    CHECK(themeEventKind("no_such_theme") == RoomEventKind::None);
}

TEST_CASE("theme rites: rarity steps up and legendary is the ceiling", "[theme-events]") {
    CHECK(nextRarityUp(content::Rarity::Common) == content::Rarity::Uncommon);
    CHECK(nextRarityUp(content::Rarity::Uncommon) == content::Rarity::Rare);
    CHECK(nextRarityUp(content::Rarity::Rare) == content::Rarity::Epic);
    CHECK(nextRarityUp(content::Rarity::Epic) == content::Rarity::Legendary);
    CHECK(nextRarityUp(content::Rarity::Legendary) == content::Rarity::Legendary);
}

TEST_CASE("theme rites: Miner's Cache wound is never fatal and pays above a trapped chest",
          "[theme-events]") {
    // A third of max HP; the caller clamps HP to >= 1, so the wound never kills.
    CHECK(minersCacheWound(300) == 100);
    CHECK(minersCacheWound(3) == 1);
    CHECK(minersCacheWound(2) == 0);
    // The reward exceeds the biggest possible trapped chest at every depth.
    for (int depth = 1; depth <= 20; ++depth) {
        const int maxTrapped = 55 * depth + 15;  // base rng(10,30)*d max + trapped add
        INFO("depth=" << depth);
        CHECK(minersCacheGold(depth) > maxTrapped);
    }
}

#ifdef CRYSTAL_TEST_DATA_DIR

namespace {
content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

int countRite(const Dungeon& d, RoomEventKind rite) {
    int n = 0;
    for (const Room& r : d.rooms) {
        if (r.type == RoomType::Event && r.event.kind == rite) {
            ++n;
        }
    }
    return n;
}

int countAnyRite(const Dungeon& d) {
    return countRite(d, RoomEventKind::ArmoryGhost) + countRite(d, RoomEventKind::MinersCache) +
           countRite(d, RoomEventKind::ElderRoot);
}
}  // namespace

TEST_CASE("theme rites: each theme guarantees its rite exactly once, never cross-theme",
          "[theme-events]") {
    const content::ContentDatabase db = loadContent();
    struct ThemeCase {
        const char* id;
        RoomEventKind rite;
    };
    const ThemeCase cases[] = {
        {"ruined_keep", RoomEventKind::ArmoryGhost},
        {"crystal_mine", RoomEventKind::MinersCache},
        {"hollow_forest", RoomEventKind::ElderRoot},
    };
    for (const ThemeCase& tc : cases) {
        for (std::uint64_t seed = 1; seed <= 150; ++seed) {
            const int depth = 1 + static_cast<int>(seed % 12);
            const int town = 1 + static_cast<int>(seed % 7);
            const Dungeon d = generate(seed, depth, db, tc.id, town);
            INFO("theme=" << tc.id << " seed=" << seed << " depth=" << depth << " town=" << town);
            CHECK(countRite(d, tc.rite) == 1);  // exactly this theme's rite, once
            CHECK(countAnyRite(d) == 1);        // and no other rite ever appears
        }
    }
}

TEST_CASE("theme rites: an empty-theme dungeon carries no rite", "[theme-events]") {
    const content::ContentDatabase db = loadContent();
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        const Dungeon d = generate(seed, 5, db, "", 1);
        CHECK(countAnyRite(d) == 0);
    }
}

TEST_CASE("theme rites: the relic never displaces the rite", "[theme-events]") {
    // At high town/depth the Royal Relic replacement is likely; the rite must
    // still be present exactly once (it owns the first slot, relic-proof).
    const content::ContentDatabase db = loadContent();
    for (std::uint64_t seed = 1; seed <= 120; ++seed) {
        const Dungeon d = generate(seed, 20, db, "ruined_keep", 7);
        INFO("seed=" << seed);
        CHECK(countRite(d, RoomEventKind::ArmoryGhost) == 1);
    }
}

TEST_CASE("theme rites: same seed reproduces the same placement and payloads",
          "[theme-events]") {
    const content::ContentDatabase db = loadContent();
    const Dungeon a = generate(4242, 8, db, "crystal_mine", 4);
    const Dungeon b = generate(4242, 8, db, "crystal_mine", 4);
    REQUIRE(a.rooms.size() == b.rooms.size());
    for (std::size_t i = 0; i < a.rooms.size(); ++i) {
        CHECK(a.rooms[i].event.kind == b.rooms[i].event.kind);
        CHECK(a.rooms[i].event.itemId == b.rooms[i].event.itemId);
        CHECK(a.rooms[i].event.goldCost == b.rooms[i].event.goldCost);
    }
    // The Miner's Cache carries a guaranteed item baked in at generation.
    bool sawCacheItem = false;
    for (const Room& r : a.rooms) {
        if (r.event.kind == RoomEventKind::MinersCache) {
            CHECK_FALSE(r.event.itemId.empty());
            sawCacheItem = true;
        }
    }
    CHECK(sawCacheItem);
    // The Elder Root carries a town-scaled price.
    const Dungeon forest = generate(7, 6, db, "hollow_forest", 3);
    for (const Room& r : forest.rooms) {
        if (r.event.kind == RoomEventKind::ElderRoot) {
            CHECK(r.event.goldCost == elderRootPrice(3, 6));
        }
    }
}

TEST_CASE("theme rites: the Armory Ghost upgrades same-slot, next-rarity, refuses legendary",
          "[theme-events]") {
    const content::ContentDatabase db = loadContent();
    // A common weapon -> an uncommon weapon, whatever the hash.
    for (std::uint64_t h = 0; h < 8; ++h) {
        const std::string up = armoryGhostUpgrade(db, "iron_sword", h);  // common weapon
        REQUIRE_FALSE(up.empty());
        const content::ItemDef* it = db.findItem(up);
        REQUIRE(it != nullptr);
        CHECK(it->slot == content::EquipSlot::Weapon);
        CHECK(it->rarity == content::Rarity::Uncommon);
    }
    // An epic can yield a legendary (a designed third legendary source).
    {
        const std::string up = armoryGhostUpgrade(db, "sunforged_greatblade", 3);  // epic weapon
        REQUIRE_FALSE(up.empty());
        const content::ItemDef* it = db.findItem(up);
        REQUIRE(it != nullptr);
        CHECK(it->slot == content::EquipSlot::Weapon);
        CHECK(it->rarity == content::Rarity::Legendary);
    }
    // Same-slot guarantee for armor and accessory inputs too.
    {
        const std::string up = armoryGhostUpgrade(db, "leather_armor", 1);  // common armor
        REQUIRE_FALSE(up.empty());
        CHECK(db.findItem(up)->slot == content::EquipSlot::Armor);
    }
    // Legendary input: the ghost declines.
    CHECK(armoryGhostUpgrade(db, "worldbreaker_axe", 0).empty());  // legendary weapon
    // A consumable is not a valid trade-in.
    CHECK(armoryGhostUpgrade(db, "potion", 0).empty());
    // Unknown id.
    CHECK(armoryGhostUpgrade(db, "no_such_item", 0).empty());
    // Deterministic.
    CHECK(armoryGhostUpgrade(db, "iron_sword", 5) == armoryGhostUpgrade(db, "iron_sword", 5));
}

TEST_CASE("theme rites: Elder Root price is affordable and its XP is elite-battle sized",
          "[theme-events]") {
    const content::ContentDatabase db = loadContent();
    CHECK(elderRootPrice(1, 1) > 0);
    CHECK(elderRootPrice(1, 1) < elderRootPrice(7, 20));  // rises with town + depth
    // A reference "one elite battle": the party earns each enemy's flat xpReward,
    // so a 2-elite team using the strongest elite is a generous upper bound.
    int topEnemyXp = 0;
    for (const auto& [id, def] : db.enemies()) {
        (void)id;
        if (def.xpReward > topEnemyXp) {
            topEnemyXp = def.xpReward;
        }
    }
    REQUIRE(topEnemyXp > 0);
    for (int t = 1; t <= 7; ++t) {
        for (int d = 1; d <= 20; d += 4) {
            const int xp = elderRootXp(t, d);
            INFO("town=" << t << " depth=" << d << " xp=" << xp << " topEnemyXp=" << topEnemyXp);
            CHECK(xp >= topEnemyXp / 2);   // at least half a single elite
            CHECK(xp <= topEnemyXp * 4);   // never a farm (a couple elite battles at most)
        }
    }
}

#endif  // CRYSTAL_TEST_DATA_DIR
