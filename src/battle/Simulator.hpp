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

// The simulator's own party AI (M43: exposed so sim-vs-live agreement can be
// asserted directly, the way `chooseEnemyAction` already can). Deterministic:
// heal a badly hurt ally if able, else the strongest affordable damaging skill
// on the weakest enemy — except that a confused member is forced to a basic
// attack through the shared `confusedChoice`, exactly as the battle screen is.
EnemyChoice choosePartyAction(const Battle& b, int actor, const content::ContentDatabase& db);

// Carries out a decided turn (skill, attack, or an M44 forced Guard/Skip). Shared
// so a scripted battery can drive real turns — injecting item uses between them —
// without re-implementing the applier and drifting from it.
void applyChoice(Battle& b, int actor, const EnemyChoice& choice,
                 const content::ContentDatabase& db);

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

// Same, but resolves `battle` in place so the caller can read the final combatant
// states (HP/MP) — used to drive a no-heal gauntlet where the party's condition
// carries from one fight to the next (M40 castle challenges / their balance sim).
SimResult simulateInPlace(Battle& battle, const content::ContentDatabase& db, int maxRounds = 300);

}  // namespace cd::battle
