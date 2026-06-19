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

TownLayout buildTown() {
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

    const Vec2 spawn{12.0f * Tilemap::kTileSize, 8.0f * Tilemap::kTileSize};
    return TownLayout{std::move(map), spawn, std::move(buildings)};
}

}  // namespace cd::town
