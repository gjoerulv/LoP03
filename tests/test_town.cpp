#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

#include "core/Geometry.hpp"
#include "town/Movement.hpp"
#include "town/Tilemap.hpp"
#include "town/TownData.hpp"

using namespace cd;
using namespace cd::town;
using Catch::Approx;

TEST_CASE("tilemap: solidity, bounds, and closed border", "[town]") {
    Tilemap m(5, 5, Tile::Ground);
    REQUIRE_FALSE(m.solidAt(2, 2));
    m.set(2, 2, Tile::Building);
    REQUIRE(m.solidAt(2, 2));

    REQUIRE(m.solidAt(-1, 0));  // out of bounds reads solid
    REQUIRE(m.solidAt(5, 5));
    REQUIRE(m.at(99, 99) == Tile::Tree);

    REQUIRE_FALSE(isSolid(Tile::Door));  // doors are walkable interact tiles
}

TEST_CASE("tilemap: rectHitsSolid covers the overlapped tiles", "[town]") {
    Tilemap m(4, 4, Tile::Ground);
    m.set(1, 1, Tile::Building);
    REQUIRE(m.rectHitsSolid(Rect{16, 16, 8, 8}));      // inside tile (1,1)
    REQUIRE_FALSE(m.rectHitsSolid(Rect{0, 0, 8, 8}));  // tile (0,0) is ground
}

TEST_CASE("movement: blocked axis cancels, the other slides", "[town]") {
    Tilemap m(10, 10, Tile::Ground);
    m.set(5, 5, Tile::Building);  // pixels x[80,96), y[80,96)

    const Rect body{82.0f, 64.0f, 12.0f, 12.0f};  // directly above the building

    const Vec2 blocked = resolveMove(body, 0.0f, 20.0f, m);  // down into the wall
    REQUIRE(blocked.y == Approx(body.y));                    // cancelled

    const Vec2 free = resolveMove(body, 0.0f, -8.0f, m);  // up, into open ground
    REQUIRE(free.y == Approx(body.y - 8.0f));
}

namespace {
// Flood-fill the walkable tiles from a start cell (4-connected), returning the
// reachable set. Doors and road-trigger tiles are non-solid, so they are
// included — which is exactly what we want to prove is reachable.
std::vector<std::vector<bool>> reachableFrom(const Tilemap& map, int sx, int sy) {
    std::vector<std::vector<bool>> seen(map.width(), std::vector<bool>(map.height(), false));
    std::vector<std::pair<int, int>> stack{{sx, sy}};
    seen[sx][sy] = true;
    while (!stack.empty()) {
        const auto [x, y] = stack.back();
        stack.pop_back();
        for (const auto [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1},
                                    std::pair{0, -1}}) {
            const int nx = x + dx;
            const int ny = y + dy;
            if (map.inBounds(nx, ny) && !seen[nx][ny] && !map.solidAt(nx, ny)) {
                seen[nx][ny] = true;
                stack.push_back({nx, ny});
            }
        }
    }
    return seen;
}
}  // namespace

TEST_CASE("town: compact layout is well-formed and everything is reachable (M50)", "[town]") {
    const TownLayout t = buildTown();
    REQUIRE(t.map.width() == kTownWidth);    // 24
    REQUIRE(t.map.height() == kTownHeight);  // 12
    REQUIRE(t.buildings.size() == 7);

    const int sx = static_cast<int>(t.spawnPixel.x) / Tilemap::kTileSize;
    const int sy = static_cast<int>(t.spawnPixel.y) / Tilemap::kTileSize;
    REQUIRE_FALSE(t.map.solidAt(sx, sy));  // spawn is walkable

    const auto seen = reachableFrom(t.map, sx, sy);

    for (const Building& b : t.buildings) {
        REQUIRE(t.map.inBounds(b.doorX, b.doorY));
        REQUIRE(t.map.at(b.doorX, b.doorY) == Tile::Door);
        REQUIRE_FALSE(t.map.solidAt(b.doorX, b.doorY));
        REQUIRE(seen[b.doorX][b.doorY]);  // every building door reachable from spawn
    }
}

TEST_CASE("town: walk-through exit triggers are walkable, reachable, clear of the footer (M50)",
          "[town]") {
    // A town in the middle of the ladder has both side roads AND is asked for the
    // castle road, so all three trigger kinds are present to check.
    const TownLayout t = buildTown(/*town=*/7, /*hasPrev=*/true, /*hasNext=*/true,
                                   /*nextUnlocked=*/true, /*hasCastle=*/true,
                                   /*castleUnlocked=*/true);
    REQUIRE(t.exits.size() == 3);

    const int sx = static_cast<int>(t.spawnPixel.x) / Tilemap::kTileSize;
    const int sy = static_cast<int>(t.spawnPixel.y) / Tilemap::kTileSize;
    const auto seen = reachableFrom(t.map, sx, sy);

    bool west = false;
    bool east = false;
    bool castle = false;
    for (const TownExit& e : t.exits) {
        REQUIRE_FALSE(t.map.solidAt(e.tileX, e.tileY));  // the trigger tile is walkable
        REQUIRE(seen[e.tileX][e.tileY]);                 // and reachable from spawn
        if (e.toCastle) {
            castle = true;
            CHECK(e.tileY == 0);  // north edge
        } else if (e.toNext) {
            east = true;
            CHECK(e.tileX == kTownWidth - 1);  // east edge
            CHECK(e.tileY == kExitRow);        // mid-height, clear of the footer row
        } else {
            west = true;
            CHECK(e.tileX == 0);  // west edge
            CHECK(e.tileY == kExitRow);
        }
    }
    CHECK(west);
    CHECK(east);
    CHECK(castle);
}

TEST_CASE("town: entrance spawns land one tile inside the matching edge, never on it (M50)",
          "[town]") {
    const TownLayout t = buildTown(/*town=*/7, true, true, true, true, true);

    const auto tileOf = [](Vec2 p) {
        return std::pair{static_cast<int>(p.x) / Tilemap::kTileSize,
                         static_cast<int>(p.y) / Tilemap::kTileSize};
    };
    const auto isTrigger = [&](int x, int y) {
        for (const TownExit& e : t.exits) {
            if (e.tileX == x && e.tileY == y) {
                return true;
            }
        }
        return false;
    };

    for (TownEntry entry : {TownEntry::Default, TownEntry::FromWest, TownEntry::FromEast,
                            TownEntry::FromNorth}) {
        const auto [x, y] = tileOf(townEntrySpawnPixel(entry));
        INFO("entry " << static_cast<int>(entry));
        REQUIRE_FALSE(t.map.solidAt(x, y));  // always a walkable landing
        CHECK_FALSE(isTrigger(x, y));        // never ON a trigger (no arrive-bounce)
    }

    // Arriving from the west (travelled east) lands beside the WEST road; from the
    // east beside the EAST road.
    const auto [wx, wy] = tileOf(townEntrySpawnPixel(TownEntry::FromWest));
    CHECK(wx == 1);
    CHECK(wy == kExitRow);
    const auto [ex, ey] = tileOf(townEntrySpawnPixel(TownEntry::FromEast));
    CHECK(ex == kTownWidth - 2);
    CHECK(ey == kExitRow);
}
