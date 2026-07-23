#include "render/VirtualScreen.hpp"

#include <array>

#include "core/Log.hpp"
#include "render/Viewport.hpp"

namespace cd {

namespace {
// M51: a deliberately subtle CRT fragment shader (GLSL 330) — faint scanlines
// and a light vertical aperture mask, NO curvature, so the pixel art stays
// crisp. Embedded as a string (LoadShaderFromMemory), so there is no new asset
// type and no dependency. Uses raylib's default vertex shader.
constexpr const char* kCrtFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 crtResolution;
out vec4 finalColor;
void main() {
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    // One dark line per virtual pixel row: even rows full, odd rows dimmed ~12%.
    float row = fragTexCoord.y * crtResolution.y;
    float scan = mix(0.88, 1.0, step(0.5, fract(row * 0.5)));
    // Faint RGB-ish aperture mask across columns, ~6%.
    float col = fragTexCoord.x * crtResolution.x;
    float mask = 0.94 + 0.06 * abs(sin(col * 3.14159265));
    texel.rgb *= scan * mask;
    finalColor = texel;
}
)";
}  // namespace

VirtualScreen::VirtualScreen(int width, int height)
    : width_(width), height_(height), target_(LoadRenderTexture(width, height)) {
    // Nearest-neighbor sampling keeps the pixel-art crisp when scaled.
    SetTextureFilter(target_.get().texture, TEXTURE_FILTER_POINT);
}

void VirtualScreen::beginDraw(Color clear) const {
    BeginTextureMode(target_.get());
    ClearBackground(clear);
}

void VirtualScreen::endDraw() const { EndTextureMode(); }

void VirtualScreen::setCrt(bool enabled) {
    crtWanted_ = enabled;
    if (enabled && !crtTried_) {
        crtTried_ = true;  // compile at most once, whatever the outcome
        // On a compile failure raylib logs the GLSL error and returns the DEFAULT
        // shader (a plain passthrough), so a failed compile degrades to an
        // ordinary blit rather than crashing. Owning that id is safe:
        // UnloadShader no-ops on the default shader.
        const Shader s = LoadShaderFromMemory(nullptr, kCrtFragment);
        if (s.id > 0) {
            crtShader_ = ShaderHandle(s);
            crtResolutionLoc_ = GetShaderLocation(crtShader_.get(), "crtResolution");
            // The uniform resolves only when our shader really compiled; the
            // default fallback lacks it (loc < 0), which the blit guards against.
            crtReady_ = crtResolutionLoc_ >= 0;
            if (!crtReady_) {
                log::warn("CRT shader unavailable; using a plain blit.");
            }
        }
    }
}

void VirtualScreen::blitToWindow(int windowWidth, int windowHeight, Color bars) const {
    ClearBackground(bars);

    const ViewportRect vp = computeViewport(windowWidth, windowHeight, width_, height_);

    // Source rect has a negative height because render textures are y-flipped.
    const Rectangle src{0.0f, 0.0f, static_cast<float>(width_), -static_cast<float>(height_)};
    const Rectangle dst{vp.x, vp.y, vp.width, vp.height};

    const bool useCrt = crtWanted_ && crtReady_;
    if (useCrt) {
        if (crtResolutionLoc_ >= 0) {
            const std::array<float, 2> res{static_cast<float>(width_), static_cast<float>(height_)};
            SetShaderValue(crtShader_.get(), crtResolutionLoc_, res.data(), SHADER_UNIFORM_VEC2);
        }
        BeginShaderMode(crtShader_.get());
    }
    DrawTexturePro(target_.get().texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    if (useCrt) {
        EndShaderMode();
    }
}

bool VirtualScreen::exportImage(const char* path) const {
    if (!target_.valid()) {
        return false;
    }
    Image img = LoadImageFromTexture(target_.get().texture);
    ImageFlipVertical(&img);  // render textures are stored y-flipped
    const bool ok = ExportImage(img, path);
    UnloadImage(img);
    return ok;
}

}  // namespace cd
