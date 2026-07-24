#include "render/VirtualScreen.hpp"

#include <algorithm>
#include <array>

#include "core/Log.hpp"
#include "render/CrtShaderSource.hpp"
#include "render/Viewport.hpp"

namespace cd {

VirtualScreen::VirtualScreen(int width, int height)
    : width_(width), height_(height), target_(LoadRenderTexture(width, height)) {
    // Nearest-neighbor sampling keeps the pixel-art crisp when scaled.
    SetTextureFilter(target_.get().texture, TEXTURE_FILTER_POINT);
    // Clamp wrapping so the CRT shader's curvature/beam/chroma taps near the
    // image edge cannot fetch wrapped texels from the opposite side.
    SetTextureWrap(target_.get().texture, TEXTURE_WRAP_CLAMP);
}

void VirtualScreen::beginDraw(Color clear) const {
    BeginTextureMode(target_.get());
    ClearBackground(clear);
}

void VirtualScreen::endDraw() const { EndTextureMode(); }

void VirtualScreen::setCrtIntensity(float intensity) {
    crtIntensity_ = std::clamp(intensity, 0.0f, 1.0f);
    // Compile lazily the first time strength becomes > 0 (at most once). While
    // strength stays 0 the shader is never compiled, so 0 costs only this float
    // comparison beyond the plain blit.
    if (crtIntensity_ > 0.0f && !crtTried_) {
        crtTried_ = true;  // compile at most once, whatever the outcome
        // On a compile failure raylib logs the GLSL error and returns the DEFAULT
        // shader (a plain passthrough), so a failed compile degrades to an
        // ordinary blit rather than crashing. Owning that id is safe:
        // UnloadShader no-ops on the default shader.
        const Shader s = LoadShaderFromMemory(nullptr, kCrtFragmentSource);
        if (s.id > 0) {
            crtShader_ = ShaderHandle(s);
            crtLocIntensity_ = GetShaderLocation(crtShader_.get(), "crtIntensity");
            crtLocSourceRes_ = GetShaderLocation(crtShader_.get(), "crtSourceRes");
            crtLocOutputRes_ = GetShaderLocation(crtShader_.get(), "crtOutputRes");
            crtLocSourceTexel_ = GetShaderLocation(crtShader_.get(), "crtSourceTexel");
            crtLocTime_ = GetShaderLocation(crtShader_.get(), "crtTime");
            // Our uniforms resolve only when the shader really compiled; the
            // default fallback lacks them (loc < 0), which the blit guards against.
            crtReady_ = crtLocIntensity_ >= 0;
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

    const bool useCrt = crtIntensity_ > 0.0f && crtReady_;
    if (useCrt) {
        const std::array<float, 2> sourceRes{static_cast<float>(width_),
                                             static_cast<float>(height_)};
        const std::array<float, 2> outputRes{vp.width, vp.height};
        const std::array<float, 2> sourceTexel{1.0f / static_cast<float>(width_),
                                               1.0f / static_cast<float>(height_)};
        const float time = static_cast<float>(GetTime());
        const Shader& sh = crtShader_.get();
        SetShaderValue(sh, crtLocIntensity_, &crtIntensity_, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, crtLocSourceRes_, sourceRes.data(), SHADER_UNIFORM_VEC2);
        SetShaderValue(sh, crtLocOutputRes_, outputRes.data(), SHADER_UNIFORM_VEC2);
        SetShaderValue(sh, crtLocSourceTexel_, sourceTexel.data(), SHADER_UNIFORM_VEC2);
        SetShaderValue(sh, crtLocTime_, &time, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(sh);
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
