#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/RoomLayout.hpp"
#include "game/Achievements.hpp"
#include "game/Curios.hpp"
#include "game/Party.hpp"
#include "save/SaveSystem.hpp"

// M66 — the single-use dungeon treasure map (generation v13) + the twelve
// curios: chart/buried seeding, the curio table and its seeded no-repeat
// draw, the Curator achievement, and the save round-trip.

using namespace cd;

namespace {
content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}
}  // namespace

TEST_CASE("curios: the chart and the buried spot seed deterministically",
          "[curio][generation]") {
    CHECK(dungeon::kGenerationVersion >= 13);  // the exact pin rides the newest bump (M68)
    const content::ContentDatabase db = loadContent();
    int withChart = 0;
    const int samples = 200;
    for (int i = 0; i < samples; ++i) {
        const std::uint64_t seed = 0x66660000ull + static_cast<std::uint64_t>(i) * 1013;
        const dungeon::Dungeon a = dungeon::generate(seed, 5, db, "crystal_mine", 3);
        const dungeon::Dungeon b = dungeon::generate(seed, 5, db, "crystal_mine", 3);
        CHECK(a.chartRoom == b.chartRoom);
        CHECK(a.buriedRoom == b.buriedRoom);
        // The pair stands or falls together, in distinct plain rooms, never
        // sharing the map-piece room's center tile.
        CHECK((a.chartRoom >= 0) == (a.buriedRoom >= 0));
        if (a.chartRoom >= 0) {
            ++withChart;
            CHECK(a.chartRoom != a.buriedRoom);
            CHECK(a.chartRoom != a.mapPieceRoom);
            CHECK(a.buriedRoom != a.mapPieceRoom);
            CHECK(a.rooms[static_cast<std::size_t>(a.chartRoom)].type ==
                  dungeon::RoomType::Normal);
            CHECK(a.rooms[static_cast<std::size_t>(a.buriedRoom)].type ==
                  dungeon::RoomType::Normal);
        }
    }
    // ~12% authored chance, wide anti-flake band.
    CHECK(withChart >= samples * 5 / 100);
    CHECK(withChart <= samples * 22 / 100);
}

TEST_CASE("curios: twelve trinkets, four per theme, unique ids", "[curio]") {
    CHECK(kCurioCount == 12);
    std::set<std::string> ids;
    int perTheme[3] = {0, 0, 0};
    for (const CurioDef& c : kCurios) {
        CHECK(ids.insert(c.id).second);
        REQUIRE(findCurio(c.id) == &c);
        const std::string theme = c.themeId;
        if (theme == "ruined_keep") ++perTheme[0];
        if (theme == "crystal_mine") ++perTheme[1];
        if (theme == "hollow_forest") ++perTheme[2];
    }
    CHECK(perTheme[0] == 4);
    CHECK(perTheme[1] == 4);
    CHECK(perTheme[2] == 4);
}

TEST_CASE("curios: the draw prefers the theme, never repeats, then runs dry", "[curio]") {
    std::vector<std::string> owned;
    // The first four mine treasures pay the four mine curios (seeded order).
    for (int i = 0; i < 4; ++i) {
        const std::string id = pickCurio(owned, "crystal_mine", 1000 + i);
        REQUIRE_FALSE(id.empty());
        CHECK(std::string(findCurio(id)->themeId) == "crystal_mine");
        CHECK_FALSE(ownsCurio(owned, id));
        owned.push_back(id);
    }
    // A fifth mine treasure spills into the other themes' unowned curios.
    const std::string spill = pickCurio(owned, "crystal_mine", 77);
    REQUIRE_FALSE(spill.empty());
    CHECK(std::string(findCurio(spill)->themeId) != "crystal_mine");
    // Determinism: the same (owned, theme, seed) always draws the same curio.
    CHECK(pickCurio(owned, "crystal_mine", 77) == spill);
    // A complete dozen draws nothing (the token fallback).
    for (const CurioDef& c : kCurios) {
        if (!ownsCurio(owned, c.id)) {
            owned.push_back(c.id);
        }
    }
    CHECK(pickCurio(owned, "ruined_keep", 5).empty());
}

TEST_CASE("curios: the Curator achievement fires on the full dozen", "[curio]") {
    Party p;
    CHECK_FALSE(achievementMet("curator", p, AchvContext{}));
    for (const CurioDef& c : kCurios) {
        p.ownedCurios.push_back(c.id);
    }
    CHECK(achievementMet("curator", p, AchvContext{}));
    // The 18th roster entry exists and is the Curator.
    bool found = false;
    for (const AchievementDef& a : kAchievements) {
        if (std::string(a.id) == "curator") {
            found = true;
        }
    }
    CHECK(found);
    CHECK(kAchievementCount == 19);  // +1 M84 Guildbane
}

TEST_CASE("curios: owned curios round-trip the save; unknown ids drop", "[curio][save]") {
    const content::ContentDatabase db = loadContent();
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "crystal_curio_save_test";
    std::filesystem::remove_all(dir);
    save::SaveSystem saves(db, dir);

    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan", 5));
    p.ownedCurios = {"keep_banner", "not_a_curio", "mine_geode_heart", "keep_banner"};

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep));
    CHECK(loaded.ownedCurios == std::vector<std::string>{"keep_banner", "mine_geode_heart"});
    std::filesystem::remove_all(dir);
}
