#include "resource/ResourceManager.hpp"

#include <filesystem>

#include "core/Log.hpp"
#include "platform/Paths.hpp"

namespace cd {

namespace fs = std::filesystem;

ResourceManager::ResourceManager() {
    // The default font ships inside raylib; copy the struct but never unload it.
    defaultFont_ = GetFontDefault();
}

TextureHandle ResourceManager::makePlaceholderTexture() const {
    // 16x16 magenta/black checker — instantly recognizable as "missing asset".
    Image img = GenImageChecked(16, 16, 4, 4, MAGENTA, BLACK);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return TextureHandle(tex);
}

const Texture2D& ResourceManager::placeholder() {
    if (!placeholder_.valid()) {
        placeholder_ = makePlaceholderTexture();
    }
    return placeholder_.get();
}

const Texture2D& ResourceManager::texture(const std::string& key, const std::string& assetPath) {
    if (auto it = textures_.find(key); it != textures_.end()) {
        return it->second.get();
    }

    // Resolve and validate the asset path before touching the filesystem.
    TextureHandle handle;
    if (auto safe = paths::sanitizeRelative(assetPath)) {
        const fs::path full = fs::path(assetRoot_) / *safe;
        std::error_code ec;
        if (fs::exists(full, ec) && !ec) {
            Texture2D tex = LoadTexture(full.string().c_str());
            if (tex.id != 0) {
                SetTextureFilter(tex, TEXTURE_FILTER_POINT);
                handle = TextureHandle(tex);
            }
        }
    }

    if (!handle.valid()) {
        log::warn("ResourceManager: using placeholder for texture '" + key + "' (path '" +
                  assetPath + "')");
        handle = makePlaceholderTexture();
    }

    auto [it, _] = textures_.emplace(key, std::move(handle));
    return it->second.get();
}

const Font& ResourceManager::font(const std::string& key, const std::string& assetPath, int size) {
    if (auto it = fonts_.find(key); it != fonts_.end()) {
        return it->second.get();
    }

    if (auto safe = paths::sanitizeRelative(assetPath)) {
        const fs::path full = fs::path(assetRoot_) / *safe;
        std::error_code ec;
        if (fs::exists(full, ec) && !ec) {
            Font f = LoadFontEx(full.string().c_str(), size, nullptr, 0);
            if (f.texture.id != 0) {
                auto [it, _] = fonts_.emplace(key, FontHandle(f));
                return it->second.get();
            }
        }
    }

    log::warn("ResourceManager: using default font for '" + key + "' (path '" + assetPath + "')");
    return defaultFont_;
}

void ResourceManager::clear() {
    textures_.clear();
    fonts_.clear();
    placeholder_.reset();
}

}  // namespace cd
