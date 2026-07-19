#include "resource/ResourceManager.hpp"

#include <utility>

#include "assets/AssetManifest.hpp"
#include "core/Log.hpp"

namespace cd {

namespace fs = std::filesystem;

ResourceManager::ResourceManager() {
    // The default font ships inside raylib; copy the struct but never unload it.
    defaultFont_ = GetFontDefault();
}

void ResourceManager::setCatalog(const assets::AssetManifest* manifest, fs::path root) {
    manifest_ = manifest;
    root_ = std::move(root);
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

const Texture2D& ResourceManager::texture(const std::string& id) {
    if (auto it = textures_.find(id); it != textures_.end()) {
        return it->second.get();
    }

    // Resolve the logical id through the catalog (paths were sanitized and
    // existence-checked at manifest load).
    TextureHandle handle;
    const assets::AssetEntry* entry = manifest_ != nullptr ? manifest_->find(id) : nullptr;
    if (entry != nullptr && entry->type == assets::AssetType::Texture) {
        const fs::path full = root_ / entry->path;
        Texture2D tex = LoadTexture(full.string().c_str());
        if (tex.id != 0) {
            SetTextureFilter(tex, entry->filter == "bilinear" ? TEXTURE_FILTER_BILINEAR
                                                              : TEXTURE_FILTER_POINT);
            handle = TextureHandle(tex);
        }
    }

    if (!handle.valid()) {
        log::warn("ResourceManager: using placeholder for texture id '" + id + "'");
        handle = makePlaceholderTexture();
    }

    auto [it, _] = textures_.emplace(id, std::move(handle));
    return it->second.get();
}

const Font& ResourceManager::font(const std::string& id) {
    if (auto it = fonts_.find(id); it != fonts_.end()) {
        return it->second.get();
    }

    const assets::AssetEntry* entry = manifest_ != nullptr ? manifest_->find(id) : nullptr;
    if (entry != nullptr && entry->type == assets::AssetType::Font) {
        const fs::path full = root_ / entry->path;
        Font f = LoadFontEx(full.string().c_str(), entry->fontSize, nullptr, 0);
        if (f.texture.id != 0) {
            auto [it, _] = fonts_.emplace(id, FontHandle(f));
            return it->second.get();
        }
    }

    log::warn("ResourceManager: using default font for id '" + id + "'");
    return defaultFont_;
}

void ResourceManager::clear() {
    textures_.clear();
    fonts_.clear();
    placeholder_.reset();
}

}  // namespace cd
