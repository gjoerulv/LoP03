#include "dungeon/DungeonGenerator.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <utility>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Enums.hpp"
#include "dungeon/Rng.hpp"
#include "dungeon/ThemeEvents.hpp"  // M55 per-theme rites (themeEventKind, prices)
#include "game/BlackMarket.hpp"  // blackMarketHash — the shared SplitMix64 (M82 floors)
#include "game/Castle.hpp"  // kDragonBossId (M85: the fallback-sweep exclusion)
#include "game/Relics.hpp"  // relicEventChancePct (M44)
#include "game/WorldLadder.hpp"

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

Pools buildPools(const content::ContentDatabase& db, const content::DungeonThemeDef* theme,
                 int town) {
    Pools p;
    // Per-town gating (M38): an enemy spawns only once town >= its minTown.
    // M49: a `bossOnly` enemy (the Royal Guards) is never generated at all — it
    // exists solely in a boss's minion list. Checked on BOTH paths below,
    // including a theme that names one explicitly, so the rule cannot be
    // sidestepped by content.
    const auto spawnable = [town](const content::EnemyDef* def) {
        // M111: a specialOnly foe (the Golden Goose) is likewise never pooled.
        return def != nullptr && !def->bossOnly && !def->specialOnly && def->minTown <= town;
    };
    if (theme != nullptr) {
        for (const std::string& id : theme->normalEnemies) {
            if (spawnable(db.findEnemy(id))) {
                p.normalEnemies.push_back(id);
            }
        }
        for (const std::string& id : theme->eliteEnemies) {
            if (spawnable(db.findEnemy(id))) {
                p.eliteEnemies.push_back(id);
            }
        }
    }
    if (p.normalEnemies.empty()) {
        // Fallback: every enemy in the database (town-gated), split by tier.
        for (const auto& [id, def] : db.enemies()) {
            if (!spawnable(&def)) {
                continue;
            }
            if (def.tier == content::EnemyTier::Elite) {
                p.eliteEnemies.push_back(id);
            } else {
                p.normalEnemies.push_back(id);
            }
        }
    }
    for (const auto& [id, def] : db.items()) {
        // Legendary gear is black-market only (M34): never a chest/merchant drop.
        if (def.rarity == content::Rarity::Legendary) {
            continue;
        }
        // Per-town gating (M37, windowed in M43): a chest reward or merchant
        // offer only holds what exists at this town, so higher-town gear appears
        // only in higher-town dungeons and a town-1-only item stays there.
        if (!def.availableAtTown(town)) {
            continue;
        }
        // M44: a valueless item is never loot either — the Royal Relics come from
        // their own event and nowhere else.
        if (def.value <= 0) {
            continue;
        }
        // M102 (owner decision, generation v18): skill scrolls left the dungeon
        // shelves — no chest and no peddler offers one. Scrolls now come from
        // the Guild's trove (M92), the town treasure digs (M65/M83), and the
        // one sanctioned gamble (the M104 reels event). Nothing else.
        if (def.type == content::ItemType::Scroll) {
            continue;
        }
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
                                 const content::ContentDatabase& db, int town) {
    std::vector<std::string> ids;
    // Per-town gating (M38): a boss is chosen only once town >= its minTown.
    if (theme != nullptr) {
        for (const std::string& id : theme->bosses) {
            const content::BossDef* def = db.findBoss(id);
            if (def != nullptr && def->minTown <= town) {
                ids.push_back(id);
            }
        }
    }
    if (ids.empty()) {
        for (const auto& [id, def] : db.bosses()) {
            // M84: a Guild Master presides over its town's gauntlet, never a
            // dungeon — the fallback sweep must skip it or adding one would
            // change what existing seeds generate. M85: the Dragon's arena is
            // behind the twelve curios, same rule.
            // M112: a special-encounter boss (the Mimic) lives in no sweep.
            if (def.minTown <= town && def.guildTown == 0 && !def.specialOnly &&
                id != kDragonBossId) {
                ids.push_back(id);
            }
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
                   const content::ContentDatabase& db, int town) {
    const content::CompositionDef& comp = db.composition();
    EnemyTeam team;
    team.isBoss = boss;
    team.statScalePct = combineTownScale(100 + comp.statScalePct(depth), town);

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
                 std::string themeId, int town) {
    // Standalone single floor: no fog, so no Surveyor (the owner's rule —
    // the event exists only where the map starts fogged).
    return generate(seed, depth, db, std::move(themeId), town, FloorContext{});
}

Dungeon generate(std::uint64_t seed, int depth, const content::ContentDatabase& db,
                 std::string themeId, int town, const FloorContext& ctx) {
    Rng rng(seed);
    const content::DungeonThemeDef* theme = themeId.empty() ? nullptr : db.findTheme(themeId);
    const int townIdx = clampTown(town);
    const Pools pools = buildPools(db, theme, townIdx);  // M37: chest gear gated by town

    Dungeon d;
    d.seed = seed;
    d.runSeed = seed;  // M82: floors 1+ of a multi-floor run override this
    d.depth = depth < 1 ? 1 : depth;
    d.town = townIdx;
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
        d.teams.push_back(makeTeam(rng, pools, d.depth, false, db, townIdx));
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
        bossTeam.statScalePct =
            combineTownScale(100 + db.composition().statScalePct(d.depth), townIdx);
        if (const content::BossDef* boss = pickBoss(rng, theme, db, townIdx)) {
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
            bossTeam = makeTeam(rng, pools, d.depth, true, db, townIdx);  // fallback: strong elite
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
            d.teams.push_back(makeTeam(rng, pools, d.depth, false, db, townIdx));
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
        bool relicPlaced = false;  // M44: at most one relic event per dungeon
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
            // The M55 rite used to be FORCED onto the first slot here — every
            // floor of a themed run opened with its rite. Owner direction
            // 2026-08-17 (generation v22): the rite now rolls in the pure-hash
            // replacement pass below at the same level as every other special
            // event, so the base roll and the relic draw run on every slot.
            // Royal Relic (M44): a rare replacement of the rolled event, at most
            // one per dungeon. The draw is taken only where the event is eligible
            // (town >= 2, depth >= 2), from this same seeded stream.
            if (!relicPlaced) {
                const int relicPct = relicEventChancePct(townIdx, d.depth);
                if (relicPct > 0 && rng.chance(relicPct)) {
                    ev.kind = RoomEventKind::RoyalRelic;
                    relicPlaced = true;
                }
            }
            switch (ev.kind) {
                case RoomEventKind::Shrine:
                    ev.goldCost = 40 + 20 * d.depth;
                    break;
                case RoomEventKind::Merchant:
                    if (!pools.consumables.empty()) {
                        ev.itemId = pools.consumables[static_cast<std::size_t>(rng.range(
                            0, static_cast<int>(pools.consumables.size()) - 1))];
                        if (const content::ItemDef* it = db.findItem(ev.itemId)) {
                            ev.goldCost = it->value * 75 / 100;  // M37: a bargain, not a markup
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
                        ct.statScalePct =
                            combineTownScale(100 + db.composition().statScalePct(d.depth), townIdx);
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
                case RoomEventKind::ElderRoot:  // M55: town-scaled price paid for XP
                    ev.goldCost = elderRootPrice(townIdx, d.depth);
                    break;
                case RoomEventKind::MinersCache:  // M55: a guaranteed item baked in now
                    if (!pools.items.empty()) {
                        ev.itemId = pools.items[static_cast<std::size_t>(rng.range(
                            0, static_cast<int>(pools.items.size()) - 1))];
                    }
                    break;
                case RoomEventKind::HealingSpring:
                case RoomEventKind::ScoreWager:
                case RoomEventKind::RestToken:
                case RoomEventKind::ArmoryGhost:  // M55: the upgrade is picked at trade time
                case RoomEventKind::RoyalRelic:  // the relic is picked at resolution (M44)
                case RoomEventKind::DuckPeddler:  // M76: placed by the post-pass, never rolled here
                case RoomEventKind::None:
                    break;
            }
            d.rooms[static_cast<std::size_t>(sideIndex)].event = ev;
            ++made;
        }
    }

    // --- The encounter tier (owner direction 2026-08-28, generation v23).
    // Every encounter event — the theme's rite plus the ten global kinds in
    // encounterRegistry() — rolls the SAME kEncounterChancePct from its own
    // salt pair: a PURE hash of the floor seed, so no rng draw is consumed
    // and reloads can never reroll (the M76 peddler contract, now the tier's
    // contract). When more encounters fire than plain slots remain, the
    // survivors are a uniform hash-shuffle of the fired list — no fixed
    // order, no kind outranking another (the pre-v23 chain gave the Duckling
    // Peddler ~3x a gambling den's effective rate). Encounters replace only
    // plain STAPLE events: never the relic, an elite challenge (whose team
    // would be orphaned), the Surveyor, or each other. Whether the peddler
    // DEALS stays an interaction-time question (one per customer — see
    // DungeonState), so the dungeon a seed generates never depends on the
    // party's bag.
    {
        std::vector<int> plainEventRooms;
        const auto recollectPlain = [&]() {
            plainEventRooms.clear();
            for (std::size_t ri = 0; ri < d.rooms.size(); ++ri) {
                const RoomEventKind k = d.rooms[ri].event.kind;
                if (d.rooms[ri].type != RoomType::Event) {
                    continue;
                }
                if (k == RoomEventKind::Shrine || k == RoomEventKind::HealingSpring ||
                    k == RoomEventKind::Merchant || k == RoomEventKind::ScoreWager ||
                    k == RoomEventKind::RestToken) {
                    plainEventRooms.push_back(static_cast<int>(ri));
                }
            }
        };

        // The Surveyor first (M93; fog-gated per the owner's 2026-08-28
        // ruling): a utility purchase outside the tier, drawn BEFORE it so
        // its 25% no longer starves at the back of the old chain. It rolls
        // from the RUN seed + floor index, so every floor of a run answers
        // independently and the floor's own stream stays untouched.
        if (ctx.fogged) {
            recollectPlain();
            const int slot = surveyorSlot(ctx.runSeed, ctx.floorIndex,
                                          static_cast<int>(plainEventRooms.size()));
            if (slot >= 0) {
                RoomEvent& ev =
                    d.rooms[static_cast<std::size_t>(
                                plainEventRooms[static_cast<std::size_t>(slot)])]
                        .event;
                ev.kind = RoomEventKind::Surveyor;
                ev.goldCost = kSurveyorPriceGold;
                ev.itemId.clear();
            }
        }

        // Roll every encounter, shuffle the fired list, then hand out the
        // surviving plain slots from the front.
        std::vector<EncounterDef> fired;
        const EncounterDef rite = themeRiteEncounter(d.themeId);
        if (encounterFires(seed, rite)) {
            fired.push_back(rite);
        }
        for (const EncounterDef& e : encounterRegistry()) {
            if (encounterFires(seed, e)) {
                fired.push_back(e);
            }
        }
        encounterContentionShuffle(seed, fired);
        for (const EncounterDef& e : fired) {
            recollectPlain();
            if (plainEventRooms.empty()) {
                break;  // starved: the shuffle already made survival uniform,
                        // so no kind is systematically the one left out
            }
            const int slot =
                encounterPick(seed, e, static_cast<int>(plainEventRooms.size()));
            const int roomIdx = plainEventRooms[static_cast<std::size_t>(slot)];
            RoomEvent& ev = d.rooms[static_cast<std::size_t>(roomIdx)].event;
            ev.kind = e.kind;
            ev.goldCost = 0;
            ev.itemId.clear();
            // Payload baking, per kind — exactly the pre-v23 rules.
            if (e.kind == RoomEventKind::ElderRoot) {
                ev.goldCost = elderRootPrice(townIdx, d.depth);
            } else if (e.kind == RoomEventKind::MinersCache && !pools.items.empty()) {
                constexpr std::uint64_t kSaltRiteItem = 0x217E5E77E0090902ull;
                ev.itemId = pools.items[static_cast<std::size_t>(
                    themeEventHash(seed, roomIdx, kSaltRiteItem) % pools.items.size())];
            } else if (e.kind == RoomEventKind::DuckPeddler) {
                ev.goldCost = kDuckPeddlerPriceGold;
                ev.itemId = kEvilDucklingItemId;
            }
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

    // --- Secret Map Piece (M65, generation v12): ~10% of dungeons hide one
    // piece in a plain Normal room. Decided by a PURE hash of the seed (its
    // own salts — never the generator Rng), so every other roll of a seed is
    // byte-identical to v11; the piece is strictly new output.
    {
        constexpr int kMapPieceChancePct = 10;
        constexpr std::uint64_t kSaltMapPieceRoll = 0x4D41505031ull;  // "MAPP1"
        constexpr std::uint64_t kSaltMapPiecePick = 0x4D41505032ull;
        if (themeEventHash(d.seed, 0, kSaltMapPieceRoll) % 100 <
            static_cast<std::uint64_t>(kMapPieceChancePct)) {
            std::vector<int> plain;
            for (std::size_t i = 0; i < d.rooms.size(); ++i) {
                if (d.rooms[i].type == RoomType::Normal) {
                    plain.push_back(static_cast<int>(i));
                }
            }
            if (!plain.empty()) {
                d.mapPieceRoom = plain[static_cast<std::size_t>(
                    themeEventHash(d.seed, 1, kSaltMapPiecePick) % plain.size())];
            }
        }
    }

    // --- Dungeon treasure map (M66, generation v13): ~12% of dungeons hide a
    // CHART in one Normal room pointing at treasure BURIED in another. Same
    // pure-hash contract as the map piece; the two rooms are always distinct
    // and never the map-piece room (its marker owns that room's center tile).
    {
        constexpr int kChartChancePct = 12;
        constexpr std::uint64_t kSaltChartRoll = 0xC4A47001ull;
        constexpr std::uint64_t kSaltChartPick = 0xC4A47002ull;
        constexpr std::uint64_t kSaltBuriedPick = 0xC4A47003ull;
        if (themeEventHash(d.seed, 2, kSaltChartRoll) % 100 <
            static_cast<std::uint64_t>(kChartChancePct)) {
            std::vector<int> plain;
            for (std::size_t i = 0; i < d.rooms.size(); ++i) {
                if (d.rooms[i].type == RoomType::Normal &&
                    static_cast<int>(i) != d.mapPieceRoom) {
                    plain.push_back(static_cast<int>(i));
                }
            }
            if (plain.size() >= 2) {
                const std::size_t chartAt = static_cast<std::size_t>(
                    themeEventHash(d.seed, 3, kSaltChartPick) % plain.size());
                d.chartRoom = plain[chartAt];
                plain.erase(plain.begin() + static_cast<std::ptrdiff_t>(chartAt));
                d.buriedRoom = plain[static_cast<std::size_t>(
                    themeEventHash(d.seed, 4, kSaltBuriedPick) % plain.size())];
            }
        }
    }

    return d;
}

std::uint64_t floorSeed(std::uint64_t runSeed, int floorIndex) {
    // Floor 0 IS the run seed, so a 1-floor run (and the first floor of a
    // 4-floor run) generates byte-identically to what that seed always meant.
    // Deeper floors derive an independent stream from the same SplitMix64
    // finalizer the black market and boss drops trust (a pure function — no
    // Rng draw anywhere, so nothing else about a seed shifts).
    if (floorIndex <= 0) {
        return runSeed;
    }
    constexpr std::uint64_t kSaltFloor = 0xF100F5EEDull;  // M82 floor stream
    return blackMarketHash(runSeed, kSaltFloor + static_cast<std::uint64_t>(floorIndex));
}

std::vector<Dungeon> generateFloors(std::uint64_t seed, int depth,
                                    const content::ContentDatabase& db, std::string themeId,
                                    int town, int floorCount) {
    if (floorCount < 1) {
        floorCount = 1;
    }
    std::vector<Dungeon> floors;
    floors.reserve(static_cast<std::size_t>(floorCount));
    for (int i = 0; i < floorCount; ++i) {
        // v23: the context tells the event pass this floor's map starts
        // fogged (multi-floor descents only today — the owner's rule is the
        // fog, not the floor count) so the Surveyor can roll, from the RUN
        // seed so every floor answers independently.
        const FloorContext ctx{seed, i, /*fogged=*/floorCount > 1};
        Dungeon f = generate(floorSeed(seed, i), depth, db, themeId, town, ctx);
        f.runSeed = seed;
        f.floorIndex = i;
        f.floorCount = floorCount;

        // Floors before the last swap the boss for an elite STAIR-GATE team.
        // The swap is a post-pass drawing from a FRESH pure-hash Rng — zero
        // draws from the floor's own generation stream — so everything else
        // about the floor (layout, gates, chests, events) is byte-identical
        // to what its sub-seed generates standalone.
        if (i + 1 < floorCount) {
            const int bossTeamIdx =
                f.rooms[static_cast<std::size_t>(f.bossRoom)].teamIndex;
            if (bossTeamIdx >= 0) {
                constexpr std::uint64_t kSaltStairGate = 0x57A1B6A7Eull;  // M82
                Rng gateRng(blackMarketHash(
                    seed, kSaltStairGate + static_cast<std::uint64_t>(i)));
                const content::DungeonThemeDef* theme =
                    themeId.empty() ? nullptr : db.findTheme(themeId);
                Pools pools = buildPools(db, theme, clampTown(town));
                // All-elite slots: the gate is the floor's climax, so every
                // pick comes from the elite pool (falling back to normals
                // only when a theme has no elites at this town).
                if (!pools.eliteEnemies.empty()) {
                    pools.normalEnemies = pools.eliteEnemies;
                }
                EnemyTeam gate =
                    makeTeam(gateRng, pools, f.depth, /*boss=*/false, db, clampTown(town));
                gate.name = "Stairway Wardens";  // a recognizable fixture, floor to floor
                f.teams[static_cast<std::size_t>(bossTeamIdx)] = std::move(gate);
            }
        }

        floors.push_back(std::move(f));
    }
    return floors;
}

Dungeon generateEternalFloor(std::uint64_t runSeed, int floorIndex,
                             const content::ContentDatabase& db,
                             const std::string& themeId, int town) {
    // M105: the standalone generation of this floor's sub-seed at the fixed
    // Eternal depth — which already carries its REAL theme boss, exactly what
    // Eternal wants on every floor (no warden swap ever). Eternal maps are
    // always fogged, so the context keeps the Surveyor sellable (v23: it
    // draws inside the event pass now, before the encounter tier).
    const FloorContext ctx{runSeed, floorIndex, /*fogged=*/true};
    Dungeon f = generate(floorSeed(runSeed, floorIndex), kEternalDepth, db, themeId, town, ctx);
    f.runSeed = runSeed;
    f.floorIndex = floorIndex;
    f.floorCount = kEternalFloorCountSentinel;
    f.eternal = true;
    f.mapPieceRoom = -1;  // an endless run feeds no map economy (owner rule)
    // The escalation: a flat +10 %pts on every team per floor past the first
    // (the M49 Endless Rush curve shape). Danger tiers recompute from these
    // scaled stats, so the labels never lie about what stands there.
    if (floorIndex > 0) {
        for (EnemyTeam& t : f.teams) {
            t.statScalePct += kEternalEscalationPctPts * floorIndex;
        }
    }
    return f;
}

EnemyTeam patrolTeam(const content::ContentDatabase& db, const std::string& themeId,
                     int town, int depth, std::uint64_t runSeed, int patrolIndex) {
    // M93: the stair-gate recipe (fresh pure-hash Rng, the real pools, the
    // real composer) at the run's own depth/town — "follows the same rules
    // as depth + town + dungeon type" (owner decision 5), verbatim.
    constexpr std::uint64_t kSaltPatrol = 0x9A7201CCA11ull;
    Rng rng(blackMarketHash(runSeed, kSaltPatrol + static_cast<std::uint64_t>(patrolIndex)));
    const content::DungeonThemeDef* theme = themeId.empty() ? nullptr : db.findTheme(themeId);
    const Pools pools = buildPools(db, theme, clampTown(town));
    EnemyTeam team = makeTeam(rng, pools, depth, /*boss=*/false, db, clampTown(town));
    team.name = "Roused Patrol";
    team.patrol = true;  // XP only, no gold, no danger credit (owner decision 7)
    return team;
}

EnemyTeam goldenGooseTeam(const content::ContentDatabase& db, const std::string& themeId,
                          int town, int depth, std::uint64_t runSeed, int patrolIndex) {
    EnemyTeam team;
    if (db.findEnemy(kGoldenGooseEnemyId) == nullptr) {
        return team;
    }
    // The shadow patrol: the ordinary team this goose replaces, for its scale
    // and its XP (the same pure recipe, so a reload derives the same numbers).
    const EnemyTeam shadow = patrolTeam(db, themeId, town, depth, runSeed, patrolIndex);
    team.name = "Golden Goose";
    team.enemyIds = {kGoldenGooseEnemyId};
    team.statScalePct = shadow.statScalePct;
    team.patrol = true;
    team.patrolPaysGold = true;
    int xp = 0;
    for (const std::string& id : shadow.enemyIds) {
        if (const content::EnemyDef* def = db.findEnemy(id)) {
            xp += def->xpReward;
        }
    }
    team.xpOverride = xp > 0 ? xp : 1;
    return team;
}

}  // namespace cd::dungeon
