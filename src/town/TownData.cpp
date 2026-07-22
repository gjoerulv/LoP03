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
    constexpr int kW = 26;
    constexpr int kH = 15;
    Tilemap map(kW, kH, Tile::Ground);

    // Closed border of trees.
    for (int x = 0; x < kW; ++x) {
        map.set(x, 0, Tile::Tree);
        map.set(x, kH - 1, Tile::Tree);
    }
    for (int y = 0; y < kH; ++y) {
        map.set(0, y, Tile::Tree);
        map.set(kW - 1, y, Tile::Tree);
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

    // Top row of buildings; doors face down into the plaza.
    place(LocationId::Inn, "Inn", 2, 2, 3, 2, true, 1);
    place(LocationId::ItemShop, "Item Shop", 7, 2, 3, 2, true, 1);
    place(LocationId::EquipShop, "Equip Shop", 12, 2, 3, 2, true, 1);
    place(LocationId::Guild, "Guild", 17, 2, 3, 2, true, 1);
    place(LocationId::TrainingHall, "Training Hall", 21, 2, 3, 2, true, 1);

    // Bottom buildings; doors face up.
    place(LocationId::Scoreboard, "Scoreboard", 7, 11, 3, 2, false, 1);
    place(LocationId::SavePoint, "Save Point", 16, 11, 3, 2, false, 1);

    // Town-ladder exits (M32): a road down to a gap in the tree border. The exit
    // trigger tile sits on the border row; the tile above is a short path so the
    // gap reads as a road leaving town. Only added when a neighbour town exists.
    std::vector<TownExit> exits;
    auto carveExit = [&](int col, int destTown, bool toNext, bool locked) {
        map.set(col, kH - 2, Tile::Path);   // road
        map.set(col, kH - 1, Tile::Door);   // gate in the border
        exits.push_back({col, kH - 1, destTown, toNext, locked});
    };
    if (hasPrev) {
        carveExit(2, town - 1, false, false);
    }
    if (hasNext) {
        carveExit(kW - 3, town + 1, true, !nextUnlocked);
    }
    // M40: the castle road leaves town 7 to the NORTH (top edge), distinct from
    // the side roads, signalling it climbs above the ladder. Always shown when
    // hasCastle, locked until the town-7 clear opens it.
    if (hasCastle) {
        const int col = kW / 2;
        map.set(col, 1, Tile::Path);
        map.set(col, 0, Tile::Door);
        TownExit e{col, 0, 0, true, !castleUnlocked, true};
        exits.push_back(e);
    }

    const Vec2 spawn{12.0f * Tilemap::kTileSize, 8.0f * Tilemap::kTileSize};
    return TownLayout{std::move(map), spawn, std::move(buildings), std::move(exits)};
}

}  // namespace cd::town
