#pragma once

#include "assets/AssetManifest.hpp"

// Pure sprite-strip animation timing (M17, manifest v2). Frame selection is
// a function of elapsed time so it is headless-testable and allocation-free;
// actual drawing lives with the renderer. Animations describe horizontal
// frame strips inside a catalog texture (assets::AssetEntry, type
// Animation).

namespace cd::render {

// Frame index at time t (seconds since the animation started). Looping
// animations wrap; non-looping animations hold their last frame. t <= 0 or
// degenerate metadata yield frame 0.
inline int frameAt(const assets::AssetEntry& anim, float t) {
    if (anim.frameCount <= 1 || !(anim.frameTime > 0.0f) || t <= 0.0f) {
        return 0;
    }
    const int raw = static_cast<int>(t / anim.frameTime);
    if (anim.loop) {
        return raw % anim.frameCount;
    }
    return raw >= anim.frameCount ? anim.frameCount - 1 : raw;
}

struct FrameRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// Pixel rectangle of a frame inside the strip texture (frames left-to-right
// on the entry's row). Out-of-range frames clamp.
inline FrameRect frameRect(const assets::AssetEntry& anim, int frame) {
    const int last = anim.frameCount - 1;
    const int f = frame < 0 ? 0 : (frame > last ? last : frame);
    return {f * anim.frameWidth, anim.row * anim.frameHeight, anim.frameWidth,
            anim.frameHeight};
}

}  // namespace cd::render
