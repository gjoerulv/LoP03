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

    // M57/M70: set both CRT parameters for the frame in ONE call (so no frame
    // can observe a mismatched pair), each 0.0..1.0 (clamped). Intensity 0
    // uses the plain DrawTexturePro path (the exact unfiltered image) no
    // matter what curvature says; any intensity > 0 applies the consumer-CRT
    // shader around the window blit, with geometry (barrel warp, inset,
    // rounded corners, curved-edge mask) driven ONLY by curvature — curvature
    // 0 stays perfectly rectangular. The shader is compiled lazily the first
    // time intensity becomes > 0 (at most once); a compile failure is logged
    // once and degrades to the plain blit. Capture is unaffected —
    // exportImage reads the pre-shader target directly.
    void setCrt(float intensity, float curvature);

    int width() const { return width_; }
    int height() const { return height_; }

    // Writes the current target as a PNG at native resolution (already
    // y-corrected). Used by the M23 capture tooling; safe anywhere.
    bool exportImage(const char* path) const;

private:
    int width_;
    int height_;
    RenderTextureHandle target_;
    // M57 CRT shader state. setCrt is non-const and called before the
    // (const) blit each frame, so the lazy compile happens there.
    ShaderHandle crtShader_;
    float crtIntensity_ = 0.0f;  // the strength setting, clamped 0..1
    float crtCurvature_ = 0.0f;  // M70: the geometry setting, clamped 0..1
    bool crtReady_ = false;      // compiled OK
    bool crtTried_ = false;      // compile attempted (so a failure logs once)
    // Cached uniform locations (resolved once on a successful compile).
    int crtLocIntensity_ = -1;
    int crtLocCurvature_ = -1;  // M70
    int crtLocSourceRes_ = -1;
    int crtLocOutputRes_ = -1;
    int crtLocSourceTexel_ = -1;
    int crtLocTime_ = -1;
};

}  // namespace cd
