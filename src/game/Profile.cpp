#include "game/Profile.hpp"

#include <fstream>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

#include "platform/AtomicFile.hpp"

namespace cd {

namespace {
constexpr const char* kSource = "profile.json";
using Json = nlohmann::json;
}  // namespace

bool parseProfileText(const std::string& text, ProfileData& data,
                      content::LoadReport& report) {
    data = ProfileData{};
    Json root = Json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        report.add(kSource, "", "not valid JSON; starting with a fresh profile");
        return false;
    }
    const auto version = root.find("version");
    if (version == root.end() || !version->is_number_integer() ||
        version->get<int>() != kProfileVersion) {
        report.add(kSource, "", "missing or unsupported version; starting fresh");
        return false;
    }
    if (const auto flag = root.find("kingDefeated"); flag != root.end() && flag->is_boolean()) {
        data.kingDefeated = flag->get<bool>();
    }
    return true;
}

std::string serializeProfile(const ProfileData& data) {
    Json root;
    root["version"] = kProfileVersion;
    root["kingDefeated"] = data.kingDefeated;
    return root.dump(2) + "\n";
}

ProfileStore::ProfileStore(std::filesystem::path file) : file_(std::move(file)) {}

bool ProfileStore::load(content::LoadReport& report) {
    data = ProfileData{};
    std::ifstream in(file_);
    if (!in) {
        return true;  // first run: nothing unlocked yet
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return parseProfileText(buffer.str(), data, report);
}

bool ProfileStore::save(content::LoadReport& report) const {
    std::string writeError;
    if (!platform::writeTextFileAtomically(file_, serializeProfile(data), writeError)) {
        report.add(kSource, "", "could not save the profile atomically: " + writeError);
        return false;
    }
    return true;
}

bool ProfileStore::recordKingDefeated() {
    if (data.kingDefeated) {
        return false;  // already unlocked; nothing to write or announce
    }
    data.kingDefeated = true;
    content::LoadReport report;
    save(report);  // best effort: a failed write only risks re-unlocking later
    return true;
}

}  // namespace cd
