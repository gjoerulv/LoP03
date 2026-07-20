#include "score/Scoreboard.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

#include "content/ContentLoader.hpp"
#include "content/JsonValidation.hpp"

namespace cd::score {

namespace fs = std::filesystem;
using content::Json;

bool ranksAbove(const ScoreEntry& a, const ScoreEntry& b) {
    if (a.score != b.score) {
        return a.score > b.score;
    }
    if (a.battleTurns != b.battleTurns) {
        return a.battleTurns < b.battleTurns;  // fewer turns is better
    }
    return a.dangerDefeated > b.dangerDefeated;
}

Scoreboard::Scoreboard(fs::path path) : path_(std::move(path)) {}

void Scoreboard::sortAndCap() {
    std::stable_sort(entries_.begin(), entries_.end(), ranksAbove);
    if (entries_.size() > kMaxScoreEntries) {
        entries_.resize(kMaxScoreEntries);
    }
}

void Scoreboard::add(const ScoreEntry& entry) {
    entries_.push_back(entry);
    sortAndCap();
}

bool Scoreboard::save(content::LoadReport& report) const {
    const std::string source = path_.filename().string();

    Json root;
    root["version"] = kScoreboardVersion;
    Json arr = Json::array();
    for (const ScoreEntry& e : entries_) {
        Json j;
        j["score"] = e.score;
        j["battleTurns"] = e.battleTurns;
        j["dangerDefeated"] = e.dangerDefeated;
        j["chestsOpened"] = e.chestsOpened;
        j["noDeath"] = e.noDeath ? 1 : 0;
        j["depth"] = e.depth;
        j["theme"] = e.theme;
        j["seed"] = e.seed;
        j["generationVersion"] = e.generationVersion;
        j["partyLevel"] = e.partyLevel;
        arr.push_back(std::move(j));
    }
    root["entries"] = std::move(arr);

    std::error_code ec;
    fs::create_directories(path_.parent_path(), ec);
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    if (!out) {
        report.add(source, "<file>", "could not open scoreboard for writing");
        return false;
    }
    out << root.dump(2) << '\n';
    return static_cast<bool>(out);
}

bool Scoreboard::load(content::LoadReport& report) {
    entries_.clear();
    const std::string source = path_.filename().string();

    std::error_code ec;
    if (!fs::exists(path_, ec) || ec) {
        return true;  // no scoreboard yet -> empty, not an error
    }

    Json root;
    if (!content::readJsonFile(path_, root, report)) {
        return false;
    }
    if (!root.is_object()) {
        report.add(source, "<root>", "expected a top-level JSON object");
        return false;
    }

    content::ObjectReader rootReader(root, "scoreboard", source, report);
    const int version = rootReader.reqInt("version");
    if (version != kScoreboardVersion) {
        report.add(source, "version", "unsupported scoreboard version");
        return false;
    }

    auto entriesIt = root.find("entries");
    if (entriesIt == root.end() || !entriesIt->is_array()) {
        report.add(source, "entries", "expected array");
        return false;
    }

    int index = 0;
    for (const auto& element : *entriesIt) {
        const std::string ctx = "entries[" + std::to_string(index) + "]";
        ++index;
        if (!element.is_object()) {
            report.add(source, ctx, "expected object");
            continue;
        }
        content::ObjectReader er(element, ctx, source, report);
        ScoreEntry e;
        e.score = er.optInt("score", 0);
        e.battleTurns = er.optIntMin("battleTurns", 0, 0);
        e.dangerDefeated = er.optIntMin("dangerDefeated", 0, 0);
        e.chestsOpened = er.optIntMin("chestsOpened", 0, 0);
        e.noDeath = er.optInt("noDeath", 0) != 0;
        e.depth = er.optIntMin("depth", 1, 1);
        e.theme = er.optString("theme");
        e.generationVersion = er.optIntMin("generationVersion", 0, 0);
        e.partyLevel = er.optIntMin("partyLevel", 0, 0);
        if (auto seedIt = element.find("seed");
            seedIt != element.end() && seedIt->is_number_unsigned()) {
            e.seed = seedIt->get<std::uint64_t>();
        }
        entries_.push_back(std::move(e));
    }

    sortAndCap();
    return true;
}

}  // namespace cd::score
