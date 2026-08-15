#pragma once

#include <string>
#include <vector>

#include "core/Geometry.hpp"
#include "town/Tilemap.hpp"

namespace cd::town {

enum class LocationId { Inn, ItemShop, EquipShop, Guild, TrainingHall, Scoreboard, SavePoint };

const char* locationName(LocationId id);

struct Building {
    LocationId id;
    std::string name;
    int doorX = 0;  // tile coords of the interact (door) tile
    int doorY = 0;
    int x = 0;  // building body origin/size in tile coords (for rendering/labels)
    int y = 0;
    int w = 0;
    int h = 0;
};

// Compact centred town dimensions (M50): 24x12 tiles = 384x192, drawn inside the
// M46 stage matte. The interior is 22x10; the layout constants below are the
// single authority the layout, the bard/market spots, and the tests all read.
inline constexpr int kTownWidth = 24;
inline constexpr int kTownHeight = 12;
inline constexpr int kExitRow = 6;    // mid-height row for the west/east road gaps
inline constexpr int kCastleCol = 13; // north gap column (between Equip Shop and Guild)
inline constexpr int kSpawnTileX = 11;
inline constexpr int kSpawnTileY = 5;

// M41: the wandering storyteller's fixed open-plaza tile in every town (Ground;
// clear of buildings, doors, road exits, the spawn, and every black-market
// tile). Lived in TownState.cpp until M88 — this header is the layout
// authority the bard, the market tiles, and the tests all read. M88 moved the
// bard two tiles east (owner request) so the west plaza breathes; the
// black-market tile list gave up the old neighbour tile in the same change.
inline constexpr int kBardTileX = 5;
inline constexpr int kBardTileY = 5;

// M97: where the hooded stranger stands after the King falls — town 7 only,
// one tile inside the eastern border, on the exit row: exactly where the road
// to a Town 8 would begin if there were one. Town 7 has no east gate (it is
// the ladder's last rung), so the tile is plain walkable Ground and clashes
// with no exit, door, market tile, bard tile, spawn, or dig spot.
inline constexpr int kGooseNpcTileX = kTownWidth - 2;
inline constexpr int kGooseNpcTileY = kExitRow;

// M88: the Scoreboard and the Save Point answer from EVERY tile around their
// body, not just the marked doorstep — they are freestanding monuments (M69),
// so "walk up to it from any side and press Confirm" is the natural read.
// Buildings with facades keep their single door.
inline bool monumentInteractsFromAllSides(LocationId id) {
    return id == LocationId::Scoreboard || id == LocationId::SavePoint;
}

// True when (tx,ty) touches the building body orthogonally (never diagonally,
// never inside the body). The doorstep tile itself also satisfies this for the
// monuments (their door sits on the body's rim), so one predicate covers both.
inline bool tileAdjacentToBuilding(const Building& b, int tx, int ty) {
    const bool inCols = tx >= b.x && tx < b.x + b.w;
    const bool inRows = ty >= b.y && ty < b.y + b.h;
    if (inCols && inRows) {
        return false;  // inside the body (unreachable anyway — solid tiles)
    }
    if (inCols && (ty == b.y - 1 || ty == b.y + b.h)) {
        return true;
    }
    return inRows && (tx == b.x - 1 || tx == b.x + b.w);
}

// A road out of town. Since M50 these are WALK-THROUGH triggers on the town edge
// (no Confirm): the west gate leads to the previous town, the east to the next,
// the north gap (town 7) up to the castle.
struct TownExit {
    int tileX = 0;
    int tileY = 0;
    int destTown = 1;      // town index to travel to (ignored when toCastle)
    bool toNext = false;   // true = next/east (higher), false = previous/west
    bool locked = false;   // exit shown but not yet unlocked
    bool toCastle = false; // M40: the road from town 7 up to the castle (not a town)
};

// How the player arrived, deciding where they spawn (M50). Transient — never
// persisted. Travelling east lands you at the WEST road of the destination, and
// vice-versa; a castle return lands under the north gap; everything else uses
// the plaza spawn.
enum class TownEntry { Default, FromWest, FromEast, FromNorth };

struct TownLayout {
    Tilemap map;
    Vec2 spawnPixel;
    std::vector<Building> buildings;
    std::vector<TownExit> exits;
};

// Builds the compact centred town (24x12 tiles, M50). `town` labels the exits; a
// previous-town road appears when `hasPrev`, a next-town road when `hasNext`
// (rendered locked until `nextUnlocked`). `hasCastle` adds the northern road to
// the castle (town 7 only), shown locked until `castleUnlocked` (M40). The edges
// are walk-through triggers, not Confirm-doors.
TownLayout buildTown(int town = 1, bool hasPrev = false, bool hasNext = false,
                     bool nextUnlocked = false, bool hasCastle = false,
                     bool castleUnlocked = false);

// The arrival spawn pixel for a given entrance (M50), one tile inside the
// matching edge trigger so a walk-through exit never re-fires on spawn.
Vec2 townEntrySpawnPixel(TownEntry entry);

}  // namespace cd::town
