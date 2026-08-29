#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dungeon/DungeonModel.hpp"

namespace cd {
namespace content {
class ContentDatabase;
}
}  // namespace cd

namespace cd::dungeon {

// Deterministically generates a dungeon from a seed. Same seed + depth + theme +
// town + content => identical dungeon. Guarantees: a Start and a Boss room, a
// single main path between them, at least 3 mandatory gated doors on that path,
// and at least one guarded chest (space permitting). Enemy teams, the boss, and
// chest rewards are drawn from the theme's pools (falling back to all content if
// the theme id is unknown). Pure (no raylib).
//
// `town` (1..kTownCount, M32) multiplies every enemy team's stat scale on top of
// depth scaling; town 1 is the identity, so town-1 output is byte-identical to
// pre-M32 and the RNG stream never depends on town (the town multiplier did not
// move kGenerationVersion, which later content bumps carried to 10).
Dungeon generate(std::uint64_t seed, int depth, const content::ContentDatabase& db,
                 std::string themeId = "", int town = 1);

// v23 (owner direction 2026-08-28): the caller's floor context for the event
// pass. `fogged` MEANS "the minimap starts fogged" — the owner's rule for
// when the Surveyor is possible. Today only multi-floor descents fog the
// map, but any future mode answers by that meaning, never by its floor
// count. The Surveyor rolls from (runSeed, floorIndex) so every floor of a
// run answers independently while the floor's own stream stays untouched.
// The default context (standalone single floor, no fog) makes the short
// overload above byte-identical to calling this one.
struct FloorContext {
    std::uint64_t runSeed = 0;
    int floorIndex = 0;
    bool fogged = false;
};
Dungeon generate(std::uint64_t seed, int depth, const content::ContentDatabase& db,
                 std::string themeId, int town, const FloorContext& ctx);

// M82: the derived per-floor generation seed. Floor 0 IS the run seed (so a
// 1-floor run generates byte-identically to pre-M82); deeper floors get an
// independent pure-hash stream. Exposed so tests can prove floor independence.
std::uint64_t floorSeed(std::uint64_t runSeed, int floorIndex);

// M82: a 1-or-4-floor run — `floorCount` standard levels, each generated from
// floorSeed(seed, i) at the SAME depth (owner decision: flat). Floors before
// the last hold an elite "Stairway Wardens" gate in the boss slot (a post-pass
// swap from a fresh pure-hash Rng, so every floor is otherwise byte-identical
// to its sub-seed's standalone generation); the real boss waits on the last.
// generateFloors(seed, ..., 1) returns exactly { generate(seed, ...) }.
std::vector<Dungeon> generateFloors(std::uint64_t seed, int depth,
                                    const content::ContentDatabase& db,
                                    std::string themeId = "", int town = 1,
                                    int floorCount = 1);

// M105 (owner request): ONE floor of the town-7 Eternal descent — the
// standalone generation of floorSeed(runSeed, floorIndex) at the fixed
// Eternal depth, which therefore carries its REAL theme boss (no warden
// swap: in Eternal every boss guards the stairs). Difficulty escalates a
// flat +10 %pts on every team per floor past the first (the M49 Endless
// Rush curve shape); the map-piece room is cleared (an endless run feeds
// no economy) and the floor bookkeeping is stamped for the endless shape.
// Deterministic per (runSeed, floorIndex) — reload-honest at any depth of
// the descent. No score ever flows from these floors (they cannot
// complete), so no generation bump: existing modes are byte-identical.
inline constexpr int kEternalDepth = 20;
inline constexpr int kEternalEscalationPctPts = 10;
inline constexpr int kEternalFloorCountSentinel = 1000000;  // never the last
Dungeon generateEternalFloor(std::uint64_t runSeed, int floorIndex,
                             const content::ContentDatabase& db,
                             const std::string& themeId, int town);

// M93: the danger counter's roused patrol — a normal team for this dungeon
// (same theme pool, composition rules, and depth/town scaling as generation),
// drawn from a fresh pure-hash Rng off (runSeed, patrolIndex) so the Nth
// patrol of a run is deterministic and reload-honest, and the floor's own
// generation stream is never touched. The team is flagged `patrol`: it pays
// XP but no gold (game/Spoils.hpp) and earns no danger credit (owner
// decision 7).
EnemyTeam patrolTeam(const content::ContentDatabase& db, const std::string& themeId,
                     int town, int depth, std::uint64_t runSeed, int patrolIndex);

}  // namespace cd::dungeon
