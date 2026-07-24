#include "game/Achievements.hpp"

#include <fstream>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

#include "game/Party.hpp"
#include "game/Story.hpp"
#include "platform/AtomicFile.hpp"

namespace cd {

namespace {
constexpr const char* kSource = "achievements.json";
using Json = nlohmann::json;
}  // namespace

const AchievementDef* findAchievement(const std::string& id) {
    for (const AchievementDef& a : kAchievements) {
        if (id == a.id) {
            return &a;
        }
    }
    return nullptr;
}

bool achievementMet(const std::string& id, const Party& p, const AchvContext& ctx) {
    if (id == "first_clear") return ctx.clearedDungeon;
    if (id == "trailblazer") return p.highestUnlockedTown >= 4;
    if (id == "ladders_end") return p.highestUnlockedTown >= 7;
    if (id == "untouchable") return ctx.clearedDungeon && ctx.runNoDeath;
    if (id == "decisive") return ctx.clearedDungeon && ctx.runTurns > 0 && ctx.runTurns <= 20;
    if (id == "deep_diver") return ctx.clearedDungeon && ctx.runDepth >= 10;
    if (id == "second_nature") {
        for (const Character& m : p.members) {
            if (!m.equippedPassive.empty()) return true;
        }
        return false;
    }
    if (id == "high_roller") return p.legendaryTokens > 0;
    if (id == "the_climb") return p.castleUnlocked;
    if (id == "kingslayer") return p.castleRecords.kingDefeated;
    if (id == "gauntlet") return p.castleRecords.bossRushCleared();
    if (id == "the_long_night") return p.castleRecords.endlessBestWave >= 10;
    if (id == "loremaster") return storyAllHeard(p.storyMet);
    if (id == "peerless") {
        for (const Character& m : p.members) {
            if (m.level >= kMaxLevel) return true;
        }
        return false;
    }
    // M53: Champion is now an efficiency goal — beating the King in
    // kChampionKingTurns turns or fewer — read from the persisted best. A save
    // whose recorded best is already at/under the bar retro-unlocks it (a player
    // who beat him efficiently before this build should not have to again).
    if (id == "champion")
        return p.castleRecords.kingBestTurns > 0 &&
               p.castleRecords.kingBestTurns <= kChampionKingTurns;
    if (id == "naturalist") return p.encountered.size() >= 30;
    return false;
}

bool parseAchievementsText(const std::string& text, std::set<std::string>& unlocked,
                           content::LoadReport& report) {
    unlocked.clear();
    Json root = Json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        report.add(kSource, "", "not valid JSON; starting with no achievements");
        return false;
    }
    const auto version = root.find("version");
    if (version == root.end() || !version->is_number_integer() ||
        version->get<int>() != kAchievementVersion) {
        report.add(kSource, "", "missing or unsupported version; starting fresh");
        return false;
    }
    if (const auto arr = root.find("unlocked"); arr != root.end() && arr->is_array()) {
        for (const Json& item : *arr) {
            // Unknown ids are kept (may belong to a newer build).
            if (item.is_string()) {
                unlocked.insert(item.get<std::string>());
            }
        }
    }
    return true;
}

std::string serializeAchievements(const std::set<std::string>& unlocked) {
    Json root;
    root["version"] = kAchievementVersion;
    Json arr = Json::array();
    for (const std::string& id : unlocked) {  // std::set: stable sorted output
        arr.push_back(id);
    }
    root["unlocked"] = std::move(arr);
    return root.dump(2) + "\n";
}

AchievementStore::AchievementStore(std::filesystem::path file) : file_(std::move(file)) {}

bool AchievementStore::load(content::LoadReport& report) {
    unlocked.clear();
    std::ifstream in(file_);
    if (!in) {
        return true;  // first run: fresh state
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return parseAchievementsText(buffer.str(), unlocked, report);
}

bool AchievementStore::save(content::LoadReport& report) const {
    std::string writeError;
    if (!platform::writeTextFileAtomically(file_, serializeAchievements(unlocked), writeError)) {
        report.add(kSource, "", "could not save achievements atomically: " + writeError);
        return false;
    }
    return true;
}

std::vector<std::string> AchievementStore::check(const Party& party, const AchvContext& ctx) {
    std::vector<std::string> newly;
    for (const AchievementDef& a : kAchievements) {
        if (unlocked.count(a.id) == 0 && achievementMet(a.id, party, ctx)) {
            unlocked.insert(a.id);
            newly.push_back(a.id);
        }
    }
    if (!newly.empty()) {
        content::LoadReport report;
        save(report);  // best effort; a failed write only risks re-toasting later
    }
    return newly;
}

}  // namespace cd
