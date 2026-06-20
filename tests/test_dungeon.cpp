#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <queue>
#include <vector>

#include "dungeon/DungeonModel.hpp"
#include "dungeon/Rng.hpp"

#ifdef CRYSTAL_TEST_DATA_DIR
#include <filesystem>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#endif

using namespace cd::dungeon;

TEST_CASE("rng: deterministic sequence for a seed", "[dungeon]") {
    Rng a(12345);
    Rng b(12345);
    for (int i = 0; i < 16; ++i) {
        REQUIRE(a.next() == b.next());
    }
    Rng c(999);
    REQUIRE(c.range(1, 6) >= 1);
    REQUIRE(c.range(1, 6) <= 6);
    REQUIRE(c.range(5, 5) == 5);
}

#ifdef CRYSTAL_TEST_DATA_DIR

namespace {

cd::content::ContentDatabase loadContent() {
    cd::content::ContentDatabase db;
    cd::content::LoadReport rep;
    cd::content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

bool reachable(const Dungeon& d, int from, int to, bool throughGates) {
    std::vector<bool> visited(d.rooms.size(), false);
    std::queue<int> q;
    q.push(from);
    visited[static_cast<std::size_t>(from)] = true;
    while (!q.empty()) {
        const int r = q.front();
        q.pop();
        if (r == to) {
            return true;
        }
        for (Dir dir : {Dir::North, Dir::East, Dir::South, Dir::West}) {
            const Door& door = d.rooms[static_cast<std::size_t>(r)].door(dir);
            if (door.neighbor < 0 || (door.gated && !throughGates)) {
                continue;
            }
            if (!visited[static_cast<std::size_t>(door.neighbor)]) {
                visited[static_cast<std::size_t>(door.neighbor)] = true;
                q.push(door.neighbor);
            }
        }
    }
    return false;
}

bool sameLayout(const Dungeon& a, const Dungeon& b) {
    if (a.rooms.size() != b.rooms.size() || a.teams.size() != b.teams.size()) return false;
    if (a.startRoom != b.startRoom || a.bossRoom != b.bossRoom) return false;
    if (a.mandatoryGates != b.mandatoryGates || a.mainPath != b.mainPath) return false;
    for (std::size_t i = 0; i < a.rooms.size(); ++i) {
        const Room& ra = a.rooms[i];
        const Room& rb = b.rooms[i];
        if (ra.gridX != rb.gridX || ra.gridY != rb.gridY || ra.type != rb.type) return false;
        if (ra.chest.present != rb.chest.present || ra.chest.guarded != rb.chest.guarded ||
            ra.chest.gold != rb.chest.gold || ra.chest.itemId != rb.chest.itemId) {
            return false;
        }
        for (int dch = 0; dch < kDirCount; ++dch) {
            const Door& da = ra.doors[static_cast<std::size_t>(dch)];
            const Door& dbr = rb.doors[static_cast<std::size_t>(dch)];
            if (da.neighbor != dbr.neighbor || da.gated != dbr.gated) return false;
        }
    }
    for (std::size_t i = 0; i < a.teams.size(); ++i) {
        if (a.teams[i].name != b.teams[i].name || a.teams[i].enemyIds != b.teams[i].enemyIds) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("dungeon: generation is deterministic for a seed", "[dungeon]") {
    const cd::content::ContentDatabase db = loadContent();
    const Dungeon a = generate(424242, 1, db);
    const Dungeon b = generate(424242, 1, db);
    REQUIRE(sameLayout(a, b));
}

TEST_CASE("dungeon: has a start, a boss, and at least three mandatory gates", "[dungeon]") {
    const cd::content::ContentDatabase db = loadContent();
    for (std::uint64_t seed : {1ull, 2ull, 7ull, 100ull, 9999ull, 123456ull}) {
        const Dungeon d = generate(seed, 1, db);
        REQUIRE(d.startRoom != d.bossRoom);
        REQUIRE(d.rooms[static_cast<std::size_t>(d.startRoom)].type == RoomType::Start);
        REQUIRE(d.rooms[static_cast<std::size_t>(d.bossRoom)].type == RoomType::Boss);
        REQUIRE(d.mandatoryGates >= 3);
        REQUIRE(d.mainPath.front() == d.startRoom);
        REQUIRE(d.mainPath.back() == d.bossRoom);
    }
}

TEST_CASE("dungeon: the boss is reachable only by passing through gates", "[dungeon]") {
    const cd::content::ContentDatabase db = loadContent();
    for (std::uint64_t seed : {1ull, 42ull, 500ull, 77777ull}) {
        const Dungeon d = generate(seed, 1, db);
        REQUIRE(reachable(d, d.startRoom, d.bossRoom, /*throughGates=*/true));
        REQUIRE_FALSE(reachable(d, d.startRoom, d.bossRoom, /*throughGates=*/false));
    }
}

TEST_CASE("dungeon: has chests including at least one guarded", "[dungeon]") {
    const cd::content::ContentDatabase db = loadContent();
    for (std::uint64_t seed : {3ull, 88ull, 4040ull, 222222ull}) {
        const Dungeon d = generate(seed, 1, db);
        REQUIRE(d.chestCount() >= 1);
        REQUIRE(d.guardedChestCount() >= 1);
    }
}

TEST_CASE("dungeon: teams and chest rewards reference real content", "[dungeon]") {
    const cd::content::ContentDatabase db = loadContent();
    const Dungeon d = generate(31337, 2, db);
    for (const EnemyTeam& team : d.teams) {
        REQUIRE(team.count() >= 1);
        for (const std::string& id : team.enemyIds) {
            REQUIRE(db.findEnemy(id) != nullptr);
        }
    }
    for (const Room& room : d.rooms) {
        if (room.chest.present && !room.chest.itemId.empty()) {
            REQUIRE(db.findItem(room.chest.itemId) != nullptr);
        }
    }
}

TEST_CASE("dungeon: a themed run uses the theme's boss pool", "[dungeon]") {
    const cd::content::ContentDatabase db = loadContent();
    const Dungeon d = generate(424242, 1, db, "ruined_keep");

    const Room& bossRoom = d.rooms[static_cast<std::size_t>(d.bossRoom)];
    REQUIRE(bossRoom.teamIndex >= 0);
    const EnemyTeam& bossTeam = d.teams[static_cast<std::size_t>(bossRoom.teamIndex)];
    REQUIRE(bossTeam.isBoss);
    REQUIRE_FALSE(bossTeam.bossId.empty());
    REQUIRE(db.findBoss(bossTeam.bossId) != nullptr);

    const cd::content::DungeonThemeDef* theme = db.findTheme("ruined_keep");
    REQUIRE(theme != nullptr);
    REQUIRE(std::find(theme->bosses.begin(), theme->bosses.end(), bossTeam.bossId) !=
            theme->bosses.end());
}

TEST_CASE("dungeon: deeper runs have at least as many gates", "[dungeon]") {
    const cd::content::ContentDatabase db = loadContent();
    const Dungeon shallow = generate(7, 1, db, "ruined_keep");
    const Dungeon deep = generate(7, 6, db, "ruined_keep");
    REQUIRE(deep.mandatoryGates >= shallow.mandatoryGates);
    REQUIRE(deep.mandatoryGates >= 3);
}

#endif  // CRYSTAL_TEST_DATA_DIR
