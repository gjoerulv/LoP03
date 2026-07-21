#pragma once

#include <string>

// Shared Details text for the result screen and the scoreboard (M22): one
// place describes the score components so the two surfaces never drift.

namespace cd {

inline std::string scoreDetailsText() {
    return "Score = completion base + boss + chest + danger + no-death "
           "bonuses, minus battle-turn and escape penalties, plus any omen "
           "wager (+150 no-death / -100 with deaths). A higher town then adds "
           "a bonus to the whole total (+10% per town, up to +100%).\n\n"
           "After completion, fewest battle turns is the main ranking - "
           "decisive fights beat long safe ones. Escaping the boss or "
           "retreating scores 0.\n\nCompare runs at the same Town, Depth and "
           "Lv, all shown on the Scoreboard.";
}

}  // namespace cd
