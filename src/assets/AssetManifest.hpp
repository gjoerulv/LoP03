#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "content/LoadReport.hpp"

// The versioned asset manifest (M14, schema v1 — owner-approved 2026-07-19):
// assets/manifest.json maps stable dotted logical IDs (public identifiers once
// shipped) to files and per-type metadata. Parsing/validation is raylib-free
// and headless-tested; loading actual GPU/audio data happens in
// ResourceManager/AudioManager, which treat this catalog as read-only truth.
//
//   { "version": 1, "assets": [
//       { "id": "sfx.ui.confirm", "type": "sfx",
//         "path": "audio/sfx/confirm.wav", "volume": 0.9 } ] }
//
// Missing manifest.json is a valid empty catalog (everything falls back).

namespace cd::assets {

inline constexpr int kManifestVersion = 1;

enum class AssetType { Texture, Font, Music, Ambience, Sfx };

std::string_view assetTypeName(AssetType t);
std::optional<AssetType> assetTypeFromName(std::string_view name);

struct AssetEntry {
    std::string id;
    AssetType type = AssetType::Texture;
    std::string path;  // relative to the assets root, sanitized

    // Per-type metadata (defaults apply when omitted).
    std::string filter = "nearest";  // texture: "nearest" | "bilinear"
    int fontSize = 10;               // font: base size
    bool loop = true;                // music/ambience
    float volume = 1.0f;             // music/ambience/sfx: 0..1 multiplier
};

class AssetManifest {
public:
    // Parses manifest text (exposed for headless tests). Returns false only
    // for unusable input (bad JSON / wrong version / bad root); individual
    // bad entries are reported and skipped, valid ones survive.
    bool parseText(const std::string& text, content::LoadReport& report);

    // Loads <root>/manifest.json. A missing file yields a valid empty
    // catalog (logged as info by the caller, not an error). When
    // checkFiles is true, every entry's file must exist under root or the
    // entry is reported and dropped.
    bool load(const std::filesystem::path& root, content::LoadReport& report,
              bool checkFiles = true);

    const AssetEntry* find(const std::string& id) const;
    const std::vector<AssetEntry>& entries() const { return entries_; }
    std::size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }

private:
    std::vector<AssetEntry> entries_;
    std::unordered_map<std::string, std::size_t> byId_;
};

}  // namespace cd::assets
