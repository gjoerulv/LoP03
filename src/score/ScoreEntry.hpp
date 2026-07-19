#pragma once

#include <cstdint>
#include <string>

namespace cd::score {

// One recorded dungeon run on the scoreboard.
struct ScoreEntry {
    int score = 0;
    int battleTurns = 0;
    int dangerDefeated = 0;
    int chestsOpened = 0;
    bool noDeath = false;
    int depth = 1;
    std::string theme;
    std::uint64_t seed = 0;
    // Room-realization version the run was played under (see
    // dungeon::kGenerationVersion). 0 = recorded before versioning existed
    // (pre-M16). Optional in the file; the scoreboard format version is
    // unchanged so old files load as-is (owner decision, 2026-07-19).
    int generationVersion = 0;
};

}  // namespace cd::score
