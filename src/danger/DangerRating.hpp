#pragma once

#include <vector>

#include "dungeon/DungeonModel.hpp"

namespace cd {
struct Character;
namespace content {
class ContentDatabase;
}
}  // namespace cd

// Deterministic danger rating derived ONLY from stats and abilities (never
// hand-authored). M68 (owner decision): the tier is PARTY-RELATIVE — a team's
// threat is compared to the current party's derived strength, so "Deadly"
// means deadly for THIS party, and the same team reads easier as the party
// grows. Rated once at dungeon entry (DungeonState snapshots the tiers, so
// the labels and the danger-defeated score credit agree for the whole run).
// Pure; unit-tested; the [danger-report] battery prints the calibration.

namespace cd::danger {

enum class Tier { Trivial, Easy, Fair, Dangerous, Deadly, Boss };

const char* tierName(Tier t);
int tierWeight(Tier t);  // weight used for the "danger defeated" score bonus

// Weighted threat of a team: per-enemy stat threat + skill threat, scaled by a
// team-synergy factor (more enemies / a healer in the group raise it) and the
// team's own stat scale (town ladder x depth).
int teamThreat(const dungeon::EnemyTeam& team, const content::ContentDatabase& db);

// The party side of the ratio: the same stat weights applied to a member's
// derived stats (gear and milestones included), summed over the party.
// Max stats, not current HP — the tier rates the matchup, not the wounds.
int memberThreat(const Character& c);
int partyThreat(const std::vector<Character>& members);

// Maps a team threat to a tier relative to the party's strength. Boss teams
// are always the Boss tier.
Tier tierFor(int threat, int partyThreat, bool isBoss);

// Convenience: teamThreat + tierFor.
Tier assess(const dungeon::EnemyTeam& team, const content::ContentDatabase& db,
            int partyThreat);

}  // namespace cd::danger
