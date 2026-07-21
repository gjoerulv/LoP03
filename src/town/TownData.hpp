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

// A road out of town (M32): stand on the tile and Confirm to travel. The
// bottom-left exit leads to the previous town, the bottom-right to the next.
struct TownExit {
    int tileX = 0;
    int tileY = 0;
    int destTown = 1;     // town index to travel to
    bool toNext = false;  // true = next/east (higher), false = previous/west
    bool locked = false;  // next-town exit shown but not yet unlocked
};

struct TownLayout {
    Tilemap map;
    Vec2 spawnPixel;
    std::vector<Building> buildings;
    std::vector<TownExit> exits;
};

// Builds the single-screen town (26x15 tiles, fits 426x240). `town` labels the
// exits; a previous-town exit appears when `hasPrev`, a next-town exit when
// `hasNext` (rendered locked until `nextUnlocked`). The default (town 1, no
// exits) reproduces the pre-M32 single-town layout byte-for-byte.
TownLayout buildTown(int town = 1, bool hasPrev = false, bool hasNext = false,
                     bool nextUnlocked = false);

}  // namespace cd::town
