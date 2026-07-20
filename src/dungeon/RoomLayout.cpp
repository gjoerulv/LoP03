#include "dungeon/RoomLayout.hpp"

#include <algorithm>
#include <queue>
#include <utility>

#include "dungeon/Rng.hpp"

namespace cd::dungeon {

namespace {

using Cell = RoomLayout::Cell;
using Point = RoomLayout::Point;

struct DimRange {
    int wLo = 0;
    int wHi = 0;
    int hLo = 0;
    int hHi = 0;
};

// Owner-approved dimension ranges (M16 kickoff, 2026-07-19): common chambers
// 9-15 x 7-11, corridors 9-13 long x 5-7 wide, boss arena 15-19 x 11-13.
DimRange dimsFor(RoomArchetype a, const Room& room) {
    switch (a) {
        case RoomArchetype::Entry: return {9, 11, 7, 9};
        case RoomArchetype::Crossroads: return {11, 15, 9, 11};
        case RoomArchetype::GateChamber: return {9, 13, 7, 11};
        case RoomArchetype::TreasureAlcove: return {7, 9, 7, 9};
        case RoomArchetype::TreasureVault: return {9, 13, 7, 11};
        case RoomArchetype::BossAntechamber: return {9, 13, 7, 9};
        case RoomArchetype::BossArena: return {15, 19, 11, 13};
        case RoomArchetype::EventChamber: return {7, 9, 7, 9};
        case RoomArchetype::Corridor: break;
    }
    // Corridor: long axis along a straight door pair; bent corridors stay a
    // small chamber.
    const bool ns = room.hasDoor(Dir::North) && room.hasDoor(Dir::South);
    const bool ew = room.hasDoor(Dir::East) && room.hasDoor(Dir::West);
    if (ns && !ew) {
        return {5, 7, 9, 13};
    }
    if (ew && !ns) {
        return {9, 13, 5, 7};
    }
    return {7, 9, 7, 9};
}

int idx(const RoomLayout& l, int x, int y) { return y * l.width + x; }

void setCell(RoomLayout& l, int x, int y, Cell c) {
    l.cells[static_cast<std::size_t>(idx(l, x, y))] = c;
}

// Steps from a wall tile toward the room interior.
Point inward(Point p, Dir wall, int steps) {
    const Dir in = opposite(wall);
    return Point{p.x + dirDx(in) * steps, p.y + dirDy(in) * steps};
}

constexpr int kDx[kDirCount] = {0, 1, 0, -1};  // N, E, S, W (room-local)
constexpr int kDy[kDirCount] = {-1, 0, 1, 0};

// Reachability check for one door configuration. openMask bit i = door in
// direction i is carved open; closed gated doors get a solid gate block on
// their interior anchor. Guard and boss anchors are solid in every
// configuration (they fall only when their encounter is defeated).
void appendConfigProblems(const Room& room, const RoomLayout& l, unsigned openMask,
                          const char* label, std::vector<std::string>& out) {
    const int w = l.width;
    const int h = l.height;
    std::vector<char> solid(static_cast<std::size_t>(w * h), 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            solid[static_cast<std::size_t>(y * w + x)] = l.at(x, y) == Cell::Wall ? 1 : 0;
        }
    }

    std::vector<Point> seeds;
    std::vector<Point> mustReach;    // tiles that must be walkable and reached
    std::vector<Point> mustTouch;    // solid tiles needing a reachable neighbor

    for (int di = 0; di < kDirCount; ++di) {
        const Dir dir = static_cast<Dir>(di);
        if (!room.hasDoor(dir)) {
            continue;
        }
        if ((openMask & (1u << di)) != 0) {
            for (const Point& p : l.doorGap(dir)) {
                solid[static_cast<std::size_t>(p.y * w + p.x)] = 0;
                mustReach.push_back(p);
            }
            for (const Point& p : l.interiorGap(dir)) {
                mustReach.push_back(p);
            }
            seeds.push_back(l.interiorGap(dir)[0]);
        } else if (room.door(dir).gated) {
            const Point g = l.interiorGap(dir)[0];
            solid[static_cast<std::size_t>(g.y * w + g.x)] = 1;
            mustTouch.push_back(g);
        }
    }
    if (l.guard.valid()) {
        solid[static_cast<std::size_t>(l.guard.y * w + l.guard.x)] = 1;
        mustTouch.push_back(l.guard);
    }
    if (l.boss.valid()) {
        solid[static_cast<std::size_t>(l.boss.y * w + l.boss.x)] = 1;
        mustTouch.push_back(l.boss);
    }
    if (l.event.valid()) {
        solid[static_cast<std::size_t>(l.event.y * w + l.event.x)] = 1;
        mustTouch.push_back(l.event);
    }
    if (l.chest.valid()) {
        mustReach.push_back(l.chest);
    }
    if (room.type == RoomType::Start) {
        seeds.push_back(l.centerSpawn);
    }
    if (seeds.empty()) {
        return;  // the player cannot be in the room in this configuration
    }

    std::vector<char> reached(static_cast<std::size_t>(w * h), 0);
    std::queue<Point> q;
    const Point first = seeds.front();
    if (solid[static_cast<std::size_t>(first.y * w + first.x)] != 0) {
        out.push_back(std::string(label) + ": seed tile is solid");
        return;
    }
    reached[static_cast<std::size_t>(first.y * w + first.x)] = 1;
    q.push(first);
    while (!q.empty()) {
        const Point p = q.front();
        q.pop();
        for (int di = 0; di < kDirCount; ++di) {
            const int nx = p.x + kDx[di];
            const int ny = p.y + kDy[di];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
                continue;
            }
            const std::size_t ni = static_cast<std::size_t>(ny * w + nx);
            if (solid[ni] == 0 && reached[ni] == 0) {
                reached[ni] = 1;
                q.push(Point{nx, ny});
            }
        }
    }

    auto isReached = [&](const Point& p) {
        return p.valid() && reached[static_cast<std::size_t>(p.y * w + p.x)] != 0;
    };
    for (const Point& s : seeds) {
        if (!isReached(s)) {
            out.push_back(std::string(label) + ": entry spawn unreachable");
        }
    }
    for (const Point& p : mustReach) {
        if (!isReached(p)) {
            out.push_back(std::string(label) + ": required tile (" + std::to_string(p.x) + "," +
                          std::to_string(p.y) + ") unreachable");
        }
    }
    for (const Point& p : mustTouch) {
        bool touched = false;
        for (int di = 0; di < kDirCount && !touched; ++di) {
            touched = isReached(Point{p.x + kDx[di], p.y + kDy[di]});
        }
        if (!touched) {
            out.push_back(std::string(label) + ": interactable (" + std::to_string(p.x) + "," +
                          std::to_string(p.y) + ") cannot be faced");
        }
    }
}

// Every configuration a player can experience: fully open (end state),
// pristine (all gates closed), and each gated door as the freshly cleared
// entry with the remaining gates still closed.
std::vector<std::string> connectivityProblems(const Room& room, const RoomLayout& l) {
    std::vector<std::string> out;
    unsigned allOpen = 0;
    unsigned nonGated = 0;
    for (int di = 0; di < kDirCount; ++di) {
        const Dir dir = static_cast<Dir>(di);
        if (!room.hasDoor(dir)) {
            continue;
        }
        allOpen |= 1u << di;
        if (!room.door(dir).gated) {
            nonGated |= 1u << di;
        }
    }
    appendConfigProblems(room, l, allOpen, "open", out);
    appendConfigProblems(room, l, nonGated, "pristine", out);
    for (int di = 0; di < kDirCount; ++di) {
        const Dir dir = static_cast<Dir>(di);
        if (room.hasDoor(dir) && room.door(dir).gated) {
            appendConfigProblems(room, l, nonGated | (1u << di), "cleared-gate", out);
        }
    }
    return out;
}

}  // namespace

const char* archetypeName(RoomArchetype a) {
    switch (a) {
        case RoomArchetype::Entry: return "Entry";
        case RoomArchetype::Corridor: return "Corridor";
        case RoomArchetype::Crossroads: return "Crossroads";
        case RoomArchetype::GateChamber: return "GateChamber";
        case RoomArchetype::TreasureAlcove: return "TreasureAlcove";
        case RoomArchetype::TreasureVault: return "TreasureVault";
        case RoomArchetype::BossAntechamber: return "BossAntechamber";
        case RoomArchetype::BossArena: return "BossArena";
        case RoomArchetype::EventChamber: return "EventChamber";
    }
    return "Unknown";
}

RoomArchetype classifyRoom(const Dungeon& d, int roomIndex) {
    const Room& room = d.rooms[static_cast<std::size_t>(roomIndex)];
    if (room.type == RoomType::Boss) {
        return RoomArchetype::BossArena;
    }
    if (room.type == RoomType::Start) {
        return RoomArchetype::Entry;
    }
    if (room.type == RoomType::Event) {
        return RoomArchetype::EventChamber;
    }
    if (room.type == RoomType::Treasure) {
        return room.teamIndex >= 0 ? RoomArchetype::TreasureVault : RoomArchetype::TreasureAlcove;
    }
    int doorCount = 0;
    bool bossAdjacent = false;
    bool gated = false;
    for (int di = 0; di < kDirCount; ++di) {
        const Door& door = room.doors[static_cast<std::size_t>(di)];
        if (door.neighbor < 0) {
            continue;
        }
        ++doorCount;
        bossAdjacent = bossAdjacent || door.neighbor == d.bossRoom;
        gated = gated || door.gated;
    }
    if (bossAdjacent) {
        return RoomArchetype::BossAntechamber;
    }
    if (gated) {
        return RoomArchetype::GateChamber;
    }
    return doorCount >= 3 ? RoomArchetype::Crossroads : RoomArchetype::Corridor;
}

std::uint64_t roomLocalSeed(std::uint64_t dungeonSeed, int generationVersion, int roomIndex,
                            RoomArchetype archetype) {
    auto mix = [](std::uint64_t x) {
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    };
    std::uint64_t h = mix(dungeonSeed ^ 0x43727973526F6F6Dull);  // "CrysRoom"
    h = mix(h ^ static_cast<std::uint64_t>(generationVersion));
    h = mix(h ^ static_cast<std::uint64_t>(roomIndex));
    h = mix(h ^ static_cast<std::uint64_t>(archetype));
    return h;
}

std::array<Point, 2> RoomLayout::doorGap(Dir d) const {
    const int gx = width / 2 - 1;
    const int gy = height / 2 - 1;
    switch (d) {
        case Dir::North: return {{{gx, 0}, {gx + 1, 0}}};
        case Dir::South: return {{{gx, height - 1}, {gx + 1, height - 1}}};
        case Dir::East: return {{{width - 1, gy}, {width - 1, gy + 1}}};
        case Dir::West: return {{{0, gy}, {0, gy + 1}}};
    }
    return {{{gx, 0}, {gx + 1, 0}}};
}

std::array<Point, 2> RoomLayout::interiorGap(Dir d) const {
    std::array<Point, 2> gap = doorGap(d);
    for (Point& p : gap) {
        p = inward(p, d, 1);
    }
    return gap;
}

RoomLayout realizeRoom(const Dungeon& d, int roomIndex, int generationVersion) {
    const Room& room = d.rooms[static_cast<std::size_t>(roomIndex)];
    const RoomArchetype archetype = classifyRoom(d, roomIndex);
    Rng rng(roomLocalSeed(d.seed, generationVersion, roomIndex, archetype));

    RoomLayout l;
    l.archetype = archetype;
    const DimRange range = dimsFor(archetype, room);
    l.width = rng.range(range.wLo, range.wHi);
    l.height = rng.range(range.hLo, range.hHi);
    l.cells.assign(static_cast<std::size_t>(l.width * l.height), Cell::Floor);
    for (int x = 0; x < l.width; ++x) {
        setCell(l, x, 0, Cell::Wall);
        setCell(l, x, l.height - 1, Cell::Wall);
    }
    for (int y = 0; y < l.height; ++y) {
        setCell(l, 0, y, Cell::Wall);
        setCell(l, l.width - 1, y, Cell::Wall);
    }

    const int cx = l.width / 2;
    const int cy = l.height / 2;
    l.centerSpawn = Point{cx, cy};

    if (room.type == RoomType::Boss) {
        l.boss = Point{cx, cy};
        l.centerSpawn = Point{cx, cy + 2};
    }
    if (room.chest.present) {
        Dir doorDir = Dir::North;
        for (int di = 0; di < kDirCount; ++di) {
            if (room.hasDoor(static_cast<Dir>(di))) {
                doorDir = static_cast<Dir>(di);
                break;
            }
        }
        switch (doorDir) {
            case Dir::North: l.chest = Point{cx, l.height - 2}; break;
            case Dir::South: l.chest = Point{cx, 1}; break;
            case Dir::East: l.chest = Point{1, cy}; break;
            case Dir::West: l.chest = Point{l.width - 2, cy}; break;
        }
        if (room.teamIndex >= 0) {
            l.guard = (doorDir == Dir::North || doorDir == Dir::South)
                          ? Point{l.chest.x + 1, l.chest.y}
                          : Point{l.chest.x, l.chest.y + 1};
        }
    }
    if (room.type == RoomType::Event) {
        // Event anchor at the focal wall opposite the single entrance,
        // exactly like a chest.
        Dir doorDir = Dir::North;
        for (int di = 0; di < kDirCount; ++di) {
            if (room.hasDoor(static_cast<Dir>(di))) {
                doorDir = static_cast<Dir>(di);
                break;
            }
        }
        switch (doorDir) {
            case Dir::North: l.event = Point{cx, l.height - 2}; break;
            case Dir::South: l.event = Point{cx, 1}; break;
            case Dir::East: l.event = Point{1, cy}; break;
            case Dir::West: l.event = Point{l.width - 2, cy}; break;
        }
    }

    // Keep-clear mask: door lanes (two tiles deep, gated or not), anchors,
    // and their neighbors. Obstacles never land here, so the number and
    // order of RNG draws depends only on immutable topology facts.
    std::vector<char> keep(static_cast<std::size_t>(l.width * l.height), 0);
    auto mark = [&](Point p) {
        if (l.inBounds(p.x, p.y)) {
            keep[static_cast<std::size_t>(idx(l, p.x, p.y))] = 1;
        }
    };
    auto markWithNeighbors = [&](Point p) {
        mark(p);
        for (int di = 0; di < kDirCount; ++di) {
            mark(Point{p.x + kDx[di], p.y + kDy[di]});
        }
    };
    for (int di = 0; di < kDirCount; ++di) {
        const Dir dir = static_cast<Dir>(di);
        if (!room.hasDoor(dir)) {
            continue;
        }
        for (const Point& p : l.doorGap(dir)) {
            mark(p);
            mark(inward(p, dir, 1));
            mark(inward(p, dir, 2));
        }
    }
    markWithNeighbors(l.centerSpawn);
    if (l.chest.valid()) markWithNeighbors(l.chest);
    if (l.guard.valid()) markWithNeighbors(l.guard);
    if (l.boss.valid()) markWithNeighbors(l.boss);
    if (l.event.valid()) markWithNeighbors(l.event);

    // Landmark obstacles: corner-inset pillars everywhere, plus a diagonal
    // ring around the center in crossroads and arenas. Composition stays
    // deliberately sparse; real prop art is M17.
    std::vector<Point> candidates = {{2, 2},
                                     {l.width - 3, 2},
                                     {2, l.height - 3},
                                     {l.width - 3, l.height - 3}};
    if (archetype == RoomArchetype::Crossroads || archetype == RoomArchetype::BossArena) {
        candidates.push_back(Point{cx - 2, cy - 2});
        candidates.push_back(Point{cx + 2, cy - 2});
        candidates.push_back(Point{cx - 2, cy + 2});
        candidates.push_back(Point{cx + 2, cy + 2});
    }
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                    [&](const Point& p) {
                                        return p.x < 1 || p.y < 1 || p.x > l.width - 2 ||
                                               p.y > l.height - 2 ||
                                               keep[static_cast<std::size_t>(idx(l, p.x, p.y))] !=
                                                   0 ||
                                               !l.walkable(p.x, p.y);
                                    }),
                     candidates.end());

    int desired = 0;
    switch (archetype) {
        case RoomArchetype::Corridor:
        case RoomArchetype::TreasureAlcove:
            break;  // clean rooms
        case RoomArchetype::Crossroads:
        case RoomArchetype::BossArena:
            desired = rng.range(3, 6);
            break;
        default:
            desired = rng.range(2, 4);
            break;
    }
    for (int i = static_cast<int>(candidates.size()) - 1; i > 0; --i) {
        std::swap(candidates[static_cast<std::size_t>(i)],
                  candidates[static_cast<std::size_t>(rng.range(0, i))]);
    }
    std::vector<Point> placed;
    for (int i = 0; i < desired && i < static_cast<int>(candidates.size()); ++i) {
        const Point p = candidates[static_cast<std::size_t>(i)];
        setCell(l, p.x, p.y, Cell::Wall);
        placed.push_back(p);
    }

    // Safety net: if the obstacles ever break reachability, drop them. The
    // keep-clear mask makes this a cold path, but the layout must always be
    // valid.
    if (!placed.empty() && !connectivityProblems(room, l).empty()) {
        for (const Point& p : placed) {
            setCell(l, p.x, p.y, Cell::Floor);
        }
    }
    return l;
}

std::vector<RoomLayout> realizeAllRooms(const Dungeon& d, int generationVersion) {
    std::vector<RoomLayout> out;
    out.reserve(d.rooms.size());
    for (int i = 0; i < static_cast<int>(d.rooms.size()); ++i) {
        out.push_back(realizeRoom(d, i, generationVersion));
    }
    return out;
}

std::vector<std::string> validateLayout(const Dungeon& d, int roomIndex,
                                        const RoomLayout& l) {
    std::vector<std::string> out;
    const Room& room = d.rooms[static_cast<std::size_t>(roomIndex)];

    if (l.width < kMinRoomSide || l.width > kMaxRoomWidth || l.height < kMinRoomSide ||
        l.height > kMaxRoomHeight) {
        out.push_back("dimensions out of bounds");
        return out;
    }
    if (l.cells.size() != static_cast<std::size_t>(l.width * l.height)) {
        out.push_back("cell buffer size mismatch");
        return out;
    }
    if (l.archetype != classifyRoom(d, roomIndex)) {
        out.push_back("archetype does not match topology classification");
    }
    for (int x = 0; x < l.width; ++x) {
        if (l.at(x, 0) != Cell::Wall || l.at(x, l.height - 1) != Cell::Wall) {
            out.push_back("border not closed");
            break;
        }
    }
    for (int y = 0; y < l.height; ++y) {
        if (l.at(0, y) != Cell::Wall || l.at(l.width - 1, y) != Cell::Wall) {
            out.push_back("border not closed");
            break;
        }
    }

    auto interior = [&](const Point& p) {
        return p.x >= 1 && p.y >= 1 && p.x <= l.width - 2 && p.y <= l.height - 2;
    };
    if (room.chest.present != l.chest.valid()) {
        out.push_back("chest anchor does not match the room's chest");
    }
    if (l.chest.valid() && (!interior(l.chest) || !l.walkable(l.chest.x, l.chest.y))) {
        out.push_back("chest anchor not on a walkable interior tile");
    }
    if (l.guard.valid() && !interior(l.guard)) {
        out.push_back("guard anchor not interior");
    }
    if (room.type == RoomType::Boss && room.teamIndex >= 0 && !l.boss.valid()) {
        out.push_back("boss room without a boss anchor");
    }
    if (l.boss.valid() && !interior(l.boss)) {
        out.push_back("boss anchor not interior");
    }
    if ((room.type == RoomType::Event) != l.event.valid()) {
        out.push_back("event anchor does not match the room type");
    }
    if (l.event.valid() && !interior(l.event)) {
        out.push_back("event anchor not interior");
    }
    if (!interior(l.centerSpawn) || !l.walkable(l.centerSpawn.x, l.centerSpawn.y) ||
        l.centerSpawn == l.boss || l.centerSpawn == l.guard) {
        out.push_back("center spawn not on a free interior tile");
    }

    if (out.empty()) {
        out = connectivityProblems(room, l);
    }
    return out;
}

}  // namespace cd::dungeon
