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

    // M51: enable/disable the CRT scanline shader applied around the window
    // blit. The shader is compiled lazily on first enable; a compile failure is
    // logged once and the flag stays effectively off (plain blit). Capture is
    // unaffected — exportImage reads the pre-shader target directly.
    void setCrt(bool enabled);

    int width() const { return width_; }
    int height() const { return height_; }

    // Writes the current target as a PNG at native resolution (already
    // y-corrected). Used by the M23 capture tooling; safe anywhere.
    bool exportImage(const char* path) const;

private:
    int width_;
    int height_;
    RenderTextureHandle target_;
    // M51 CRT shader state (mutable so setCrt can lazily compile from a const
    // blit path is avoided — setCrt is non-const and called before the blit).
    ShaderHandle crtShader_;
    bool crtWanted_ = false;   // the setting
    bool crtReady_ = false;    // compiled OK
    bool crtTried_ = false;    // compile attempted (so a failure logs once)
    int crtResolutionLoc_ = -1;
};

}  // namespace cd
