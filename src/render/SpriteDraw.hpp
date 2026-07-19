#pragma once

#include <string>

#include "raylib.h"

// Anchored world-sprite drawing (M17). Sprites draw centered on the
// collision-rect center, so art dimensions can change freely without
// touching gameplay collision. Callers try the animation first, then the
// static texture, then their own shape fallback.

namespace cd {
class ResourceManager;
}

namespace cd::render {

// Draws the current frame of a catalog animation centered on (cx, cy).
// Returns false when the animation or its strip texture is unavailable.
bool drawAnimationCentered(ResourceManager& resources, const std::string& animId, float t,
                           float cx, float cy, Color tint = WHITE);

// Same anchoring for a static texture id.
bool drawTextureCentered(ResourceManager& resources, const std::string& textureId, float cx,
                         float cy, Color tint = WHITE);

// Dominant-axis facing from a movement/facing vector (ties resolve
// vertically, matching the default south-facing spawn).
enum class Facing { Down, Up, Left, Right };
inline Facing facingFrom(float fx, float fy) {
    if (fx * fx > fy * fy) {
        return fx < 0.0f ? Facing::Left : Facing::Right;
    }
    return fy < 0.0f ? Facing::Up : Facing::Down;
}

}  // namespace cd::render
