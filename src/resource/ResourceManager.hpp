#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "raylib.h"
#include "render/RaylibRAII.hpp"

// Caches and owns GPU resources (textures, fonts), resolved through the asset
// manifest by stable logical ID (M14): states never pass file paths. Missing
// ids, missing files, or failed loads degrade gracefully to a generated
// placeholder / the default font, and never crash. Reload model (owner
// decision): callers cache nothing and re-fetch by ID; reload() drops the
// cache and resources re-resolve through the current catalog on next use.
// Requires an initialized window (created/destroyed with one).

namespace cd {

namespace assets {
class AssetManifest;
struct AssetEntry;
}

class ResourceManager {
public:
    ResourceManager();

    // Points the manager at the live catalog and the directory manifest paths
    // are relative to. The manifest is not owned and must outlive use.
    void setCatalog(const assets::AssetManifest* manifest, std::filesystem::path root);

    // True when the catalog maps this id to a texture entry. States use this
    // to fall back to the pre-asset drawing path (colored shapes) instead of
    // showing the missing-asset checker for roles that simply have no art yet.
    bool hasTexture(const std::string& id) const;

    // Returns the cached texture for a logical id ("ui.frame.default"). On
    // first request the id is resolved through the catalog; unknown ids or
    // failed loads cache the magenta/black checker placeholder instead.
    const Texture2D& texture(const std::string& id);

    // Shared placeholder texture (lazily generated).
    const Texture2D& placeholder();

    // Returns the cached font for a logical id, or the raylib default font on
    // any failure. The default font is never unloaded.
    const Font& font(const std::string& id);
    const Font& defaultFont() const { return defaultFont_; }

    // Returns the catalog's animation entry for a logical id, or nullptr when
    // absent (callers then fall back to a static texture or shape). The
    // returned entry lives in the catalog; per the reload model, re-fetch by
    // id every frame instead of caching it.
    const assets::AssetEntry* animation(const std::string& id) const;

    // Drops every cached resource; next use re-resolves through the catalog.
    void reload() { clear(); }
    void clear();

private:
    TextureHandle makePlaceholderTexture() const;

    const assets::AssetManifest* manifest_ = nullptr;
    std::filesystem::path root_ = "assets";
    Font defaultFont_{};
    TextureHandle placeholder_;
    std::unordered_map<std::string, TextureHandle> textures_;
    std::unordered_map<std::string, FontHandle> fonts_;
};

}  // namespace cd
