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
    std::vector<std::string> consumables;  // merchant offers (M20)
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
        p.items.push_back(id);
        if (def.type == content::ItemType::Consumable) {
            p.consumables.push_back(id);
        }
    }
    std::sort(p.normalEnemies.begin(), p.normalEnemies.end());
    std::sort(p.eliteEnemies.begin(), p.eliteEnemies.end());
    std::sort(p.items.begin(), p.items.end());
    std::sort(p.consumables.begin(), p.consumables.end());
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

// Constraint-aware slot filling (M20): candidates come from the tier pool
// filtered by the composition rules — support roles capped per team, and
// the trailing slots reserved for damage roles until the minimum is met.
// Filtering preserves the sorted pool order, so composition stays fully
// deterministic for a seed.
std::vector<std::string> slotCandidates(const std::vector<std::string>& pool,
                                        const content::ContentDatabase& db,
                                        const content::CompositionDef& comp, int slotsLeft,
                                        int supportCount, int damageCount) {
    std::vector<std::string> out;
    const bool needDamage = comp.minDamage - damageCount >= slotsLeft;
    for (const std::string& id : pool) {
        const content::EnemyDef* def = db.findEnemy(id);
        if (def == nullptr) {
            continue;
        }
        if (content::isSupportRole(def->role) && supportCount >= comp.maxSupport) {
            continue;
        }
        if (needDamage && !content::isDamageRole(def->role)) {
            continue;
        }
        out.push_back(id);
    }
    return out;
}

EnemyTeam makeTeam(Rng& rng, const Pools& pools, int depth, bool boss,
                   const content::ContentDatabase& db) {
    const content::CompositionDef& comp = db.composition();
    EnemyTeam team;
    team.isBoss = boss;
    team.statScalePct = 100 + comp.statScalePct(depth);

    const auto& normals = pools.normalEnemies;
    const auto& elites = pools.eliteEnemies.empty() ? pools.normalEnemies : pools.eliteEnemies;

    if (boss) {
        if (!elites.empty()) {
            team.enemyIds.push_back(elites[static_cast<std::size_t>(rng.range(
                0, static_cast<int>(elites.size()) - 1))]);
        }
    } else {
        const int size = rng.range(comp.teamSizeMin(depth), comp.teamSizeMax(depth));
        const int eliteChance = comp.eliteChancePct(depth);
        int supportCount = 0;
        int damageCount = 0;
        for (int i = 0; i < size; ++i) {
            const bool elite = !pools.eliteEnemies.empty() && rng.chance(eliteChance);
            const auto& pool = elite ? pools.eliteEnemies : normals;
            if (pool.empty()) {
                continue;
            }
            std::vector<std::string> candidates =
                slotCandidates(pool, db, comp, size - i, supportCount, damageCount);
            if (candidates.empty() && elite) {
                // The elite pool cannot satisfy the constraint (e.g. no
                // damage-role elites in this theme): fill from normals so the
                // rule holds rather than the tier.
                candidates = slotCandidates(normals, db, comp, size - i, supportCount,
                                            damageCount);
            }
            if (candidates.empty()) {
                candidates = pool;  // data gap: degrade rather than under-fill
            }
            const std::string& pick = candidates[static_cast<std::size_t>(
                rng.range(0, static_cast<int>(candidates.size()) - 1))];
            if (const content::EnemyDef* def = db.findEnemy(pick)) {
                supportCount += content::isSupportRole(def->role) ? 1 : 0;
                damageCount += content::isDamageRole(def->role) ? 1 : 0;
            }
            team.enemyIds.push_back(pick);
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
    d.themeId = theme != nullptr ? theme->id : "";
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
        bossTeam.statScalePct = 100 + db.composition().statScalePct(d.depth);
        if (const content::BossDef* boss = pickBoss(rng, theme, db)) {
            bossTeam.bossId = boss->id;
            bossTeam.name = boss->name;
            bossTeam.enemyIds = boss->minions;
            const int maxMinions = db.composition().maxMinions;
            if (static_cast<int>(bossTeam.enemyIds.size()) > maxMinions) {
                bossTeam.enemyIds.resize(static_cast<std::size_t>(maxMinions));
            }
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

    // --- Event side rooms (M20): dead-end rooms offering visible decisions.
    {
        std::array<RoomEventKind, 6> kinds{RoomEventKind::Shrine, RoomEventKind::HealingSpring,
                                           RoomEventKind::Merchant, RoomEventKind::EliteChallenge,
                                           RoomEventKind::ScoreWager, RoomEventKind::RestToken};
        for (int i = 5; i > 0; --i) {
            std::swap(kinds[static_cast<std::size_t>(i)],
                      kinds[static_cast<std::size_t>(rng.range(0, i))]);
        }
        const int desiredEvents = rng.range(2, 3);
        int made = 0;
        for (std::size_t pi = 0; pi < d.mainPath.size() && made < desiredEvents; ++pi) {
            const int parent = d.mainPath[pi];
            if (d.rooms[static_cast<std::size_t>(parent)].type == RoomType::Boss) {
                continue;
            }
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
            side.type = RoomType::Event;
            const int sideIndex = static_cast<int>(d.rooms.size());
            grid[static_cast<std::size_t>(cellIndex(nx, ny))] = sideIndex;
            d.rooms.push_back(side);
            connect(d, parent, sideIndex);

            RoomEvent ev;
            ev.kind = kinds[static_cast<std::size_t>(made)];
            switch (ev.kind) {
                case RoomEventKind::Shrine:
                    ev.goldCost = 40 + 20 * d.depth;
                    break;
                case RoomEventKind::Merchant:
                    if (!pools.consumables.empty()) {
                        ev.itemId = pools.consumables[static_cast<std::size_t>(rng.range(
                            0, static_cast<int>(pools.consumables.size()) - 1))];
                        if (const content::ItemDef* it = db.findItem(ev.itemId)) {
                            ev.goldCost = it->value * 130 / 100;  // dungeon markup
                        }
                    } else {
                        ev.kind = RoomEventKind::HealingSpring;  // no stock: degrade
                    }
                    break;
                case RoomEventKind::EliteChallenge: {
                    const auto& elitePool =
                        pools.eliteEnemies.empty() ? pools.normalEnemies : pools.eliteEnemies;
                    if (!elitePool.empty()) {
                        EnemyTeam ct;
                        ct.statScalePct = 100 + db.composition().statScalePct(d.depth);
                        const int n = rng.range(1, 2);
                        for (int e = 0; e < n; ++e) {
                            ct.enemyIds.push_back(elitePool[static_cast<std::size_t>(rng.range(
                                0, static_cast<int>(elitePool.size()) - 1))]);
                        }
                        for (const std::string& id : ct.enemyIds) {
                            addTags(db, id, ct.tags);
                        }
                        ct.name = teamName(rng, false);
                        const int teamIdx = static_cast<int>(d.teams.size());
                        d.teams.push_back(std::move(ct));
                        d.rooms[static_cast<std::size_t>(sideIndex)].teamIndex = teamIdx;
                    } else {
                        ev.kind = RoomEventKind::ScoreWager;  // no elites: degrade
                    }
                    break;
                }
                case RoomEventKind::HealingSpring:
                case RoomEventKind::ScoreWager:
                case RoomEventKind::RestToken:
                case RoomEventKind::None:
                    break;
            }
            d.rooms[static_cast<std::size_t>(sideIndex)].event = ev;
            ++made;
        }
    }

    // --- Trapped chests (M20): some unguarded chests carry a visible
    // risk/reward trade — extra gold, but claiming wounds the party.
    for (Room& r : d.rooms) {
        if (r.chest.present && !r.chest.guarded && rng.chance(35)) {
            r.chest.trapped = true;
            r.chest.gold += 25 * d.depth + 15;
        }
    }

    return d;
}

}  // namespace cd::dungeon
