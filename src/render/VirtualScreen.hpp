#pragma once

#include "raylib.h"
#include "render/RaylibRAII.hpp"

// Owns the offscreen render target the game draws into at the fixed internal
// resolution, then blits it to the window with aspect-preserving scaling.

namespace cd {

class VirtualScreen {
public:
    VirtualScreen(int width, int height);

    // Begin/end drawing into the offscreen target (logical coordinates).
    void beginDraw(Color clear) const;
    void endDraw() const;

    // Blit the target to the current window framebuffer, centered and scaled.
    // Call between BeginDrawing()/EndDrawing(). Fills letterbox bars with `bars`.
    void blitToWindow(int windowWidth, int windowHeight, Color bars) const;

    int width() const { return width_; }
    int height() const { return height_; }

    // Writes the current target as a PNG at native resolution (already
    // y-corrected). Used by the M23 capture tooling; safe anywhere.
    bool exportImage(const char* path) const;

private:
    int width_;
    int height_;
    RenderTextureHandle target_;
};

}  // namespace cd
