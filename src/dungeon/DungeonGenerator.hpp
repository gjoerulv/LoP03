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

}  // namespace cd::dungeon
