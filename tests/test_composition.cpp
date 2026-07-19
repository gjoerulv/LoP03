#include <catch2/catch_test_macros.hpp>

#include "content/Definitions.hpp"

#ifdef CRYSTAL_TEST_DATA_DIR
#include <filesystem>
#include <set>
#include <string>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "danger/DangerRating.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"
#endif

using namespace cd;

TEST_CASE("composition: curve helpers grow past the old caps", "[composition]") {
    const content::CompositionDef c;  // shipped defaults
    // Team size keeps growing to the cap instead of saturating at depth 3.
    REQUIRE(c.teamSizeMax(1) == 2);
    REQUIRE(c.teamSizeMax(4) == 4);
    REQUIRE(c.teamSizeMax(6) == 5);
    REQUIRE(c.teamSizeMax(20) == 5);
    REQUIRE(c.teamSizeMin(1) == 2);
    REQUIRE(c.teamSizeMin(4) == 3);
    // Elite share keeps growing to 70% (old cap was 50% by depth ~4).
    REQUIRE(c.eliteChancePct(4) == 36);
    REQUIRE(c.eliteChancePct(8) == 70);
    // Stat scaling starts past depth 5 and caps at +90%.
    REQUIRE(c.statScalePct(5) == 0);
    REQUIRE(c.statScalePct(8) == 18);
    REQUIRE(c.statScalePct(50) == 90);
}

#ifdef CRYSTAL_TEST_DATA_DIR

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

int countRoles(const dungeon::EnemyTeam& team, const content::ContentDatabase& db,
               bool (*pred)(content::EnemyRole)) {
    int n = 0;
    for (const std::string& id : team.enemyIds) {
        if (const content::EnemyDef* def = db.findEnemy(id)) {
            n += pred(def->role) ? 1 : 0;
        }
    }
    return n;
}

}  // namespace

TEST_CASE("composition: generated teams never violate the authored constraints",
          "[composition]") {
    const content::ContentDatabase db = loadContent();
    const content::CompositionDef& comp = db.composition();
    const char* const themes[] = {"", "ruined_keep", "crystal_mine", "hollow_forest"};

    int teamsChecked = 0;
    for (int i = 0; i < 30; ++i) {
        const std::uint64_t seed = static_cast<std::uint64_t>(i) * 6151u + 7u;
        for (int depth : {1, 3, 6, 10}) {
            for (const char* theme : themes) {
                const dungeon::Dungeon d = dungeon::generate(seed, depth, db, theme);
                // Elite-challenge teams (event rooms) are deliberately 1-2
                // elites and exempt from normal-team composition rules.
                std::set<int> challengeTeams;
                for (const dungeon::Room& r : d.rooms) {
                    if (r.type == dungeon::RoomType::Event && r.teamIndex >= 0) {
                        challengeTeams.insert(r.teamIndex);
                    }
                }
                for (std::size_t ti = 0; ti < d.teams.size(); ++ti) {
                    const dungeon::EnemyTeam& team = d.teams[ti];
                    INFO("seed " << seed << " depth " << depth << " theme '" << theme << "' team "
                                 << team.name);
                    REQUIRE(team.statScalePct == 100 + comp.statScalePct(depth));
                    if (team.isBoss) {
                        REQUIRE(static_cast<int>(team.enemyIds.size()) <= comp.maxMinions);
                        continue;
                    }
                    if (challengeTeams.count(static_cast<int>(ti)) > 0) {
                        const int csize = static_cast<int>(team.enemyIds.size());
                        REQUIRE(csize >= 1);
                        REQUIRE(csize <= 2);
                        continue;
                    }
                    const int size = static_cast<int>(team.enemyIds.size());
                    REQUIRE(size >= comp.teamSizeMin(depth));
                    REQUIRE(size <= comp.teamSizeMax(depth));
                    REQUIRE(countRoles(team, db, content::isSupportRole) <= comp.maxSupport);
                    REQUIRE(countRoles(team, db, content::isDamageRole) >= comp.minDamage);
                    ++teamsChecked;
                }
            }
        }
    }
    REQUIRE(teamsChecked > 1000);
}

TEST_CASE("composition: depth scaling reaches the fight and the danger label",
          "[composition]") {
    const content::ContentDatabase db = loadContent();
    const dungeon::Dungeon shallow = dungeon::generate(4242, 3, db, "ruined_keep");
    const dungeon::Dungeon deep = dungeon::generate(4242, 10, db, "ruined_keep");

    // The same seed's teams are strictly more threatening at depth 10 than
    // depth 3 (bigger teams, more elites, +30% stats) — the plateau fix.
    int shallowSum = 0;
    int deepSum = 0;
    for (const dungeon::EnemyTeam& t : shallow.teams) {
        shallowSum += danger::teamThreat(t, db);
    }
    for (const dungeon::EnemyTeam& t : deep.teams) {
        deepSum += danger::teamThreat(t, db);
    }
    REQUIRE(deepSum > shallowSum);

    // Combatants actually carry the scaled stats.
    const dungeon::EnemyTeam* deepNormal = nullptr;
    for (const dungeon::EnemyTeam& t : deep.teams) {
        if (!t.isBoss && !t.enemyIds.empty()) {
            deepNormal = &t;
            break;
        }
    }
    REQUIRE(deepNormal != nullptr);
    REQUIRE(deepNormal->statScalePct == 130);  // depth 10: (10-5)*6 = +30%
    Party party;
    if (const content::ClassDef* cls = db.findClass("knight")) {
        party.members.push_back(createCharacter(*cls, "K", 1));
    }
    const battle::Battle b = battle::buildBattle(party, *deepNormal, db);
    bool sawEnemy = false;
    for (const battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Enemy) {
            const content::EnemyDef* def = db.findEnemy(u.sourceId);
            REQUIRE(def != nullptr);
            REQUIRE(u.stats.attack == def->stats.attack * 130 / 100);
            sawEnemy = true;
        }
    }
    REQUIRE(sawEnemy);
}

TEST_CASE("composition: every theme pool covers the damage requirement", "[composition]") {
    const content::ContentDatabase db = loadContent();
    for (const auto& [id, theme] : db.themes()) {
        int damage = 0;
        int distinctRoles = 0;
        bool seen[8] = {};
        for (const std::string& eid : theme.normalEnemies) {
            const content::EnemyDef* def = db.findEnemy(eid);
            REQUIRE(def != nullptr);
            damage += content::isDamageRole(def->role) ? 1 : 0;
            const int r = static_cast<int>(def->role);
            if (!seen[r]) {
                seen[r] = true;
                ++distinctRoles;
            }
        }
        INFO("theme " << id);
        REQUIRE(damage >= 2);         // constraints can always be satisfied
        REQUIRE(distinctRoles >= 4);  // encounters can pose distinct questions
    }
}

#endif  // CRYSTAL_TEST_DATA_DIR
