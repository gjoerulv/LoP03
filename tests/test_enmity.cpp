#include <catch2/catch_test_macros.hpp>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"

// M28 enmity/targeting: threat-driven target choice, per-role profiles, control
// skills (taunt/fade/redirect), and purity (so live play and the Simulator agree
// and a given encounter is reproducible).

using namespace cd;
using namespace cd::battle;

namespace {

content::ContentDatabase makeDb() {
    content::ContentDatabase db;

    content::SkillDef strike;
    strike.id = "strike";
    strike.name = "Strike";
    strike.category = content::SkillCategory::Physical;
    strike.target = content::SkillTarget::SingleEnemy;
    strike.power = 10;
    db.addSkill(strike);

    const auto control = [&](const std::string& id, content::SkillEffect eff) {
        content::SkillDef s;
        s.id = id;
        s.name = id;
        s.category = content::SkillCategory::Support;
        s.target = content::SkillTarget::Self;
        s.controlEffect = eff;
        db.addSkill(s);
    };
    control("taunt", content::SkillEffect::Taunt);
    control("fade", content::SkillEffect::Fade);
    control("redirect", content::SkillEffect::Intercept);

    const auto enemy = [&](const std::string& id, content::EnemyRole role) {
        content::EnemyDef e;
        e.id = id;
        e.name = id;
        e.stats = {200, 20, 10, 10, 7};
        e.role = role;
        e.skills = {"strike"};
        db.addEnemy(e);
    };
    enemy("brute", content::EnemyRole::Bruiser);   // -> Aggressive (threat)
    enemy("sniper", content::EnemyRole::Sniper);   // -> Opportunist (low HP)
    enemy("seer", content::EnemyRole::Disruptor);  // -> Tactician (caster)
    return db;
}

Combatant partyMember(const std::string& name, content::StatBlock st, int hp) {
    Combatant c;
    c.side = Side::Party;
    c.name = name;
    c.sourceId = "hero";
    c.stats = st;
    c.maxHp = st.maxHp;
    c.hp = hp;
    c.mp = c.maxMp = 20;
    return c;
}

// party[0] Warrior (high attack), [1] Mage (high magic), [2] Rogue; enemy at [3].
Battle board(const std::string& enemyId) {
    Battle b;
    b.units.push_back(partyMember("Warrior", {100, 22, 4, 10, 8}, 100));
    b.units.push_back(partyMember("Mage", {60, 4, 22, 4, 11}, 60));
    b.units.push_back(partyMember("Rogue", {70, 14, 6, 8, 18}, 70));
    Combatant e;
    e.side = Side::Enemy;
    e.name = enemyId;
    e.sourceId = enemyId;
    e.stats = {200, 20, 10, 10, 7};
    e.maxHp = 200;
    e.hp = 200;
    e.mp = e.maxMp = 20;
    e.skillIds = {"strike"};
    b.units.push_back(e);
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 12345;
    return b;
}

}  // namespace

TEST_CASE("enmity: an aggressive enemy chases threat, not the lowest HP (the reported bug)",
          "[battle][enmity]") {
    const auto db = makeDb();
    Battle b = board("brute");
    b.units[0].hp = 20;    // Warrior is badly hurt — the OLD AI would pile on here
    b.addThreat(1, 300);   // but the Mage has generated the most threat
    CHECK(chooseEnemyAction(b, 3, db).target == 1);
}

TEST_CASE("enmity: an opportunist enemy still goes for the kill (lowest HP)",
          "[battle][enmity]") {
    const auto db = makeDb();
    Battle b = board("sniper");
    b.units[0].hp = 15;    // Warrior nearly dead
    b.addThreat(1, 200);   // Mage high threat, but a sniper wants the kill
    CHECK(chooseEnemyAction(b, 3, db).target == 0);
}

TEST_CASE("enmity: a tactician enemy targets the backline caster", "[battle][enmity]") {
    const auto db = makeDb();
    Battle b = board("seer");
    CHECK(chooseEnemyAction(b, 3, db).target == 1);  // the Mage (highest magic)
}

TEST_CASE("enmity: different profiles target differently on the same board",
          "[battle][enmity]") {
    const auto db = makeDb();
    Battle aggressive = board("brute");
    Battle tactician = board("seer");
    aggressive.addThreat(0, 50);  // the Warrior has some threat (not overwhelming)
    tactician.addThreat(0, 50);
    // The aggressive enemy chases the Warrior's threat; the tactician stays on
    // the backline caster (moderate threat doesn't pull it off the Mage).
    CHECK(chooseEnemyAction(aggressive, 3, db).target == 0);
    CHECK(chooseEnemyAction(tactician, 3, db).target == 1);
}

TEST_CASE("enmity: dealing damage and healing accrue threat", "[battle][enmity]") {
    const auto db = makeDb();
    Battle b = board("brute");
    const content::SkillDef* strike = db.findSkill("strike");
    REQUIRE(strike != nullptr);
    b.units[0].skillIds = {"strike"};
    b.useSkill(0, 3, *strike);      // Warrior hits the enemy
    CHECK(b.threatOf(0) > 0);
    CHECK(b.threatOf(1) == 0);      // the Mage did nothing yet
}

TEST_CASE("enmity: taunt pulls an aggressive enemy onto the taunter", "[battle][enmity]") {
    const auto db = makeDb();
    Battle b = board("brute");
    b.addThreat(1, 300);            // the Mage would be the natural target
    b.units[0].skillIds = {"taunt"};
    b.useSkill(0, 0, *db.findSkill("taunt"));
    CHECK(chooseEnemyAction(b, 3, db).target == 0);  // now the taunter
}

TEST_CASE("enmity: fade sheds the caster's threat", "[battle][enmity]") {
    const auto db = makeDb();
    Battle b = board("brute");
    b.addThreat(1, 400);
    b.units[1].skillIds = {"fade"};
    b.useSkill(1, 1, *db.findSkill("fade"));
    CHECK(b.threatOf(1) == 100);   // 400 / 4
}

TEST_CASE("enmity: redirect makes the interceptor take a blow aimed at an ally",
          "[battle][enmity]") {
    const auto db = makeDb();
    Battle b = board("brute");
    b.units[0].skillIds = {"redirect"};
    b.useSkill(0, 0, *db.findSkill("redirect"));
    REQUIRE(b.units[0].intercepting);
    const int mageBefore = b.units[1].hp;
    const int warriorBefore = b.units[0].hp;
    b.attack(3, 1);                       // enemy swings at the Mage
    CHECK(b.units[1].hp == mageBefore);   // Mage is untouched
    CHECK(b.units[0].hp < warriorBefore); // the interceptor took it
}

TEST_CASE("enmity: targeting is a pure function of the board (reproducible)",
          "[battle][enmity]") {
    const auto db = makeDb();
    Battle a = board("brute");
    Battle c = board("brute");
    a.addThreat(1, 150);
    a.addThreat(2, 80);
    c.addThreat(1, 150);
    c.addThreat(2, 80);
    CHECK(chooseEnemyAction(a, 3, db).target == chooseEnemyAction(c, 3, db).target);
}

TEST_CASE("enmity: round decay reduces threat", "[battle][enmity]") {
    Battle b = board("brute");
    b.addThreat(1, 400);
    b.beginRound();
    CHECK(b.threatOf(1) == 300);  // 400 * 3 / 4
}
