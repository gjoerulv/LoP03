#pragma once

#include "battle/Battle.hpp"

namespace cd {
namespace content {
class ContentDatabase;
}
}  // namespace cd

// Deterministically auto-resolves a battle with simple AI for BOTH sides. Pure
// (no raylib), used by balance/validation tests (difficulty curves, playability,
// score sanity) and available for future auto-resolve features.

namespace cd::battle {

struct SimResult {
    Outcome outcome = Outcome::Ongoing;  // Ongoing means it hit the round cap
    int rounds = 0;
    int partyHpRemaining = 0;
    int partyMaxHp = 0;
    int partyAlive = 0;

    // Fraction of party HP remaining (0..1); 0 if the party has no max HP.
    float partyHpFraction() const {
        return partyMaxHp > 0 ? static_cast<float>(partyHpRemaining) / static_cast<float>(partyMaxHp)
                              : 0.0f;
    }
};

SimResult simulate(Battle battle, const content::ContentDatabase& db, int maxRounds = 300);

}  // namespace cd::battle
