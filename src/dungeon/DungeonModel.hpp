#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Plain data describing a generated dungeon: a graph of rooms (laid out on a
// grid) connected by doors, with visible enemy teams, chests, mandatory gates,
// and a boss. No raylib; generation and queries are unit-tested.

namespace cd::dungeon {

enum class RoomType { Start, Normal, Treasure, Boss, Event };

// Room events (M20, owner-approved set; M30 adds RestToken). TrappedChest is a
// chest flag, not an event room. Every event shows its full trade-off before
// confirmation.
// M44 adds RoyalRelic: a rare event that REPLACES one of the rolled events (it is
// never part of the shuffled kind list), granting one of the four Royal Relics.
enum class RoomEventKind {
    None, Shrine, HealingSpring, Merchant, EliteChallenge, ScoreWager, RestToken, RoyalRelic
};

struct RoomEvent {
    RoomEventKind kind = RoomEventKind::None;
    bool resolved = false;
    int goldCost = 0;       // shrine offering / merchant price
    std::string itemId;     // merchant: the offered item
};

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
    // Depth stat scaling (M20, composition.json): 100 = base stats. Applied
    // when combatants are built and when danger is assessed, so displayed
    // danger always matches what the player will fight.
    int statScalePct = 100;

    int count() const { return static_cast<int>(enemyIds.size()) + (bossId.empty() ? 0 : 1); }
};

struct Chest {
    bool present = false;
    bool guarded = false;
    bool opened = false;
    bool trapped = false;  // visible trap: extra gold, taking it wounds the party
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
    RoomEvent event{};   // for RoomType::Event side rooms
    int teamIndex = -1;  // a team occupying the room (chest guard, elite challenge, or the boss)
    bool visited = false;

    bool hasDoor(Dir d) const { return doors[static_cast<std::size_t>(d)].neighbor >= 0; }
    const Door& door(Dir d) const { return doors[static_cast<std::size_t>(d)]; }
    Door& door(Dir d) { return doors[static_cast<std::size_t>(d)]; }
};

struct Dungeon {
    std::uint64_t seed = 0;
    int depth = 1;
    int town = 1;  // town ladder index (M32); scales enemy stats + score bonus
    std::string themeName;
    std::string themeId;  // content id ("ruined_keep"); presentation keys off it
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
