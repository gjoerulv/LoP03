#include "assets/AssetManifest.hpp"

#include <fstream>
#include <sstream>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

#include "content/JsonValidation.hpp"
#include "platform/Paths.hpp"

namespace cd::assets {

namespace fs = std::filesystem;
using content::Json;

namespace {
constexpr const char* kSource = "manifest.json";
}

std::string_view assetTypeName(AssetType t) {
    switch (t) {
        case AssetType::Texture: return "texture";
        case AssetType::Font: return "font";
        case AssetType::Music: return "music";
        case AssetType::Ambience: return "ambience";
        case AssetType::Sfx: return "sfx";
        case AssetType::Animation: return "animation";
    }
    return "texture";
}

std::optional<AssetType> assetTypeFromName(std::string_view name) {
    if (name == "texture") return AssetType::Texture;
    if (name == "font") return AssetType::Font;
    if (name == "music") return AssetType::Music;
    if (name == "ambience") return AssetType::Ambience;
    if (name == "sfx") return AssetType::Sfx;
    if (name == "animation") return AssetType::Animation;
    return std::nullopt;
}

bool AssetManifest::parseText(const std::string& text, content::LoadReport& report) {
    entries_.clear();
    byId_.clear();

    Json root = Json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        report.add(kSource, "<root>", "not a JSON object");
        return false;
    }
    content::ObjectReader rootReader(root, "manifest", kSource, report);
    const int version = rootReader.reqInt("version");
    if (version < 1 || version > kManifestVersion) {
        report.add(kSource, "version",
                   "unsupported manifest version " + std::to_string(version));
        return false;
    }

    const auto it = root.find("assets");
    if (it == root.end()) {
        return true;  // empty catalog
    }
    if (!it->is_array()) {
        report.add(kSource, "assets", "expected an array");
        return false;
    }

    for (const Json& item : *it) {
        if (!item.is_object()) {
            report.add(kSource, "assets[]", "entry is not an object; skipped");
            continue;
        }
        content::ObjectReader entryReader(item, "asset", kSource, report);
        AssetEntry entry;
        entry.id = entryReader.reqString("id");
        const std::string typeName = entryReader.reqString("type");
        if (entry.id.empty() || typeName.empty()) {
            continue;  // errors already reported
        }
        const auto type = assetTypeFromName(typeName);
        if (!type) {
            report.add(kSource, entry.id, "unknown asset type '" + typeName + "'; skipped");
            continue;
        }
        entry.type = *type;
        if (byId_.count(entry.id) > 0) {
            report.add(kSource, entry.id, "duplicate id; later entry skipped");
            continue;
        }

        if (entry.type == AssetType::Animation) {
            // Animations have no file: they describe a frame strip inside a
            // texture entry of this same catalog.
            entry.texture = entryReader.reqString("texture");
            entry.row = entryReader.optIntMin("row", 0, 0);
            entry.frameCount = entryReader.optInt("frameCount", 0);
            entry.frameWidth = entryReader.optInt("frameWidth", 0);
            entry.frameHeight = entryReader.optInt("frameHeight", 0);
            entry.frameTime = entryReader.optFloat("frameTime", 0.15f);
            if (entry.texture.empty()) {
                continue;  // error already reported
            }
            if (entry.frameCount < 1 || entry.frameWidth < 1 || entry.frameHeight < 1) {
                report.add(kSource, entry.id,
                           "animation needs frameCount/frameWidth/frameHeight >= 1; skipped");
                continue;
            }
            if (!(entry.frameTime > 0.0f)) {
                report.add(kSource, entry.id, "frameTime must be > 0; skipped");
                continue;
            }
        } else {
            const std::string path = entryReader.reqString("path");
            if (path.empty()) {
                continue;
            }
            const auto safe = paths::sanitizeRelative(path);
            if (!safe) {
                report.add(kSource, entry.id, "unsafe path '" + path + "'; skipped");
                continue;
            }
            entry.path = safe->generic_string();

            entry.filter = entryReader.optString("filter", "nearest");
            if (entry.filter != "nearest" && entry.filter != "bilinear") {
                report.add(kSource, entry.id,
                           "unknown filter '" + entry.filter + "'; using nearest");
                entry.filter = "nearest";
            }
            entry.fontSize = entryReader.optIntMin("size", 1, 10);
            entry.volume = entryReader.optFloat("volume", 1.0f);
            if (entry.volume < 0.0f || entry.volume > 1.0f) {
                report.add(kSource, entry.id, "volume outside 0..1; clamped");
                entry.volume = entry.volume < 0.0f ? 0.0f : 1.0f;
            }
        }
        if (const auto loop = item.find("loop"); loop != item.end()) {
            if (loop->is_boolean()) {
                entry.loop = loop->get<bool>();
            } else {
                report.add(kSource, entry.id, "loop must be a boolean; using true");
            }
        }

        byId_[entry.id] = entries_.size();
        entries_.push_back(std::move(entry));
    }

    dropDanglingAnimations(report);
    return true;
}

// Drops animation entries whose strip texture is not a texture entry of the
// current catalog (unknown id, or dropped earlier by validation/file checks).
void AssetManifest::dropDanglingAnimations(content::LoadReport& report) {
    std::unordered_set<std::string> textureIds;
    for (const AssetEntry& entry : entries_) {
        if (entry.type == AssetType::Texture) {
            textureIds.insert(entry.id);
        }
    }
    std::vector<AssetEntry> kept;
    kept.reserve(entries_.size());
    byId_.clear();
    for (AssetEntry& entry : entries_) {
        if (entry.type == AssetType::Animation && textureIds.count(entry.texture) == 0) {
            report.add(kSource, entry.id,
                       "animation references unknown texture '" + entry.texture + "'; skipped");
            continue;
        }
        byId_[entry.id] = kept.size();
        kept.push_back(std::move(entry));
    }
    entries_ = std::move(kept);
}

bool AssetManifest::load(const fs::path& root, content::LoadReport& report, bool checkFiles) {
    entries_.clear();
    byId_.clear();

    const fs::path file = root / "manifest.json";
    std::error_code ec;
    if (!fs::exists(file, ec) || ec) {
        return true;  // no manifest: valid empty catalog, everything falls back
    }
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        report.add(kSource, "<file>", "could not open " + file.string());
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const bool ok = parseText(buffer.str(), report);
    if (!ok) {
        return false;
    }

    if (checkFiles) {
        std::vector<AssetEntry> kept;
        kept.reserve(entries_.size());
        byId_.clear();
        for (AssetEntry& entry : entries_) {
            if (entry.type != AssetType::Animation &&
                (!fs::exists(root / entry.path, ec) || ec)) {
                report.add(kSource, entry.id, "file not found: " + entry.path + "; skipped");
                continue;
            }
            byId_[entry.id] = kept.size();
            kept.push_back(std::move(entry));
        }
        entries_ = std::move(kept);
        dropDanglingAnimations(report);  // a dropped strip texture orphans its animations
    }
    return true;
}

const AssetEntry* AssetManifest::find(const std::string& id) const {
    const auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : &entries_[it->second];
}

}  // namespace cd::assets
