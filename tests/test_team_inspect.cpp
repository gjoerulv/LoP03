// M88 — pre-fight team inspection: the pure describeTeam body the dungeon's
// Details overlay renders. Its one honesty rule: the stats it prints are the
// stats buildBattle will field (the shared content::scaledStats multiply), and
// it discloses exactly the affinity/passive set the in-battle target panel
// already shows while aiming — never more, never less.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "content/Stats.hpp"
#include "dungeon/TeamInspect.hpp"

using namespace cd;

namespace {

const content::ContentDatabase& shipped() {
    static content::ContentDatabase db;
    static bool loaded = false;
    if (!loaded) {
        content::LoadReport rep;
        REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep));
        loaded = true;
    }
    return db;
}

// First shipped enemy satisfying a predicate — content-shape agnostic.
template <typename Pred>
const content::EnemyDef* firstEnemy(Pred pred) {
    for (const auto& [id, def] : shipped().enemies()) {
        (void)id;
        if (pred(def)) {
            return &def;
        }
    }
    return nullptr;
}

bool contains(const std::string& body, const std::string& needle) {
    return body.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("team inspect: stats are the buildBattle multiply, duplicates collapse",
          "[teaminspect]") {
    const content::EnemyDef* foe = firstEnemy([](const content::EnemyDef& e) {
        return !e.bossOnly && e.stats.maxHp > 0;
    });
    REQUIRE(foe != nullptr);

    dungeon::EnemyTeam team;
    team.name = "Test Patrol";
    team.enemyIds = {foe->id, foe->id, foe->id};
    team.statScalePct = 150;

    const std::string body = dungeon::describeTeam(team, "Dangerous", shipped());
    CHECK(contains(body, "Test Patrol - Dangerous, 3 enemies."));
    CHECK(contains(body, foe->name + " x3"));  // duplicates collapse to one block

    const content::StatBlock s = content::scaledStats(foe->stats, 150);
    CHECK(contains(body, "HP " + std::to_string(s.maxHp)));
    CHECK(contains(body, "ATK " + std::to_string(s.attack)));
    CHECK(contains(body, "MAG " + std::to_string(s.magic)));
    CHECK(contains(body, "DEF " + std::to_string(s.defense)));
    CHECK(contains(body, "SPD " + std::to_string(s.speed)));

    // At base scale the numbers are the authored ones — same multiply, pct 100.
    team.statScalePct = 100;
    const std::string base = dungeon::describeTeam(team, "Fair", shipped());
    CHECK(contains(base, "HP " + std::to_string(foe->stats.maxHp)));
}

TEST_CASE("team inspect: affinities and passives disclose like the target panel",
          "[teaminspect]") {
    const content::EnemyDef* weak = firstEnemy([](const content::EnemyDef& e) {
        return !e.affinity.weaknesses.empty();
    });
    REQUIRE(weak != nullptr);  // the M48 curation guarantees weak-hitting foes exist

    dungeon::EnemyTeam team;
    team.name = "Affinity Check";
    team.enemyIds = {weak->id};
    std::string body = dungeon::describeTeam(team, "Easy", shipped());
    CHECK(contains(body, "Weak: "));
    CHECK(contains(body, content::toString(weak->affinity.weaknesses.front())));

    const content::EnemyDef* passived = firstEnemy([](const content::EnemyDef& e) {
        return !e.passives.empty();
    });
    REQUIRE(passived != nullptr);
    team.enemyIds = {passived->id};
    body = dungeon::describeTeam(team, "Easy", shipped());
    const content::PassiveDef* p = shipped().findPassive(passived->passives.front());
    REQUIRE(p != nullptr);
    CHECK(contains(body, "Passive: "));
    CHECK(contains(body, p->name));

    // A plain foe stays plain: no empty Weak/Immune/Passive lines.
    const content::EnemyDef* plain = firstEnemy([](const content::EnemyDef& e) {
        return !e.affinity.any() && e.passives.empty() && !e.bossOnly;
    });
    if (plain != nullptr) {
        team.enemyIds = {plain->id};
        body = dungeon::describeTeam(team, "Trivial", shipped());
        CHECK_FALSE(contains(body, "Weak: "));
        CHECK_FALSE(contains(body, "Immune: "));
        CHECK_FALSE(contains(body, "Passive: "));
    }
}

TEST_CASE("team inspect: a boss team leads with the boss, count includes it",
          "[teaminspect]") {
    const content::BossDef* boss = nullptr;
    for (const auto& [id, def] : shipped().bosses()) {
        (void)id;
        if (!def.minions.empty()) {
            boss = &def;
            break;
        }
    }
    REQUIRE(boss != nullptr);

    dungeon::EnemyTeam team;
    team.name = boss->name;
    team.isBoss = true;
    team.bossId = boss->id;
    team.enemyIds = boss->minions;
    team.statScalePct = 200;

    const std::string body = dungeon::describeTeam(team, "Boss", shipped());
    CHECK(contains(body, std::to_string(team.count()) +
                             (team.count() == 1 ? " enemy." : " enemies.")));
    // The boss block appears, with its scaled stats, before any minion block.
    const content::StatBlock bs = content::scaledStats(boss->stats, 200);
    const std::size_t bossAt = body.find("HP " + std::to_string(bs.maxHp));
    CHECK(bossAt != std::string::npos);
    if (const content::EnemyDef* minion = shipped().findEnemy(boss->minions.front())) {
        const std::size_t minionAt = body.find(minion->name);
        REQUIRE(minionAt != std::string::npos);
        CHECK(bossAt < minionAt);
    }

    // Unknown ids never crash — they are simply absent (defensive rule).
    dungeon::EnemyTeam bad;
    bad.name = "Ghost Team";
    bad.enemyIds = {"no_such_enemy"};
    bad.bossId = "no_such_boss";
    const std::string ghost = dungeon::describeTeam(bad, "Fair", shipped());
    CHECK(contains(ghost, "Ghost Team"));
    CHECK_FALSE(contains(ghost, "no_such"));
}
