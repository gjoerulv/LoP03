#include "render/VirtualScreen.hpp"

#include "render/Viewport.hpp"

namespace cd {

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

void VirtualScreen::blitToWindow(int windowWidth, int windowHeight, Color bars) const {
    ClearBackground(bars);

    const ViewportRect vp = computeViewport(windowWidth, windowHeight, width_, height_);

    // Source rect has a negative height because render textures are y-flipped.
    const Rectangle src{0.0f, 0.0f, static_cast<float>(width_), -static_cast<float>(height_)};
    const Rectangle dst{vp.x, vp.y, vp.width, vp.height};

    DrawTexturePro(target_.get().texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
}

}  // namespace cd
