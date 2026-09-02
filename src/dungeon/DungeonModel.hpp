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
// M55 adds the three per-theme rites (ArmoryGhost / MinersCache / ElderRoot): each
// is GUARANTEED exactly once per dungeon of its theme (forced onto the first event
// slot) and never appears outside that theme. See dungeon/ThemeEvents.hpp.
// M76 adds DuckPeddler: a rare replacement of one plain rolled event, decided by
// a PURE hash of the seed (never an rng draw — every other roll of a seed is
// byte-identical, so generation stays v14; see dungeon/ThemeEvents.hpp). The
// peddler sells the Evil Duckling for flat gold and will not deal while the
// party already owns one.
// M93 (generation v17) adds two more pure-hash replacements of plain rolled
// events: the Surveyor (only where the map starts fogged — pays the fog away
// for 20 gold; since v23 a utility drawn FIRST, outside the encounter tier)
// and Dragonform (any run — the party fights its NEXT battle as Dragons for a
// flat -100 score, stated up front). Same contract as the DuckPeddler: never
// an rng draw, never a rite/relic slot.
// M103 (generation v19) adds six more on the same contract, all towns and
// themes: GoosePolymorph (one random non-goose member becomes a Goose for the
// REST of the run, +100 score), Sacrifice (give up one bag equipment piece for
// double XP in the next battle), LevelAltar (level one member up, their MP
// drops to 0; at the cap only a dry line), StrangerStory (a tale from THE
// STRANGER "P" and 20 MP for a chosen member), TokenExchange (1 legendary
// token for 3 rest tokens or 1 map piece), PatrolReset (the fuse rewinds to
// 100). Party-state gates (all geese, no token, empty bag) are checked at
// interaction, never at generation.
// M104 (generation v20) adds the two gambling dens, same contract: Reels (a
// ONE-SHOT machine — 1 spin for 10g or 3 for 70g, the bad bundle being the
// owner's joke; three-of-a-kind pays the symbol's prize) and Blackjack (bet
// gold, dealer stands 17, a win pays the bet back doubled).
enum class RoomEventKind {
    None, Shrine, HealingSpring, Merchant, EliteChallenge, ScoreWager, RestToken, RoyalRelic,
    ArmoryGhost, MinersCache, ElderRoot, DuckPeddler, Surveyor, Dragonform,
    GoosePolymorph, Sacrifice, LevelAltar, StrangerStory, TokenExchange, PatrolReset,
    Reels, Blackjack,
    // M106 (generation v21): the Goosy Gauntlet's rite — the WHOLE party
    // fights the next battle as Geese, +300 score. Like every rite it rolls
    // in the encounter tier at kEncounterChancePct (v23; leveled v22).
    GoosyFlock
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
    // M93: a danger-counter patrol (owner decisions 5/7). Pays XP but no gold
    // (game/Spoils.hpp reads this) and earns no danger-defeated credit.
    bool patrol = false;

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
    // M82: the seed the PLAYER entered at the Guild. On floor 0 (and every
    // 1-floor run) it equals `seed`; deeper floors generate from a derived
    // sub-seed in `seed` while `runSeed` keeps the re-enterable identity the
    // scoreboard, black market, and boss drops key off.
    std::uint64_t runSeed = 0;
    // M82: this floor's position in its run. floorCount 1 is every pre-M82
    // dungeon; floors below floorCount-1 hold an elite stair-gate in the boss
    // slot instead of the boss, and `stairsOpen` flips live when it falls.
    int floorIndex = 0;
    int floorCount = 1;
    bool stairsOpen = false;
    // M105: an Eternal floor (town 7's endless descent). floorCount holds an
    // unreachable sentinel so finalFloor() never fires — every boss guards
    // stairs, nothing completes, nothing scores; the next floor is generated
    // on demand from floorSeed(runSeed, floorIndex + 1).
    bool eternal = false;
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
    // M65: the room holding a Secret Map Piece, or -1 (the common case). At
    // most one per dungeon, seeded by a PURE hash of the dungeon seed (no
    // generator-Rng draw, so every other roll of a seed is untouched);
    // cleared live when the piece is taken.
    int mapPieceRoom = -1;
    // M66: the single-use dungeon treasure map — the room holding the CHART
    // and the room where the treasure lies BURIED (both -1 usually; ~12% of
    // dungeons carry the pair, same pure-hash contract, always two distinct
    // Normal rooms and never the map-piece room). The chart clears live when
    // read; the buried spot is claimable only once the chart was found.
    int chartRoom = -1;
    int buriedRoom = -1;

    int chestCount() const;
    int guardedChestCount() const;
};

}  // namespace cd::dungeon
