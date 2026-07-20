#include "render/SpriteDraw.hpp"

#include "assets/AssetManifest.hpp"
#include "render/Animation.hpp"
#include "resource/ResourceManager.hpp"

namespace cd::render {

bool drawAnimationCentered(ResourceManager& resources, const std::string& animId, float t,
                           float cx, float cy, Color tint) {
    const assets::AssetEntry* anim = resources.animation(animId);
    if (anim == nullptr || !resources.hasTexture(anim->texture)) {
        return false;
    }
    const FrameRect r = frameRect(*anim, frameAt(*anim, t));
    const Rectangle src{static_cast<float>(r.x), static_cast<float>(r.y),
                        static_cast<float>(r.w), static_cast<float>(r.h)};
    // Snap to whole pixels so 1x rendering stays crisp.
    const Vector2 pos{static_cast<float>(static_cast<int>(cx - r.w * 0.5f)),
                      static_cast<float>(static_cast<int>(cy - r.h * 0.5f))};
    DrawTextureRec(resources.texture(anim->texture), src, pos, tint);
    return true;
}

bool drawTextureCentered(ResourceManager& resources, const std::string& textureId, float cx,
                         float cy, Color tint) {
    if (!resources.hasTexture(textureId)) {
        return false;
    }
    const Texture2D& tex = resources.texture(textureId);
    DrawTexture(tex, static_cast<int>(cx - tex.width * 0.5f),
                static_cast<int>(cy - tex.height * 0.5f), tint);
    return true;
}

}  // namespace cd::render
