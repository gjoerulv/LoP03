#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "dungeon/DungeonModel.hpp"
#include "dungeon/RoomLayout.hpp"

#ifdef CRYSTAL_TEST_DATA_DIR
#include <filesystem>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#endif

using namespace cd::dungeon;

namespace {

void link(Dungeon& d, int a, int b, Dir dir, bool gated = false, int teamIndex = -1) {
    d.rooms[static_cast<std::size_t>(a)].door(dir).neighbor = b;
    d.rooms[static_cast<std::size_t>(a)].door(dir).gated = gated;
    d.rooms[static_cast<std::size_t>(a)].door(dir).teamIndex = teamIndex;
    d.rooms[static_cast<std::size_t>(b)].door(opposite(dir)).neighbor = a;
    d.rooms[static_cast<std::size_t>(b)].door(opposite(dir)).gated = gated;
    d.rooms[static_cast<std::size_t>(b)].door(opposite(dir)).teamIndex = teamIndex;
}

// Hand-built dungeon exercising every archetype:
//   0 Entry - 1 GateChamber =gate= 2 GateChamber - 3 Corridor - 4 Crossroads
//   - 5 BossAntechamber - 6 BossArena; 7 TreasureAlcove south of 4;
//   8 TreasureVault south of 2.
Dungeon makeDungeon() {
    Dungeon d;
    d.seed = 777;
    d.depth = 1;
    d.gridW = 7;
    d.gridH = 7;
    d.rooms.resize(9);
    for (int i = 0; i <= 6; ++i) {
        d.rooms[static_cast<std::size_t>(i)].gridX = i;
        d.rooms[static_cast<std::size_t>(i)].gridY = 3;
    }
    d.rooms[7].gridX = 4;
    d.rooms[7].gridY = 4;
    d.rooms[8].gridX = 2;
    d.rooms[8].gridY = 4;

    d.rooms[0].type = RoomType::Start;
    d.rooms[6].type = RoomType::Boss;
    d.rooms[7].type = RoomType::Treasure;
    d.rooms[8].type = RoomType::Treasure;

    d.teams.resize(3);  // 0 = gate team, 1 = vault guard, 2 = boss
    d.teams[2].isBoss = true;

    for (int i = 0; i <= 5; ++i) {
        link(d, i, i + 1, Dir::East, /*gated=*/i == 1, /*teamIndex=*/i == 1 ? 0 : -1);
    }
    link(d, 4, 7, Dir::South);
    link(d, 2, 8, Dir::South);

    d.rooms[6].teamIndex = 2;
    d.rooms[7].chest.present = true;
    d.rooms[8].chest.present = true;
    d.rooms[8].chest.guarded = true;
    d.rooms[8].teamIndex = 1;

    d.startRoom = 0;
    d.bossRoom = 6;
    d.mainPath = {0, 1, 2, 3, 4, 5, 6};
    d.mandatoryGates = 1;
    return d;
}

bool sameLayout(const RoomLayout& a, const RoomLayout& b) {
    return a.width == b.width && a.height == b.height && a.archetype == b.archetype &&
           a.cells == b.cells && a.centerSpawn == b.centerSpawn && a.chest == b.chest &&
           a.guard == b.guard && a.boss == b.boss;
}

}  // namespace

TEST_CASE("room layout: classification covers all eight archetypes", "[roomlayout]") {
    const Dungeon d = makeDungeon();
    REQUIRE(classifyRoom(d, 0) == RoomArchetype::Entry);
    REQUIRE(classifyRoom(d, 1) == RoomArchetype::GateChamber);
    REQUIRE(classifyRoom(d, 2) == RoomArchetype::GateChamber);
    REQUIRE(classifyRoom(d, 3) == RoomArchetype::Corridor);
    REQUIRE(classifyRoom(d, 4) == RoomArchetype::Crossroads);
    REQUIRE(classifyRoom(d, 5) == RoomArchetype::BossAntechamber);
    REQUIRE(classifyRoom(d, 6) == RoomArchetype::BossArena);
    REQUIRE(classifyRoom(d, 7) == RoomArchetype::TreasureAlcove);
    REQUIRE(classifyRoom(d, 8) == RoomArchetype::TreasureVault);
}

TEST_CASE("room layout: derived seeds are stable and independent", "[roomlayout]") {
    const std::uint64_t base =
        roomLocalSeed(42, kGenerationVersion, 3, RoomArchetype::Corridor);
    REQUIRE(base == roomLocalSeed(42, kGenerationVersion, 3, RoomArchetype::Corridor));
    REQUIRE(base != roomLocalSeed(43, kGenerationVersion, 3, RoomArchetype::Corridor));
    REQUIRE(base != roomLocalSeed(42, kGenerationVersion + 1, 3, RoomArchetype::Corridor));
    REQUIRE(base != roomLocalSeed(42, kGenerationVersion, 4, RoomArchetype::Corridor));
    REQUIRE(base != roomLocalSeed(42, kGenerationVersion, 3, RoomArchetype::Crossroads));
}

TEST_CASE("room layout: realization is deterministic", "[roomlayout]") {
    const Dungeon d = makeDungeon();
    const std::vector<RoomLayout> a = realizeAllRooms(d);
    const std::vector<RoomLayout> b = realizeAllRooms(d);
    REQUIRE(a.size() == d.rooms.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        REQUIRE(sameLayout(a[i], b[i]));
    }
}

TEST_CASE("room layout: every hand-built room validates", "[roomlayout]") {
    const Dungeon d = makeDungeon();
    const std::vector<RoomLayout> layouts = realizeAllRooms(d);
    for (std::size_t i = 0; i < layouts.size(); ++i) {
        INFO("room " << i << " (" << archetypeName(layouts[i].archetype) << ")");
        const std::vector<std::string> problems =
            validateLayout(d, static_cast<int>(i), layouts[i]);
        for (const std::string& p : problems) {
            INFO(p);
        }
        REQUIRE(problems.empty());
    }
}

TEST_CASE("room layout: straight corridors run along their door axis", "[roomlayout]") {
    const Dungeon d = makeDungeon();
    const RoomLayout corridor = realizeRoom(d, 3);  // east-west pass-through
    REQUIRE(corridor.archetype == RoomArchetype::Corridor);
    REQUIRE(corridor.width >= 9);
    REQUIRE(corridor.width <= 13);
    REQUIRE(corridor.height >= 5);
    REQUIRE(corridor.height <= 7);
}

TEST_CASE("room layout: validator rejects a blocked door lane", "[roomlayout]") {
    const Dungeon d = makeDungeon();
    RoomLayout l = realizeRoom(d, 3);
    const RoomLayout::Point p = l.interiorGap(Dir::West)[0];
    l.cells[static_cast<std::size_t>(p.y * l.width + p.x)] = RoomLayout::Cell::Wall;
    REQUIRE_FALSE(validateLayout(d, 3, l).empty());
}

TEST_CASE("room layout: validator rejects a walled-off chest", "[roomlayout]") {
    const Dungeon d = makeDungeon();
    RoomLayout l = realizeRoom(d, 7);
    REQUIRE(l.chest.valid());
    for (const auto [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1},
                                std::pair{0, -1}}) {
        const int x = l.chest.x + dx;
        const int y = l.chest.y + dy;
        if (l.inBounds(x, y)) {
            l.cells[static_cast<std::size_t>(y * l.width + x)] = RoomLayout::Cell::Wall;
        }
    }
    REQUIRE_FALSE(validateLayout(d, 7, l).empty());
}

TEST_CASE("room layout: rooms are compact", "[roomlayout]") {
    const Dungeon d = makeDungeon();
    for (const RoomLayout& l : realizeAllRooms(d)) {
        REQUIRE(l.width <= kMaxRoomWidth);
        REQUIRE(l.height <= kMaxRoomHeight);
        REQUIRE(l.width >= kMinRoomSide);
        REQUIRE(l.height >= kMinRoomSide);
        // Strictly smaller than the pre-M16 fixed 26x15 full-screen room.
        REQUIRE(l.width < 26);
        REQUIRE(l.height < 15);
    }
}

#ifdef CRYSTAL_TEST_DATA_DIR

namespace {

cd::content::ContentDatabase loadContent() {
    cd::content::ContentDatabase db;
    cd::content::LoadReport rep;
    cd::content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

}  // namespace

TEST_CASE("room layout: mass generation validates across seeds, depths, and themes",
          "[roomlayout]") {
    const cd::content::ContentDatabase db = loadContent();
    const char* const themes[] = {"", "ruined_keep", "crystal_mine", "hollow_forest"};

    int roomsChecked = 0;
    std::set<RoomArchetype> seen;
    for (int i = 0; i < 40; ++i) {
        const std::uint64_t seed = static_cast<std::uint64_t>(i) * 7919u + 13u;
        for (int depth : {1, 4, 9}) {
            for (const char* theme : themes) {
                const Dungeon d = generate(seed, depth, db, theme);
                const std::vector<RoomLayout> layouts = realizeAllRooms(d);
                REQUIRE(layouts.size() == d.rooms.size());
                for (std::size_t r = 0; r < layouts.size(); ++r) {
                    INFO("seed " << seed << " depth " << depth << " theme '" << theme
                                 << "' room " << r << " ("
                                 << archetypeName(layouts[r].archetype) << ")");
                    const std::vector<std::string> problems =
                        validateLayout(d, static_cast<int>(r), layouts[r]);
                    for (const std::string& p : problems) {
                        INFO(p);
                    }
                    REQUIRE(problems.empty());
                    seen.insert(layouts[r].archetype);
                    ++roomsChecked;
                }
            }
        }
    }
    REQUIRE(roomsChecked > 2000);          // "thousands of rooms" acceptance bar
    REQUIRE(seen.size() >= 5);             // at least five archetypes appear
    REQUIRE(seen.count(RoomArchetype::Entry) == 1);
    REQUIRE(seen.count(RoomArchetype::BossArena) == 1);
}

TEST_CASE("room layout: same seed and version give identical layouts", "[roomlayout]") {
    const cd::content::ContentDatabase db = loadContent();
    const Dungeon d1 = generate(424242, 3, db, "ruined_keep");
    const Dungeon d2 = generate(424242, 3, db, "ruined_keep");
    const std::vector<RoomLayout> a = realizeAllRooms(d1);
    const std::vector<RoomLayout> b = realizeAllRooms(d2);
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        REQUIRE(sameLayout(a[i], b[i]));
    }
}

#endif  // CRYSTAL_TEST_DATA_DIR
