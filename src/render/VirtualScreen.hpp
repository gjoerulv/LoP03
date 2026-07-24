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

    // M57: set the CRT post-process strength, 0.0..1.0 (clamped). 0 uses the
    // plain DrawTexturePro path (the exact unfiltered image); any value > 0
    // applies the advanced consumer-CRT shader around the window blit. The
    // shader is compiled lazily the first time intensity becomes > 0 (at most
    // once); a compile failure is logged once and degrades to the plain blit.
    // Capture is unaffected — exportImage reads the pre-shader target directly.
    void setCrtIntensity(float intensity);

    int width() const { return width_; }
    int height() const { return height_; }

    // Writes the current target as a PNG at native resolution (already
    // y-corrected). Used by the M23 capture tooling; safe anywhere.
    bool exportImage(const char* path) const;

private:
    int width_;
    int height_;
    RenderTextureHandle target_;
    // M57 CRT shader state. setCrtIntensity is non-const and called before the
    // (const) blit each frame, so the lazy compile happens there.
    ShaderHandle crtShader_;
    float crtIntensity_ = 0.0f;  // the setting, clamped 0..1
    bool crtReady_ = false;      // compiled OK
    bool crtTried_ = false;      // compile attempted (so a failure logs once)
    // Cached uniform locations (resolved once on a successful compile).
    int crtLocIntensity_ = -1;
    int crtLocSourceRes_ = -1;
    int crtLocOutputRes_ = -1;
    int crtLocSourceTexel_ = -1;
    int crtLocTime_ = -1;
};

}  // namespace cd
