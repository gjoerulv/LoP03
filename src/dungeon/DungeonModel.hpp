#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Plain data describing a generated dungeon: a graph of rooms (laid out on a
// grid) connected by doors, with visible enemy teams, chests, mandatory gates,
// and a boss. No raylib; generation and queries are unit-tested.

namespace cd::dungeon {

enum class RoomType { Start, Normal, Treasure, Boss };

enum class Dir { North, East, South, West };
inline constexpr int kDirCount = 4;

Dir opposite(Dir d);
int dirDx(Dir d);
int dirDy(Dir d);

struct EnemyTeam {
    std::string name;
    std::vector<std::string> enemyIds;  // ids into the content database
    std::vector<std::string> tags;      // aggregated unique tags (display)
    bool isBoss = false;
    std::string bossId;  // for boss teams: the BossDef id (enemyIds are its minions)

    int count() const { return static_cast<int>(enemyIds.size()) + (bossId.empty() ? 0 : 1); }
};

struct Chest {
    bool present = false;
    bool guarded = false;
    bool opened = false;
    int gold = 0;
    std::string itemId;  // optional item reward (may be empty)
    std::string rarity;  // display string for the item, if any
};

struct Door {
    int neighbor = -1;     // room index, or -1 for a wall
    bool gated = false;    // a mandatory gate team blocks this door
    int teamIndex = -1;    // index into Dungeon::teams when gated
};

struct Room {
    int gridX = 0;
    int gridY = 0;
    RoomType type = RoomType::Normal;
    std::array<Door, kDirCount> doors{};
    Chest chest{};
    int teamIndex = -1;  // a team occupying the room (chest guard, or the boss)
    bool visited = false;

    bool hasDoor(Dir d) const { return doors[static_cast<std::size_t>(d)].neighbor >= 0; }
    const Door& door(Dir d) const { return doors[static_cast<std::size_t>(d)]; }
    Door& door(Dir d) { return doors[static_cast<std::size_t>(d)]; }
};

struct Dungeon {
    std::uint64_t seed = 0;
    int depth = 1;
    std::string themeName;
    int gridW = 0;
    int gridH = 0;

    std::vector<Room> rooms;
    std::vector<EnemyTeam> teams;
    std::vector<int> mainPath;  // room indices from start to boss
    int startRoom = 0;
    int bossRoom = 0;
    int mandatoryGates = 0;  // gated doors on the path to the boss

    int chestCount() const;
    int guardedChestCount() const;
};

}  // namespace cd::dungeon
