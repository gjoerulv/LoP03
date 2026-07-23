#include "town/TownData.hpp"

#include <utility>

namespace cd::town {

const char* locationName(LocationId id) {
    switch (id) {
        case LocationId::Inn: return "Inn";
        case LocationId::ItemShop: return "Item Shop";
        case LocationId::EquipShop: return "Equip Shop";
        case LocationId::Guild: return "Guild";
        case LocationId::TrainingHall: return "Training Hall";
        case LocationId::Scoreboard: return "Scoreboard";
        case LocationId::SavePoint: return "Save Point";
    }
    return "?";
}

TownLayout buildTown(int town, bool hasPrev, bool hasNext, bool nextUnlocked, bool hasCastle,
                     bool castleUnlocked) {
    // M50: compact, centred town (24x12 = 384x192). Rendered inside the M46 stage
    // matte, with walk-through road triggers on the edges instead of Confirm-doors
    // on the bottom border.
    Tilemap map(kTownWidth, kTownHeight, Tile::Ground);

    // Closed border of trees; the road gaps below are carved back into it.
    for (int x = 0; x < kTownWidth; ++x) {
        map.set(x, 0, Tile::Tree);
        map.set(x, kTownHeight - 1, Tile::Tree);
    }
    for (int y = 0; y < kTownHeight; ++y) {
        map.set(0, y, Tile::Tree);
        map.set(kTownWidth - 1, y, Tile::Tree);
    }

    std::vector<Building> buildings;
    auto place = [&](LocationId id, const char* name, int bx, int by, int bw, int bh,
                     bool doorBelow, int doorOffset) {
        for (int yy = 0; yy < bh; ++yy) {
            for (int xx = 0; xx < bw; ++xx) {
                map.set(bx + xx, by + yy, Tile::Building);
            }
        }
        const int doorX = bx + doorOffset;
        const int doorY = doorBelow ? (by + bh) : (by - 1);
        map.set(doorX, doorY, Tile::Door);
        buildings.push_back({id, name, doorX, doorY, bx, by, bw, bh});
    };

    // Top row (bodies y1-2, doors face down at y3): five services across the
    // interior with the castle-road gap left open at x13.
    place(LocationId::Inn, "Inn", 2, 1, 3, 2, true, 1);
    place(LocationId::ItemShop, "Item Shop", 6, 1, 3, 2, true, 1);
    place(LocationId::EquipShop, "Equip Shop", 10, 1, 3, 2, true, 1);
    place(LocationId::Guild, "Guild", 14, 1, 3, 2, true, 1);
    place(LocationId::TrainingHall, "Training Hall", 18, 1, 3, 2, true, 1);

    // Bottom row (bodies y8-9, doors face up at y7).
    place(LocationId::Scoreboard, "Scoreboard", 6, 8, 3, 2, false, 1);
    place(LocationId::SavePoint, "Save Point", 15, 8, 3, 2, false, 1);

    // Walk-through edge triggers (M50). Each carves a road gap in the border at a
    // mid row (sides) or a top gap (castle) plus one road tile inside, so the gap
    // reads as a way out of town. The trigger tile is the border cell itself:
    // walk onto it (no Confirm) to travel. Placed clear of the M46 footer strip.
    std::vector<TownExit> exits;
    if (hasPrev) {
        map.set(0, kExitRow, Tile::Path);  // west gate
        map.set(1, kExitRow, Tile::Path);  // road inside
        exits.push_back({0, kExitRow, town - 1, false, false});
    }
    if (hasNext) {
        map.set(kTownWidth - 1, kExitRow, Tile::Path);  // east gate
        map.set(kTownWidth - 2, kExitRow, Tile::Path);
        exits.push_back({kTownWidth - 1, kExitRow, town + 1, true, !nextUnlocked});
    }
    // M40 castle road: leaves town 7 to the NORTH through the x13 gap, distinct
    // from the side roads. Shown locked until the town-7 clear opens it.
    if (hasCastle) {
        map.set(kCastleCol, 0, Tile::Path);
        map.set(kCastleCol, 1, Tile::Path);
        exits.push_back({kCastleCol, 0, 0, true, !castleUnlocked, true});
    }

    const Vec2 spawn{static_cast<float>(kSpawnTileX) * Tilemap::kTileSize,
                     static_cast<float>(kSpawnTileY) * Tilemap::kTileSize};
    return TownLayout{std::move(map), spawn, std::move(buildings), std::move(exits)};
}

Vec2 townEntrySpawnPixel(TownEntry entry) {
    // Where the player lands on arrival: one tile INSIDE the matching edge
    // trigger (never on it), so a walk-through exit cannot re-fire on spawn. The
    // default is the plaza spawn (new game, load, leaving a building/dungeon).
    int tx = kSpawnTileX;
    int ty = kSpawnTileY;
    switch (entry) {
        case TownEntry::FromWest:  // arrived travelling east -> land at the WEST road
            tx = 1;
            ty = kExitRow;
            break;
        case TownEntry::FromEast:  // arrived travelling west -> land at the EAST road
            tx = kTownWidth - 2;
            ty = kExitRow;
            break;
        case TownEntry::FromNorth:  // down from the castle -> land under the north gap
            tx = kCastleCol;
            ty = 1;
            break;
        case TownEntry::Default:
            break;
    }
    return Vec2{static_cast<float>(tx) * Tilemap::kTileSize,
               static_cast<float>(ty) * Tilemap::kTileSize};
}

}  // namespace cd::town
