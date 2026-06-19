#pragma once

#include "core/Geometry.hpp"
#include "town/Tilemap.hpp"

namespace cd::town {

// Axis-separated collision resolution: returns the new top-left position for
// `body` after attempting to move by (dx, dy). Blocked axes are cancelled
// independently, which gives natural wall-sliding. Pure and unit-tested.
Vec2 resolveMove(const Rect& body, float dx, float dy, const Tilemap& map);

}  // namespace cd::town
