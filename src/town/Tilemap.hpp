#pragma once

#include <vector>

#include "core/Geometry.hpp"

// A small fixed-size grid of tiles for the town (and, later, dungeon rooms).
// Pure: collision queries are unit-tested without raylib.

namespace cd::town {

enum class Tile { Ground, Path, Grass, Tree, Water, Building, Door };

bool isSolid(Tile tile);

class Tilemap {
public:
    static constexpr int kTileSize = 16;

    Tilemap(int width, int height, Tile fill = Tile::Ground);

    int width() const { return width_; }
    int height() const { return height_; }
    bool inBounds(int x, int y) const;

    Tile at(int x, int y) const;  // out-of-bounds reads as a solid Tree
    void set(int x, int y, Tile tile);

    bool solidAt(int x, int y) const;  // true out-of-bounds, so the map is closed
    bool rectHitsSolid(const Rect& pixelRect) const;

private:
    int width_;
    int height_;
    std::vector<Tile> tiles_;
};

}  // namespace cd::town
