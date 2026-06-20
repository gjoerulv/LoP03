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
};

}  // namespace cd::score
