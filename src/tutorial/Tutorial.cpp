#include "tutorial/Tutorial.hpp"

#include "platform/AtomicFile.hpp"

#include <fstream>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace cd::tutorial {

namespace {
constexpr const char* kSource = "tutorial.json";
using Json = nlohmann::json;
} // namespace

const Beat* findBeat(std::string_view id) {
  for (const Beat& b : kBeats) {
    if (id == b.id) {
      return &b;
    }
  }
  return nullptr;
}

bool parseTutorialText(const std::string& text, Progress& p, content::LoadReport& report) {
  p = Progress{};
  Json root = Json::parse(text, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    report.add(kSource, "", "not valid JSON; starting with a fresh tutorial state");
    return false;
  }
  const auto version = root.find("version");
  if (version == root.end() || !version->is_number_integer() ||
      version->get<int>() != kTutorialVersion) {
    report.add(kSource, "", "missing or unsupported version; starting fresh");
    return false;
  }
  if (const auto enabled = root.find("enabled");
      enabled != root.end() && enabled->is_boolean()) {
    p.enabled = enabled->get<bool>();
  }
  if (const auto seen = root.find("seen"); seen != root.end() && seen->is_array()) {
    for (const Json& item : *seen) {
      // Unknown ids are kept: they may belong to a newer build and must
      // not re-fire after a downgrade/upgrade round trip.
      if (item.is_string()) {
        p.seen.insert(item.get<std::string>());
      }
    }
  }
  return true;
}

std::string serializeTutorial(const Progress& p) {
  Json root;
  root["version"] = kTutorialVersion;
  root["enabled"] = p.enabled;
  Json seen = Json::array();
  for (const std::string& id : p.seen) { // std::set: stable sorted output
    seen.push_back(id);
  }
  root["seen"] = std::move(seen);
  return root.dump(2) + "\n";
}

TutorialStore::TutorialStore(std::filesystem::path file) : file_(std::move(file)) {}

bool TutorialStore::load(content::LoadReport& report) {
  state = Progress{};
  std::ifstream in(file_);
  if (!in) {
    return true; // first run: fresh state, nothing to report
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  return parseTutorialText(buffer.str(), state, report);
}

bool TutorialStore::save(content::LoadReport& report) const {
  std::string writeError;
  if (!platform::writeTextFileAtomically(file_, serializeTutorial(state), writeError)) {
    report.add(kSource, "", "could not save tutorial state atomically: " + writeError);
    return false;
  }
  return true;
}

bool TutorialStore::takeBeat(const std::string& id) {
  if (!state.enabled || state.seen.count(id) > 0) {
    return false;
  }
  state.seen.insert(id);
  content::LoadReport report;
  save(report); // best effort; a failed write only risks a repeat prompt
  return true;
}

void TutorialStore::reset() {
  state.seen.clear();
  content::LoadReport report;
  save(report);
}

} // namespace cd::tutorial
