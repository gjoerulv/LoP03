#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "content/LoadReport.hpp"

// M124 - the Hall of Shame: a record of every Iron Man run that ended in a
// wipe (game/IronMan.hpp), kept OUTSIDE the saves - an Iron Man party never
// reaches a slot - in its own versioned `fallen_runs.json` beside the
// achievements (the AchievementStore pattern: defensive load, atomic write,
// malformed or foreign -> an empty hall, never a crash).
//
// A record is where the party fell, to whom, and a snapshot of the party in
// exactly the slot codec's own format (SaveSystem::serialize), so the title's
// Hall of Shame can open the same summary the send-off showed. The store
// never interprets the snapshot: it embeds it and hands it back as text. The
// few list columns (leader, level, play time) are copied out at record time
// so the list needs no party parse.

namespace cd {

inline constexpr int kFallenRunsVersion = 1;
inline constexpr std::size_t kFallenRunsKept = 20;  // newest first; older runs fall off

struct FallenRun {
    std::string place;
    std::string foes;
    std::string leader;         // the first member's name
    int highestLevel = 0;
    long long playSeconds = 0;
    std::string party;          // SaveSystem::serialize text ("" = snapshot unusable)
};

// Newest first, capped at kFallenRunsKept.
void addFallenRun(std::vector<FallenRun>& runs, FallenRun run);

// In-memory codec (exposed for headless tests). Malformed text or a foreign
// version -> `runs` empty + a report line, returns false. A single bad record
// is skipped with a report line and the rest are kept.
bool parseFallenRunsText(const std::string& text, std::vector<FallenRun>& runs,
                         content::LoadReport& report);
std::string serializeFallenRuns(const std::vector<FallenRun>& runs);

class FallenRunStore {
public:
    explicit FallenRunStore(std::filesystem::path file);

    std::vector<FallenRun> runs;

    bool load(content::LoadReport& report);  // missing file -> an empty hall (true)
    bool save(content::LoadReport& report) const;
    // Adds the run (newest first, capped) and saves; a failed write keeps the
    // run in memory for this sitting and is reported, never thrown.
    bool record(FallenRun run, content::LoadReport& report);

    const std::filesystem::path& file() const { return file_; }

private:
    std::filesystem::path file_;
};

}  // namespace cd
