#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"
#include "game/Spoils.hpp"

#ifdef CRYSTAL_TEST_DATA_DIR
#include <filesystem>

#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#endif

using namespace cd;

namespace {

content::ContentDatabase makeDb() {
    content::ContentDatabase db;
    content::EnemyDef rat;
    rat.id = "rat";
    rat.name = "Rat";
    rat.stats = {10, 4, 0, 1, 5};
    rat.xpReward = 7;
    rat.goldReward = 11;
    db.addEnemy(rat);

    content::BossDef boss;
    boss.id = "big_rat";
    boss.name = "Big Rat";
    boss.stats = {100, 10, 0, 5, 5};
    boss.xpReward = 50;
    boss.goldReward = 120;
    db.addBoss(boss);
    return db;
}

}  // namespace

TEST_CASE("spoils: a team pays every enemy plus the boss", "[spoils]") {
    const content::ContentDatabase db = makeDb();
    dungeon::EnemyTeam team;
    team.enemyIds = {"rat", "rat", "rat"};
    const BattleSpoils plain = teamSpoils(team, db);
    CHECK(plain.xp == 21);
    CHECK(plain.gold == 33);

    team.bossId = "big_rat";
    const BattleSpoils withBoss = teamSpoils(team, db);
    CHECK(withBoss.xp == 71);
    CHECK(withBoss.gold == 153);

    dungeon::EnemyTeam unknown;
    unknown.enemyIds = {"no_such_enemy"};  // defensive: bad content pays zero
    const BattleSpoils none = teamSpoils(unknown, db);
    CHECK(none.xp == 0);
    CHECK(none.gold == 0);
}

#ifdef CRYSTAL_TEST_DATA_DIR

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

}  // namespace

TEST_CASE("spoils: applySpoils grants gold and party-wide XP", "[spoils]") {
    const content::ContentDatabase db = loadContent();
    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan", 5));
    p.members.push_back(createCharacter(*db.findClass("mage"), "Mira", 5));
    p.gold = 100;

    BattleSpoils s;
    s.xp = 5;  // below xpToNext(5): no level-up
    s.gold = 40;
    const SpoilsResult r = applySpoils(p, s, db);
    CHECK(r.xp == 5);
    CHECK(r.gold == 40);  // no milestone bonuses on a bare party
    CHECK(p.gold == 140);
    CHECK(p.members[0].xp == 5);
    CHECK(p.members[1].xp == 5);
    CHECK(r.levelUps.empty());
}

TEST_CASE("spoils: a level-up reports the diff and the new skills", "[spoils]") {
    const content::ContentDatabase db = loadContent();
    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan", 1));

    // Enough XP to cross several levels; the diff must match the character's
    // actual before/after, and newSkillNames exactly the learnset additions.
    const Character before = p.members[0];
    const std::vector<std::string> knownBefore = allKnownSkills(before, db);

    BattleSpoils s;
    s.xp = 400;
    const SpoilsResult r = applySpoils(p, s, db);
    const Character& after = p.members[0];
    REQUIRE(after.level > before.level);
    REQUIRE(r.levelUps.size() == 1);
    const LevelUpDiff& d = r.levelUps[0];
    CHECK(d.name == "Rolan");
    CHECK(d.fromLevel == before.level);
    CHECK(d.toLevel == after.level);
    CHECK(d.hpDelta == after.maxHp - before.maxHp);
    CHECK(d.hpDelta > 0);
    CHECK(d.atkDelta == after.stats.attack - before.stats.attack);
    CHECK(d.spdDelta == after.stats.speed - before.stats.speed);
    // New skills = exactly the learnset entries the new level unlocked.
    const std::vector<std::string> knownAfter = allKnownSkills(after, db);
    std::size_t expectedNew = 0;
    for (const std::string& id : knownAfter) {
        if (std::find(knownBefore.begin(), knownBefore.end(), id) == knownBefore.end()) {
            ++expectedNew;
        }
    }
    CHECK(d.newSkillNames.size() == expectedNew);
}

TEST_CASE("spoils: standing-member gold milestones sweeten the take", "[spoils]") {
    const content::ContentDatabase db = loadContent();
    REQUIRE(db.findMilestone("rogue_10_b") != nullptr);  // Cutpurse: +15% gold
    Party p;
    p.members.push_back(createCharacter(*db.findClass("rogue"), "Vex", 10));
    p.members[0].milestone10 = "rogue_10_b";

    BattleSpoils s;
    s.gold = 100;
    SECTION("standing rogue collects") {
        const SpoilsResult r = applySpoils(p, s, db);
        CHECK(r.gold == 115);
        CHECK(p.gold == 115);
    }
    SECTION("a KO'd rogue does not") {
        p.members[0].hp = 0;
        const SpoilsResult r = applySpoils(p, s, db);
        CHECK(r.gold == 100);
    }
}

TEST_CASE("spoils: a capped member never reports a level-up", "[spoils]") {
    const content::ContentDatabase db = loadContent();
    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan", kMaxLevel));
    BattleSpoils s;
    s.xp = 100000;
    const SpoilsResult r = applySpoils(p, s, db);
    CHECK(r.levelUps.empty());
    CHECK(p.members[0].level == kMaxLevel);
}

#endif  // CRYSTAL_TEST_DATA_DIR
