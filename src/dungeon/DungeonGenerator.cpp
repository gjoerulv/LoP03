#include "dungeon/DungeonGenerator.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <utility>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Enums.hpp"
#include "dungeon/Rng.hpp"

namespace cd::dungeon {

namespace {

constexpr int kGridW = 7;
constexpr int kGridH = 7;
constexpr int kMinGates = 3;

int cellIndex(int x, int y) { return y * kGridW + x; }
bool inBounds(int x, int y) { return x >= 0 && y >= 0 && x < kGridW && y < kGridH; }

// Sorted id pools (sorted for cross-run determinism since the DB is unordered).
struct Pools {
    std::vector<std::string> normalEnemies;
    std::vector<std::string> eliteEnemies;
    std::vector<std::string> items;
};

Pools buildPools(const content::ContentDatabase& db, const content::DungeonThemeDef* theme) {
    Pools p;
    if (theme != nullptr) {
        for (const std::string& id : theme->normalEnemies) {
            if (db.findEnemy(id) != nullptr) {
                p.normalEnemies.push_back(id);
            }
        }
        for (const std::string& id : theme->eliteEnemies) {
            if (db.findEnemy(id) != nullptr) {
                p.eliteEnemies.push_back(id);
            }
        }
    }
    if (p.normalEnemies.empty()) {
        // Fallback: every enemy in the database, split by tier.
        for (const auto& [id, def] : db.enemies()) {
            if (def.tier == content::EnemyTier::Elite) {
                p.eliteEnemies.push_back(id);
            } else {
                p.normalEnemies.push_back(id);
            }
        }
    }
    for (const auto& [id, def] : db.items()) {
        (void)def;
        p.items.push_back(id);
    }
    std::sort(p.normalEnemies.begin(), p.normalEnemies.end());
    std::sort(p.eliteEnemies.begin(), p.eliteEnemies.end());
    std::sort(p.items.begin(), p.items.end());
    return p;
}

const content::BossDef* pickBoss(Rng& rng, const content::DungeonThemeDef* theme,
                                 const content::ContentDatabase& db) {
    std::vector<std::string> ids;
    if (theme != nullptr) {
        for (const std::string& id : theme->bosses) {
            if (db.findBoss(id) != nullptr) {
                ids.push_back(id);
            }
        }
    }
    if (ids.empty()) {
        for (const auto& [id, def] : db.bosses()) {
            (void)def;
            ids.push_back(id);
        }
    }
    if (ids.empty()) {
        return nullptr;
    }
    std::sort(ids.begin(), ids.end());
    return db.findBoss(ids[static_cast<std::size_t>(rng.range(0, static_cast<int>(ids.size()) - 1))]);
}

std::string teamName(Rng& rng, bool boss) {
    static const char* const adjectives[] = {"Snarling", "Hollow", "Iron",  "Ashen",
                                              "Grim",     "Feral",  "Cinder", "Murk"};
    static const char* const nouns[] = {"Pack",      "Sentinels", "Horde",  "Wardens",
                                        "Marauders", "Cohort",    "Fangs",  "Brood"};
    static const char* const bossNouns[] = {"Warden", "Tyrant", "Devourer", "Colossus", "Revenant"};

    const std::string adj = adjectives[rng.range(0, 7)];
    if (boss) {
        return "The " + adj + " " + bossNouns[rng.range(0, 4)];
    }
    return adj + " " + nouns[rng.range(0, 7)];
}

void addTags(const content::ContentDatabase& db, const std::string& enemyId,
             std::vector<std::string>& tags) {
    const content::EnemyDef* def = db.findEnemy(enemyId);
    if (def == nullptr) {
        return;
    }
    for (content::EnemyTag tag : def->tags) {
        std::string name = content::toString(tag);
        if (std::find(tags.begin(), tags.end(), name) == tags.end()) {
            tags.push_back(name);
        }
    }
}

EnemyTeam makeTeam(Rng& rng, const Pools& pools, int depth, bool boss,
                   const content::ContentDatabase& db) {
    EnemyTeam team;
    team.isBoss = boss;

    const auto& normals = pools.normalEnemies;
    const auto& elites = pools.eliteEnemies.empty() ? pools.normalEnemies : pools.eliteEnemies;

    if (boss) {
        if (!elites.empty()) {
            team.enemyIds.push_back(elites[static_cast<std::size_t>(rng.range(
                0, static_cast<int>(elites.size()) - 1))]);
        }
    } else {
        const int size = rng.range(2, std::min(5, 2 + depth));
        const int eliteChance = std::min(50, depth * 12);
        for (int i = 0; i < size; ++i) {
            const bool elite = !pools.eliteEnemies.empty() && rng.chance(eliteChance);
            const auto& pool = elite ? pools.eliteEnemies : normals;
            if (pool.empty()) {
                continue;
            }
            team.enemyIds.push_back(
                pool[static_cast<std::size_t>(rng.range(0, static_cast<int>(pool.size()) - 1))]);
        }
    }

    for (const std::string& id : team.enemyIds) {
        addTags(db, id, team.tags);
    }
    team.name = teamName(rng, boss);
    return team;
}

Chest makeChest(Rng& rng, bool guarded, int depth, const Pools& pools,
                const content::ContentDatabase& db) {
    Chest c;
    c.present = true;
    c.guarded = guarded;
    c.gold = rng.range(10, 30) * std::max(1, depth) + (guarded ? 40 : 0);

    const bool giveItem = guarded || rng.chance(40);
    if (giveItem && !pools.items.empty()) {
        c.itemId =
            pools.items[static_cast<std::size_t>(rng.range(0, static_cast<int>(pools.items.size()) - 1))];
        if (const content::ItemDef* it = db.findItem(c.itemId)) {
            c.rarity = content::toString(it->rarity);
        }
    }
    return c;
}

Dir directionBetween(const Room& from, const Room& to) {
    const int dx = to.gridX - from.gridX;
    const int dy = to.gridY - from.gridY;
    if (dx == 1) return Dir::East;
    if (dx == -1) return Dir::West;
    if (dy == 1) return Dir::South;
    return Dir::North;
}

void connect(Dungeon& d, int a, int b) {
    const Dir dir = directionBetween(d.rooms[a], d.rooms[b]);
    d.rooms[a].door(dir).neighbor = b;
    d.rooms[b].door(opposite(dir)).neighbor = a;
}

}  // namespace

Dungeon generate(std::uint64_t seed, int depth, const content::ContentDatabase& db,
                 std::string themeId) {
    Rng rng(seed);
    const content::DungeonThemeDef* theme = themeId.empty() ? nullptr : db.findTheme(themeId);
    const Pools pools = buildPools(db, theme);

    Dungeon d;
    d.seed = seed;
    d.depth = depth < 1 ? 1 : depth;
    d.themeName = theme != nullptr ? theme->name : "Dungeon";
    d.gridW = kGridW;
    d.gridH = kGridH;

    std::vector<int> grid(static_cast<std::size_t>(kGridW * kGridH), -1);

    // --- Main path: randomized DFS producing a simple path of a target length.
    const int targetLen = std::clamp(5 + d.depth, 6, 9);
    std::vector<std::pair<int, int>> path;
    std::vector<bool> visited(static_cast<std::size_t>(kGridW * kGridH), false);

    std::function<bool(int, int, int)> walk = [&](int x, int y, int remaining) -> bool {
        visited[static_cast<std::size_t>(cellIndex(x, y))] = true;
        path.emplace_back(x, y);
        if (remaining == 1) {
            return true;
        }
        std::array<Dir, 4> dirs{Dir::North, Dir::East, Dir::South, Dir::West};
        for (int i = 3; i > 0; --i) {
            std::swap(dirs[static_cast<std::size_t>(i)],
                      dirs[static_cast<std::size_t>(rng.range(0, i))]);
        }
        for (Dir dir : dirs) {
            const int nx = x + dirDx(dir);
            const int ny = y + dirDy(dir);
            if (inBounds(nx, ny) && !visited[static_cast<std::size_t>(cellIndex(nx, ny))]) {
                if (walk(nx, ny, remaining - 1)) {
                    return true;
                }
            }
        }
        visited[static_cast<std::size_t>(cellIndex(x, y))] = false;
        path.pop_back();
        return false;
    };

    const int startX = rng.range(1, kGridW - 2);
    const int startY = rng.range(1, kGridH - 2);
    walk(startX, startY, targetLen);

    // Create the path rooms.
    for (const auto& [x, y] : path) {
        Room r;
        r.gridX = x;
        r.gridY = y;
        const int index = static_cast<int>(d.rooms.size());
        grid[static_cast<std::size_t>(cellIndex(x, y))] = index;
        d.rooms.push_back(r);
        d.mainPath.push_back(index);
    }
    d.startRoom = d.mainPath.front();
    d.bossRoom = d.mainPath.back();
    d.rooms[static_cast<std::size_t>(d.startRoom)].type = RoomType::Start;
    d.rooms[static_cast<std::size_t>(d.bossRoom)].type = RoomType::Boss;

    for (std::size_t i = 0; i + 1 < d.mainPath.size(); ++i) {
        connect(d, d.mainPath[i], d.mainPath[i + 1]);
    }

    // --- Gates: choose >= kMinGates path transitions (skip the very first).
    std::vector<int> transitions;
    for (int i = 1; i + 1 < static_cast<int>(d.mainPath.size()); ++i) {
        transitions.push_back(i);
    }
    // Include the final transition into the boss as gateable too.
    transitions.push_back(static_cast<int>(d.mainPath.size()) - 2);
    std::sort(transitions.begin(), transitions.end());
    transitions.erase(std::unique(transitions.begin(), transitions.end()), transitions.end());
    for (int i = static_cast<int>(transitions.size()) - 1; i > 0; --i) {
        std::swap(transitions[static_cast<std::size_t>(i)],
                  transitions[static_cast<std::size_t>(rng.range(0, i))]);
    }
    const int gateCount = std::min(static_cast<int>(transitions.size()), std::max(kMinGates, depth));
    for (int g = 0; g < gateCount; ++g) {
        const int t = transitions[static_cast<std::size_t>(g)];
        const int ra = d.mainPath[static_cast<std::size_t>(t)];
        const int rb = d.mainPath[static_cast<std::size_t>(t + 1)];
        const int teamIdx = static_cast<int>(d.teams.size());
        d.teams.push_back(makeTeam(rng, pools, d.depth, false, db));
        const Dir dir = directionBetween(d.rooms[ra], d.rooms[rb]);
        d.rooms[ra].door(dir).gated = true;
        d.rooms[ra].door(dir).teamIndex = teamIdx;
        d.rooms[rb].door(opposite(dir)).gated = true;
        d.rooms[rb].door(opposite(dir)).teamIndex = teamIdx;
    }
    d.mandatoryGates = gateCount;

    // --- Boss team (from the theme's boss pool; minions accompany the boss).
    {
        const int teamIdx = static_cast<int>(d.teams.size());
        EnemyTeam bossTeam;
        bossTeam.isBoss = true;
        if (const content::BossDef* boss = pickBoss(rng, theme, db)) {
            bossTeam.bossId = boss->id;
            bossTeam.name = boss->name;
            bossTeam.enemyIds = boss->minions;
            for (const std::string& id : bossTeam.enemyIds) {
                addTags(db, id, bossTeam.tags);
            }
        } else {
            bossTeam = makeTeam(rng, pools, d.depth, true, db);  // fallback: strong elite
        }
        d.teams.push_back(std::move(bossTeam));
        d.rooms[static_cast<std::size_t>(d.bossRoom)].teamIndex = teamIdx;
    }

    // --- Side rooms with chests (dead-ends off the main path).
    const int desiredSideRooms = rng.range(2, 4);
    int created = 0;
    for (std::size_t pi = 0; pi < d.mainPath.size() && created < desiredSideRooms; ++pi) {
        const int parent = d.mainPath[pi];
        if (d.rooms[static_cast<std::size_t>(parent)].type == RoomType::Boss) {
            continue;
        }
        // Collect free neighbor cells.
        std::vector<Dir> freeDirs;
        for (Dir dir : {Dir::North, Dir::East, Dir::South, Dir::West}) {
            const int nx = d.rooms[static_cast<std::size_t>(parent)].gridX + dirDx(dir);
            const int ny = d.rooms[static_cast<std::size_t>(parent)].gridY + dirDy(dir);
            if (inBounds(nx, ny) && grid[static_cast<std::size_t>(cellIndex(nx, ny))] == -1) {
                freeDirs.push_back(dir);
            }
        }
        if (freeDirs.empty()) {
            continue;
        }
        const Dir chosen = freeDirs[static_cast<std::size_t>(rng.range(
            0, static_cast<int>(freeDirs.size()) - 1))];
        const int nx = d.rooms[static_cast<std::size_t>(parent)].gridX + dirDx(chosen);
        const int ny = d.rooms[static_cast<std::size_t>(parent)].gridY + dirDy(chosen);

        Room side;
        side.gridX = nx;
        side.gridY = ny;
        side.type = RoomType::Treasure;
        const int sideIndex = static_cast<int>(d.rooms.size());
        grid[static_cast<std::size_t>(cellIndex(nx, ny))] = sideIndex;
        d.rooms.push_back(side);
        connect(d, parent, sideIndex);

        // Guarantee the first side room is guarded so a guarded chest exists.
        const bool guarded = (created == 0) ? true : rng.chance(45);
        d.rooms[static_cast<std::size_t>(sideIndex)].chest =
            makeChest(rng, guarded, d.depth, pools, db);
        if (guarded) {
            const int teamIdx = static_cast<int>(d.teams.size());
            d.teams.push_back(makeTeam(rng, pools, d.depth, false, db));
            d.rooms[static_cast<std::size_t>(sideIndex)].teamIndex = teamIdx;
        }
        ++created;
    }

    return d;
}

}  // namespace cd::dungeon
