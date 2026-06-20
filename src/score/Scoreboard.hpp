#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "content/LoadReport.hpp"
#include "score/ScoreEntry.hpp"

namespace cd::score {

inline constexpr int kScoreboardVersion = 1;
inline constexpr std::size_t kMaxScoreEntries = 50;

// Higher score first, then fewer battle turns, then more danger defeated.
bool ranksAbove(const ScoreEntry& a, const ScoreEntry& b);

// Persistent high-score table (global JSON file in the user data dir). Defensive
// loading: a missing file is an empty board; malformed files are reported, never
// crash.
class Scoreboard {
public:
    explicit Scoreboard(std::filesystem::path path);

    bool load(content::LoadReport& report);
    bool save(content::LoadReport& report) const;

    void add(const ScoreEntry& entry);  // inserts ranked, caps to kMaxScoreEntries
    const std::vector<ScoreEntry>& entries() const { return entries_; }
    bool empty() const { return entries_.empty(); }
    const std::filesystem::path& path() const { return path_; }

private:
    void sortAndCap();

    std::filesystem::path path_;
    std::vector<ScoreEntry> entries_;
};

}  // namespace cd::score
