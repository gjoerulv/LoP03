#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "dungeon/DungeonModel.hpp"

// Compact room realization (M16). Topology (DungeonGenerator) keeps deciding
// connectivity, gates, rewards, and the boss; this layer decides each room's
// dimensions, internal collision, door/marker anchors, and spawn points.
// Realization draws only from a derived room-local seed — never from the
// topology RNG — so presentation changes cannot alter what a published
// dungeon seed means. Pure and unit-tested (no raylib).

namespace cd::dungeon {

// Bumped when generation changes how a seed plays. Version 1 is the pre-M16
// fixed 26x15 room build; version 2 is the compact archetype system;
// version 3 adds composition constraints, depth stat scaling, and room
// events (M20); version 4 is the M23 early-ramp tuning (elite pressure
// values in data/composition.json — owner-approved, battery-evidenced);
// version 5 enlarges the theme enemy/boss pools with the M29 content
// expansion, so a given seed spawns a different roster (owner-approved);
// version 6 adds the M30 RestToken room event to the event pool, changing
// each seed's event shuffle/placement (owner-approved); version 7 (M37) gives
// the dungeon merchant a 75% bargain (was a 130% markup) and gates chest gear by
// town (per-town equipment); version 8 (M38) adds town-gated per-town enemies and
// bosses, changing a seed's roster at towns 2+; version 9 (M43) reprices the
// consumables a dungeon merchant offers and adds a town-1-only item to the town-1
// chest/merchant pools; version 10 (M44) draws the Royal Relic replacement roll
// from the event stream at town >= 2 / depth >= 2, so a seed's events shift —
// each changes generated output for a seed (owner-approved).
inline constexpr int kGenerationVersion = 10;

// Largest realized room; must stay inside the 426x240 exploration viewport
// at 16px tiles with the 16px footer reserved (26x14 max drawable).
inline constexpr int kMaxRoomWidth = 19;
inline constexpr int kMaxRoomHeight = 13;
inline constexpr int kMinRoomSide = 5;

enum class RoomArchetype {
    Entry,            // start room
    Corridor,         // 2-door pass-through
    Crossroads,       // 3+ doors
    GateChamber,      // has a gated door (non-boss-adjacent)
    TreasureAlcove,   // unguarded chest side room
    TreasureVault,    // guarded chest side room
    BossAntechamber,  // main-path room with a door into the boss room
    BossArena,        // boss room
    EventChamber,     // event side room (M20: shrine/spring/merchant/...)
};
inline constexpr int kRoomArchetypeCount = 9;

const char* archetypeName(RoomArchetype a);

// Classify from immutable topology facts. Call on a freshly generated
// dungeon (before play mutates gate/guard state) so archetypes are stable
// for the whole run.
RoomArchetype classifyRoom(const Dungeon& d, int roomIndex);

// Stable derived seed: same inputs => same layout; distinct rooms, versions,
// and archetypes get independent streams.
std::uint64_t roomLocalSeed(std::uint64_t dungeonSeed, int generationVersion, int roomIndex,
                            RoomArchetype archetype);

// One realized room. `cells` is the immutable base: a closed border wall
// plus interior obstacles. Door gaps are carved by the presentation layer
// from doorGap()/interiorGap() according to live gate state, and gate/guard/
// boss anchors become solid marker tiles while their encounters stand.
struct RoomLayout {
    enum class Cell : std::uint8_t { Floor, Wall };

    struct Point {
        int x = -1;
        int y = -1;
        bool valid() const { return x >= 0 && y >= 0; }
        bool operator==(const Point&) const = default;
    };

    int width = 0;
    int height = 0;
    RoomArchetype archetype = RoomArchetype::Corridor;

    std::vector<Cell> cells;  // width * height, row-major base collision

    Point centerSpawn;   // start-room / fallback spawn (walkable)
    Point chest;         // walkable chest anchor, if the room has a chest
    Point guard;         // guard anchor beside the chest (solid while guarded)
    Point boss;          // boss anchor (solid while the boss stands)
    Point event;         // event anchor (solid until the event resolves)

    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
    Cell at(int x, int y) const {
        return inBounds(x, y) ? cells[static_cast<std::size_t>(y * width + x)] : Cell::Wall;
    }
    bool walkable(int x, int y) const { return at(x, y) == Cell::Floor; }

    // The two border tiles forming a door gap on the given wall, and the
    // matching interior tiles one step inward. interiorGap(d)[0] doubles as
    // the entry spawn and the gate-marker anchor (mirroring the pre-M16
    // room build).
    std::array<Point, 2> doorGap(Dir d) const;
    std::array<Point, 2> interiorGap(Dir d) const;
};

RoomLayout realizeRoom(const Dungeon& d, int roomIndex,
                       int generationVersion = kGenerationVersion);
std::vector<RoomLayout> realizeAllRooms(const Dungeon& d,
                                        int generationVersion = kGenerationVersion);

// Validates a realized layout against its room's graph facts: dimension
// bounds, closed borders, anchor sanity, and BFS reachability of every door,
// chest, and encounter anchor in the fully-open configuration, the pristine
// configuration (gates closed), and each cleared-gate entry configuration.
// Returns human-readable problems; empty means valid.
std::vector<std::string> validateLayout(const Dungeon& d, int roomIndex, const RoomLayout& layout);

}  // namespace cd::dungeon
