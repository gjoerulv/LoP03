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

struct TownLayout {
    Tilemap map;
    Vec2 spawnPixel;
    std::vector<Building> buildings;
};

// Builds the fixed single-screen town (26x15 tiles, fits 426x240).
TownLayout buildTown();

}  // namespace cd::town
