#include <catch2/catch_test_macros.hpp>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "danger/DangerRating.hpp"
#include "dungeon/DungeonModel.hpp"

using namespace cd;
using namespace cd::danger;

namespace {

content::ContentDatabase makeDb() {
    content::ContentDatabase db;
    content::SkillDef strike;
    strike.id = "strike";
    strike.name = "Strike";
    strike.category = content::SkillCategory::Physical;
    strike.power = 10;
    db.addSkill(strike);

    content::EnemyDef rat;
    rat.id = "rat";
    rat.name = "Rat";
    rat.stats = {10, 4, 0, 1, 5};
    rat.tier = content::EnemyTier::Normal;
    db.addEnemy(rat);

    content::EnemyDef ogre;
    ogre.id = "ogre";
    ogre.name = "Ogre";
    ogre.stats = {110, 22, 0, 14, 7};
    ogre.tier = content::EnemyTier::Elite;
    ogre.skills = {"strike"};
    db.addEnemy(ogre);
    return db;
}

dungeon::EnemyTeam team(std::vector<std::string> ids, bool boss = false) {
    dungeon::EnemyTeam t;
    t.enemyIds = std::move(ids);
    t.isBoss = boss;
    return t;
}

}  // namespace

TEST_CASE("danger: threat grows with stronger and more numerous enemies", "[danger]") {
    const content::ContentDatabase db = makeDb();
    const int oneRat = teamThreat(team({"rat"}), db);
    const int twoRats = teamThreat(team({"rat", "rat"}), db);
    const int oneOgre = teamThreat(team({"ogre"}), db);

    REQUIRE(oneRat > 0);
    REQUIRE(twoRats > oneRat);   // more enemies (and synergy) => more threat
    REQUIRE(oneOgre > twoRats);  // a much stronger enemy
}

TEST_CASE("danger: a stronger team rates a higher tier", "[danger]") {
    const content::ContentDatabase db = makeDb();
    const Tier weak = assess(team({"rat"}), 1, db);
    const Tier strong = assess(team({"ogre", "ogre"}), 1, db);
    REQUIRE(static_cast<int>(strong) > static_cast<int>(weak));
}

TEST_CASE("danger: a boss team is always the Boss tier", "[danger]") {
    const content::ContentDatabase db = makeDb();
    REQUIRE(assess(team({"rat"}, /*boss=*/true), 1, db) == Tier::Boss);
}

TEST_CASE("danger: tierFor crosses thresholds against the depth baseline", "[danger]") {
    // Depth-1 baseline is 50, so ratio = threat * 2 percent.
    REQUIRE(tierFor(0, 1, false) == Tier::Trivial);
    REQUIRE(tierFor(29, 1, false) == Tier::Trivial);   // ratio 58 < 60
    REQUIRE(tierFor(30, 1, false) == Tier::Easy);       // ratio 60
    REQUIRE(tierFor(45, 1, false) == Tier::Fair);       // ratio 90
    REQUIRE(tierFor(65, 1, false) == Tier::Dangerous);  // ratio 130
    REQUIRE(tierFor(90, 1, false) == Tier::Deadly);     // ratio 180
    REQUIRE(tierFor(10, 1, true) == Tier::Boss);
}

TEST_CASE("danger: tier weights are ordered", "[danger]") {
    REQUIRE(tierWeight(Tier::Trivial) < tierWeight(Tier::Fair));
    REQUIRE(tierWeight(Tier::Fair) < tierWeight(Tier::Deadly));
    REQUIRE(tierWeight(Tier::Deadly) < tierWeight(Tier::Boss));
}

TEST_CASE("danger: rating is deterministic", "[danger]") {
    const content::ContentDatabase db = makeDb();
    REQUIRE(teamThreat(team({"ogre", "rat"}), db) == teamThreat(team({"ogre", "rat"}), db));
}
