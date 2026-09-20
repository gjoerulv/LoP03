#include "game/FallenRuns.hpp"

#include <fstream>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

#include "platform/AtomicFile.hpp"

namespace cd {

namespace {
using Json = nlohmann::json;
constexpr const char* kSource = "fallen_runs.json";

std::string stringField(const Json& obj, const char* key) {
    const auto it = obj.find(key);
    return it != obj.end() && it->is_string() ? it->get<std::string>() : std::string{};
}

long long intField(const Json& obj, const char* key) {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_number_integer()) {
        return 0;
    }
    const long long v = it->get<long long>();
    return v > 0 ? v : 0;
}
}  // namespace

void addFallenRun(std::vector<FallenRun>& runs, FallenRun run) {
    runs.insert(runs.begin(), std::move(run));
    if (runs.size() > kFallenRunsKept) {
        runs.resize(kFallenRunsKept);
    }
}

bool parseFallenRunsText(const std::string& text, std::vector<FallenRun>& runs,
                         content::LoadReport& report) {
    runs.clear();
    const Json root = Json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        report.add(kSource, "", "not valid JSON; starting with an empty hall");
        return false;
    }
    const auto version = root.find("version");
    if (version == root.end() || !version->is_number_integer() ||
        version->get<int>() != kFallenRunsVersion) {
        report.add(kSource, "", "missing or unsupported version; starting with an empty hall");
        return false;
    }
    const auto arr = root.find("runs");
    if (arr == root.end() || !arr->is_array()) {
        return true;  // a valid, empty hall
    }
    std::size_t index = 0;
    for (const Json& item : *arr) {
        const std::string ctx = "runs[" + std::to_string(index++) + "]";
        if (!item.is_object()) {
            report.add(kSource, ctx, "expected an object; record skipped");
            continue;
        }
        FallenRun run;
        run.place = stringField(item, "place");
        run.foes = stringField(item, "foes");
        run.leader = stringField(item, "leader");
        run.highestLevel = static_cast<int>(intField(item, "highestLevel"));
        run.playSeconds = intField(item, "playSeconds");
        if (const auto party = item.find("party"); party != item.end() && party->is_object()) {
            run.party = party->dump(2) + "\n";
        }
        if (run.place.empty() && run.foes.empty() && run.party.empty()) {
            report.add(kSource, ctx, "an empty record; skipped");
            continue;
        }
        runs.push_back(std::move(run));
        if (runs.size() >= kFallenRunsKept) {
            break;
        }
    }
    return true;
}

std::string serializeFallenRuns(const std::vector<FallenRun>& runs) {
    Json root;
    root["version"] = kFallenRunsVersion;
    Json arr = Json::array();
    for (const FallenRun& run : runs) {
        Json item;
        item["place"] = run.place;
        item["foes"] = run.foes;
        item["leader"] = run.leader;
        item["highestLevel"] = run.highestLevel;
        item["playSeconds"] = run.playSeconds;
        // The snapshot is embedded as the object it is; text that is not a
        // JSON object is dropped rather than poisoning the file.
        const Json party = Json::parse(run.party, nullptr, false);
        if (!party.is_discarded() && party.is_object()) {
            item["party"] = party;
        }
        arr.push_back(std::move(item));
    }
    root["runs"] = std::move(arr);
    return root.dump(2) + "\n";
}

FallenRunStore::FallenRunStore(std::filesystem::path file) : file_(std::move(file)) {}

bool FallenRunStore::load(content::LoadReport& report) {
    runs.clear();
    std::ifstream in(file_);
    if (!in) {
        return true;  // nobody has fallen yet
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return parseFallenRunsText(buffer.str(), runs, report);
}

bool FallenRunStore::save(content::LoadReport& report) const {
    std::string writeError;
    if (!platform::writeTextFileAtomically(file_, serializeFallenRuns(runs), writeError)) {
        report.add(kSource, "", "could not save the fallen runs atomically: " + writeError);
        return false;
    }
    return true;
}

bool FallenRunStore::record(FallenRun run, content::LoadReport& report) {
    addFallenRun(runs, std::move(run));
    return save(report);
}

}  // namespace cd
