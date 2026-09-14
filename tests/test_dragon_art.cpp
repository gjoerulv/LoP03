// M115 - the clone wears the Dragon's face: `bossArt` is set on every boss
// at build and copied to the boss's clone slot, which stays a non-boss.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"

using namespace cd;

namespace {

const content::ContentDatabase& db() {
    static content::ContentDatabase database;
    static bool loaded = false;
    if (!loaded) {
        content::LoadReport rep;
        REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), database, rep));
        loaded = true;
    }
    return database;
}

Party makeParty() {
    Party party;
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        const content::ClassDef* cls = db().findClass(id);
        REQUIRE(cls != nullptr);
        party.members.push_back(createCharacter(*cls, id, 40));
    }
    return party;
}

}  // namespace

TEST_CASE("dragon art: the Dragon and its clone slot both carry the boss art", "[dragon][formation]") {
    battle::Battle b = battle::buildBattle(makeParty(), dragonTeam(db()), db());
    int bosses = 0;
    int clones = 0;
    for (const battle::Combatant& u : b.units) {
        if (u.side != battle::Side::Enemy) {
            CHECK_FALSE(u.bossArt);
            continue;
        }
        if (u.isBoss) {
            CHECK(u.bossArt);
            ++bosses;
        } else if (u.summonSlot) {
            CHECK(u.bossArt);       // the face
            CHECK_FALSE(u.isBoss);  // never the rules
            CHECK(u.sourceId == std::string(kDragonBossId));
            ++clones;
        } else {
            CHECK_FALSE(u.bossArt);
        }
    }
    CHECK(bosses == 1);
    CHECK(clones == 1);
}

TEST_CASE("dragon art: ordinary foes carry no boss art", "[dragon][formation]") {
    const dungeon::EnemyTeam team = dungeon::patrolTeam(db(), "ruined_keep", 3, 5, 11ull, 0);
    battle::Battle b = battle::buildBattle(makeParty(), team, db());
    for (const battle::Combatant& u : b.units) {
        CHECK_FALSE(u.bossArt);
    }
    // A dungeon boss with minions: only the boss (and a clone slot, if any).
    dungeon::EnemyTeam bossTeam;
    bossTeam.isBoss = true;
    bossTeam.bossId = "keep_warden";
    bossTeam.enemyIds = db().findBoss("keep_warden")->minions;
    battle::Battle bb = battle::buildBattle(makeParty(), bossTeam, db());
    for (const battle::Combatant& u : bb.units) {
        CHECK(u.bossArt == (u.isBoss || u.summonSlot));
    }
}
