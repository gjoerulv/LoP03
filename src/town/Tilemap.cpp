#include "town/Tilemap.hpp"

#include <cmath>

namespace cd::town {

bool isSolid(Tile tile) {
    switch (tile) {
        case Tile::Tree:
        case Tile::Water:
        case Tile::Building:
            return true;
        case Tile::Ground:
        case Tile::Path:
        case Tile::Grass:
        case Tile::Door:
            return false;
    }
    return true;
}

Tilemap::Tilemap(int width, int height, Tile fill)
    : width_(width), height_(height),
      tiles_(static_cast<std::size_t>(width * height), fill) {}

bool Tilemap::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

Tile Tilemap::at(int x, int y) const {
    if (!inBounds(x, y)) {
        return Tile::Tree;  // closed border
    }
    return tiles_[static_cast<std::size_t>(y * width_ + x)];
}

void Tilemap::set(int x, int y, Tile tile) {
    if (inBounds(x, y)) {
        tiles_[static_cast<std::size_t>(y * width_ + x)] = tile;
    }
}

bool Tilemap::solidAt(int x, int y) const { return isSolid(at(x, y)); }

bool Tilemap::rectHitsSolid(const Rect& r) const {
    constexpr float ts = static_cast<float>(kTileSize);
    const int minX = static_cast<int>(std::floor(r.x / ts));
    const int minY = static_cast<int>(std::floor(r.y / ts));
    const int maxX = static_cast<int>(std::floor((r.x + r.w - 0.001f) / ts));
    const int maxY = static_cast<int>(std::floor((r.y + r.h - 0.001f) / ts));
    for (int ty = minY; ty <= maxY; ++ty) {
        for (int tx = minX; tx <= maxX; ++tx) {
            if (solidAt(tx, ty)) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace cd::town
