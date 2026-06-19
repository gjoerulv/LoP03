#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("town: layout is well-formed and all doors are reachable tiles", "[town]") {
    const TownLayout t = buildTown();
    REQUIRE(t.map.width() == 26);
    REQUIRE(t.map.height() == 15);
    REQUIRE(t.buildings.size() == 7);

    const int sx = static_cast<int>(t.spawnPixel.x) / Tilemap::kTileSize;
    const int sy = static_cast<int>(t.spawnPixel.y) / Tilemap::kTileSize;
    REQUIRE_FALSE(t.map.solidAt(sx, sy));  // spawn is walkable

    for (const Building& b : t.buildings) {
        REQUIRE(t.map.inBounds(b.doorX, b.doorY));
        REQUIRE(t.map.at(b.doorX, b.doorY) == Tile::Door);
        REQUIRE_FALSE(t.map.solidAt(b.doorX, b.doorY));
    }
}
