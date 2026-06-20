#pragma once

#include "dungeon/DungeonModel.hpp"

namespace cd {
namespace content {
class ContentDatabase;
}
}  // namespace cd

// Deterministic danger rating derived ONLY from enemy stats and abilities (never
// hand-authored). The tier compares a team's threat to a depth baseline. Pure;
// unit-tested.

namespace cd::danger {

enum class Tier { Trivial, Easy, Fair, Dangerous, Deadly, Boss };

const char* tierName(Tier t);
int tierWeight(Tier t);  // weight used for the "danger defeated" score bonus

// Weighted threat of a team: per-enemy stat threat + skill threat, scaled by a
// team-synergy factor (more enemies / a healer in the group raise it).
int teamThreat(const dungeon::EnemyTeam& team, const content::ContentDatabase& db);

// Maps a threat value to a tier relative to the dungeon depth baseline. Boss
// teams are always the Boss tier.
Tier tierFor(int threat, int depth, bool isBoss);

// Convenience: teamThreat + tierFor.
Tier assess(const dungeon::EnemyTeam& team, int depth, const content::ContentDatabase& db);

}  // namespace cd::danger
