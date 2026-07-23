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
