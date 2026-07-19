#include <catch2/catch_test_macros.hpp>

#include <string>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"

// Boss archetype mechanics (M20): deterministic sim rules, tested headlessly
// on hand-built battles so no data file can mask a rule change.

using namespace cd;
using namespace cd::battle;

namespace {

Combatant unit(Side side, int hp, int attack, int defense, int speed) {
    Combatant u;
    u.side = side;
    u.name = side == Side::Party ? "Hero" : "Foe";
    u.hp = u.maxHp = hp;
    u.stats.maxHp = hp;
    u.stats.attack = attack;
    u.stats.defense = defense;
    u.stats.speed = speed;
    return u;
}

content::SkillDef magicSkill(int power) {
    content::SkillDef s;
    s.id = "zap";
    s.name = "Zap";
    s.category = content::SkillCategory::Magic;
    s.target = content::SkillTarget::SingleEnemy;
    s.power = power;
    return s;
}

}  // namespace

TEST_CASE("boss: brute enrage announces exactly once", "[boss]") {
    Battle b;
    b.units.push_back(unit(Side::Party, 200, 10, 5, 5));
    Combatant boss = unit(Side::Enemy, 100, 20, 10, 8);
    boss.isBoss = true;
    boss.enrages = true;
    b.units.push_back(boss);

    // Above half HP: no rage line.
    std::string log = b.attack(1, 0);
    REQUIRE(log.find("rage") == std::string::npos);

    b.units[1].hp = 40;  // below half
    log = b.attack(1, 0);
    REQUIRE(log.find("flies into a rage!") != std::string::npos);
    log = b.attack(1, 0);
    REQUIRE(log.find("rage") == std::string::npos);  // announced once
}

TEST_CASE("boss: sorcerer magic grows as its allies fall", "[boss]") {
    content::ContentDatabase db;  // skills passed directly; db unused here
    (void)db;
    Battle b;
    b.units.push_back(unit(Side::Party, 400, 10, 0, 5));
    Combatant boss = unit(Side::Enemy, 200, 5, 5, 9);
    boss.isBoss = true;
    boss.empowersOnAllyFall = true;
    boss.stats.magic = 20;
    b.units.push_back(boss);
    Combatant minionA = unit(Side::Enemy, 30, 5, 5, 7);
    Combatant minionB = unit(Side::Enemy, 30, 5, 5, 7);
    b.units.push_back(minionA);
    b.units.push_back(minionB);

    const content::SkillDef zap = magicSkill(10);
    const int hp0 = b.units[0].hp;
    b.useSkill(1, 0, zap);
    const int baseDmg = hp0 - b.units[0].hp;

    b.units[2].hp = 0;  // one minion falls
    const int hp1 = b.units[0].hp;
    const std::string log1 = b.useSkill(1, 0, zap);
    const int dmg1 = hp1 - b.units[0].hp;
    REQUIRE(dmg1 == baseDmg * 125 / 100);
    REQUIRE(log1.find("empowered +25%") != std::string::npos);

    b.units[3].hp = 0;  // both fallen
    const int hp2 = b.units[0].hp;
    b.useSkill(1, 0, zap);
    REQUIRE(hp2 - b.units[0].hp == baseDmg * 150 / 100);
}

TEST_CASE("boss: commander rallies fallen minions once below half HP", "[boss]") {
    Battle b;
    b.units.push_back(unit(Side::Party, 200, 10, 5, 5));
    Combatant boss = unit(Side::Enemy, 100, 15, 8, 8);
    boss.isBoss = true;
    boss.ralliesMinions = true;
    b.units.push_back(boss);
    Combatant minion = unit(Side::Enemy, 40, 8, 4, 6);
    minion.hp = 0;  // already fallen
    b.units.push_back(minion);

    // Above half HP: nothing happens.
    REQUIRE(b.tickStatuses(1).empty());
    REQUIRE_FALSE(b.units[2].alive());

    b.units[1].hp = 45;  // below half
    const std::string log = b.tickStatuses(1);
    REQUIRE(log.find("rallies the fallen") != std::string::npos);
    REQUIRE(b.units[2].alive());
    REQUIRE(b.units[2].hp == 20);  // half strength

    // Only once: kill the minion again, no second rally.
    b.units[2].hp = 0;
    REQUIRE(b.tickStatuses(1).empty());
    REQUIRE_FALSE(b.units[2].alive());
}

TEST_CASE("boss: rush opener doubles only the first action", "[boss]") {
    Battle b;
    b.units.push_back(unit(Side::Party, 400, 10, 10, 5));
    Combatant boss = unit(Side::Enemy, 200, 20, 10, 9);
    boss.isBoss = true;
    boss.rushOpener = true;
    b.units.push_back(boss);

    const int hp0 = b.units[0].hp;
    const std::string log = b.attack(1, 0);
    const int openerDmg = hp0 - b.units[0].hp;
    REQUIRE(log.find("opening fury") != std::string::npos);

    const int hp1 = b.units[0].hp;
    b.attack(1, 0);
    const int normalDmg = hp1 - b.units[0].hp;
    REQUIRE(openerDmg == normalDmg * 2);
}

TEST_CASE("boss: enemy AI casts an uncovered support skill", "[boss]") {
    content::ContentDatabase db;
    content::SkillDef weaken;
    weaken.id = "weaken";
    weaken.name = "Weaken";
    weaken.category = content::SkillCategory::Support;
    weaken.target = content::SkillTarget::SingleEnemy;
    weaken.statusEffect = content::StatusType::AttackDown;
    weaken.statusMagnitude = 20;
    weaken.statusDuration = 3;
    REQUIRE(db.addSkill(weaken));

    Battle b;
    b.units.push_back(unit(Side::Party, 100, 10, 5, 5));
    Combatant foe = unit(Side::Enemy, 60, 10, 5, 8);
    foe.skillIds = {"weaken"};
    b.units.push_back(foe);

    const EnemyChoice first = chooseEnemyAction(b, 1, db);
    REQUIRE(first.useSkill);
    REQUIRE(first.skillId == "weaken");
    REQUIRE(first.target == 0);
    b.useSkill(1, first.target, weaken);

    // Status active: the AI falls back to attacking instead of spamming.
    const EnemyChoice second = chooseEnemyAction(b, 1, db);
    REQUIRE_FALSE(second.useSkill);
}
