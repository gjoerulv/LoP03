#pragma once

#include <string>

// Shared Details text for the result screen and the scoreboard (M22): one
// place describes the score components so the two surfaces never drift.

namespace cd {

inline std::string scoreDetailsText() {
    return "Score components: completion base + boss bonus + chest bonus + "
           "danger bonus + no-death bonus - battle-turn penalty - escape "
           "penalty, plus an accepted omen wager (+150 finished with no "
           "deaths / -100 finished with deaths).\n\nAfter completion, fewest "
           "battle turns is the main ranking - decisive fights beat long "
           "safe ones. Escaping the boss or retreating scores 0: a finished "
           "run always beats an abandoned one.\n\nCompare runs at the same "
           "Depth and Lv; both are shown on the Scoreboard.";
}

}  // namespace cd
