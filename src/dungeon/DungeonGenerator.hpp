#pragma once

#include <cstdint>
#include <string>

#include "dungeon/DungeonModel.hpp"

namespace cd {
namespace content {
class ContentDatabase;
}
}  // namespace cd

namespace cd::dungeon {

// Deterministically generates a dungeon from a seed. Same seed + depth + theme +
// content => identical dungeon. Guarantees: a Start and a Boss room, a single
// main path between them, at least 3 mandatory gated doors on that path, and at
// least one guarded chest (space permitting). Enemy teams, the boss, and chest
// rewards are drawn from the theme's pools (falling back to all content if the
// theme id is unknown). Pure (no raylib).
Dungeon generate(std::uint64_t seed, int depth, const content::ContentDatabase& db,
                 std::string themeId = "");

}  // namespace cd::dungeon
