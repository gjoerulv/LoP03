#pragma once

#include <string>
#include <unordered_map>

#include "raylib.h"
#include "render/RaylibRAII.hpp"

// Caches and owns GPU resources (textures, fonts). Loads are keyed by a caller
// supplied id; the same id always returns the same cached resource. Missing or
// failed loads degrade gracefully to a generated placeholder / the default font,
// and never crash. Requires an initialized window (created/destroyed with one).

namespace cd {

class ResourceManager {
public:
    ResourceManager();

    // Returns a cached texture for `key`. On first request it loads `assetPath`
    // (a path relative to the assets directory). If that fails for any reason a
    // magenta/black checker placeholder is cached and returned instead.
    const Texture2D& texture(const std::string& key, const std::string& assetPath);

    // Shared placeholder texture (lazily generated).
    const Texture2D& placeholder();

    // Returns a cached font, or the raylib default font on failure. The default
    // font is never unloaded.
    const Font& font(const std::string& key, const std::string& assetPath, int size);
    const Font& defaultFont() const { return defaultFont_; }

    // Sets the base directory that asset paths are resolved against. Defaults to
    // "assets". The path is sanitized before use.
    void setAssetRoot(std::string root) { assetRoot_ = std::move(root); }

    void clear();

private:
    TextureHandle makePlaceholderTexture() const;

    std::string assetRoot_ = "assets";
    Font defaultFont_{};
    TextureHandle placeholder_;
    std::unordered_map<std::string, TextureHandle> textures_;
    std::unordered_map<std::string, FontHandle> fonts_;
};

}  // namespace cd
