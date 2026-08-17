#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/RoomLayout.hpp"
#include "game/Party.hpp"
#include "game/TreasureMap.hpp"
#include "save/SaveSystem.hpp"
#include "town/TownData.hpp"

// M65 — the town puzzle map (generation v12): the seeded Secret Map Piece,
// the reveal state, the Lost Scroll pool, the seeded guard, the dig tile's
// walkability, and the save round-trip with defensive drops.

using namespace cd;

namespace {
content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}
}  // namespace

TEST_CASE("treasure: map pieces seed deterministically into some dungeons",
          "[treasure][generation]") {
    CHECK(dungeon::kGenerationVersion >= 12);  // the exact pin rides the newest bump (M66)
    const content::ContentDatabase db = loadContent();
    int withPiece = 0;
    const int samples = 200;
    for (int i = 0; i < samples; ++i) {
        const std::uint64_t seed = 0xABCD0000ull + static_cast<std::uint64_t>(i) * 977;
        const dungeon::Dungeon a = dungeon::generate(seed, 4, db, "ruined_keep", 2);
        const dungeon::Dungeon b = dungeon::generate(seed, 4, db, "ruined_keep", 2);
        CHECK(a.mapPieceRoom == b.mapPieceRoom);  // reload-proof
        if (a.mapPieceRoom >= 0) {
            ++withPiece;
            REQUIRE(a.mapPieceRoom < static_cast<int>(a.rooms.size()));
            // Only a plain Normal room ever hides one.
            CHECK(a.rooms[static_cast<std::size_t>(a.mapPieceRoom)].type ==
                  dungeon::RoomType::Normal);
        }
    }
    // ~10% authored chance: allow a generous band so the pin never flakes.
    CHECK(withPiece >= samples * 4 / 100);
    CHECK(withPiece <= samples * 20 / 100);
}

TEST_CASE("treasure: the Lost Scroll pool draws in order and never repeats",
          "[treasure][content]") {
    const content::ContentDatabase db = loadContent();
    std::vector<std::string> awarded;
    for (const char* expected : treasureScrollPool()) {
        const std::string next = nextTreasureScroll(awarded);
        CHECK(next == expected);
        // Every pool entry is a real, TREASURE-ONLY scroll: value <= 0 keeps
        // it out of every shop, chest, and drop pool (the M44 rule).
        const content::ItemDef* item = db.findItem(next);
        REQUIRE(item != nullptr);
        CHECK(item->type == content::ItemType::Scroll);
        CHECK(db.hasSkill(item->grantsSkill));
        CHECK(item->value <= 0);
        awarded.push_back(next);
    }
    CHECK(nextTreasureScroll(awarded).empty());  // spent -> the token fallback
}

TEST_CASE("treasure: the guard is a seeded dungeon-roster boss", "[treasure]") {
    const content::ContentDatabase db = loadContent();
    const std::vector<std::string> roster = bossRushOrder(db);
    for (std::uint64_t seed : {1ull, 42ull, 0xFEEDull, 0xB16B00B5ull}) {
        const std::string id = treasureGuardBossId(db, seed, 7);
        CHECK(id == treasureGuardBossId(db, seed, 7));  // deterministic
        CHECK(std::find(roster.begin(), roster.end(), id) != roster.end());
        // M106: a low-town dig never meets a town-7-gated boss (the geese).
        const std::string lowTown = treasureGuardBossId(db, seed, 1);
        const content::BossDef* guard = db.findBoss(lowTown);
        REQUIRE(guard != nullptr);
        CHECK(guard->minTown < 7);
    }
}

TEST_CASE("treasure: the dig tile is walkable plaza ground", "[treasure][town]") {
    const town::TownLayout layout = town::buildTown();
    REQUIRE(kDigTileX >= 0);
    REQUIRE(kDigTileY >= 0);
    REQUIRE(kDigTileX < layout.map.width());
    REQUIRE(kDigTileY < layout.map.height());
    CHECK_FALSE(layout.map.solidAt(kDigTileX, kDigTileY));
    // Clear of every building door, so the dig prompt never shadows an entry.
    for (const town::Building& b : layout.buildings) {
        CHECK_FALSE((b.doorX == kDigTileX && b.doorY == kDigTileY));
    }
}

TEST_CASE("treasure: the reveal and pieces round-trip the save; ghosts drop",
          "[treasure][save]") {
    const content::ContentDatabase db = loadContent();
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "crystal_treasure_save_test";
    std::filesystem::remove_all(dir);
    save::SaveSystem saves(db, dir);

    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan", 5));
    p.mapPieces = 2;
    p.treasure.active = true;
    p.treasure.town = 5;
    p.treasure.bossId = bossRushOrder(db).front();
    p.treasure.scalePct = 240;
    p.treasureScrollsAwarded = {"treasure_scroll_meteor"};

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep));
    CHECK(loaded.mapPieces == 2);
    CHECK(loaded.treasure.active);
    CHECK(loaded.treasure.town == 5);
    CHECK(loaded.treasure.bossId == p.treasure.bossId);
    CHECK(loaded.treasure.scalePct == 240);
    CHECK(loaded.treasureScrollsAwarded == p.treasureScrollsAwarded);

    // A reveal whose guard the content no longer knows deactivates cleanly.
    p.treasure.bossId = "boss_of_nowhere";
    REQUIRE(saves.save(save::SaveSlot::Manual2, p, rep));
    Party ghost;
    REQUIRE(saves.load(save::SaveSlot::Manual2, ghost, rep));
    CHECK_FALSE(ghost.treasure.active);
    std::filesystem::remove_all(dir);
}
