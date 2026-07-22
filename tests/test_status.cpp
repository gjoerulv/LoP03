#include <catch2/catch_test_macros.hpp>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"

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

    content::SkillDef venom;
    venom.id = "venom";
    venom.name = "Venom";
    venom.category = content::SkillCategory::Physical;
    venom.target = content::SkillTarget::SingleEnemy;
    venom.power = 5;
    venom.statusEffect = content::StatusType::Poison;
    venom.statusMagnitude = 6;
    venom.statusDuration = 3;
    db.addSkill(venom);

    content::SkillDef shieldup;
    shieldup.id = "shieldup";
    shieldup.name = "Shield Up";
    shieldup.category = content::SkillCategory::Support;
    shieldup.target = content::SkillTarget::Self;
    shieldup.statusEffect = content::StatusType::DefenseUp;
    shieldup.statusMagnitude = 100;
    shieldup.statusDuration = 2;
    db.addSkill(shieldup);

    content::SkillDef weaken;
    weaken.id = "weaken";
    weaken.name = "Weaken";
    weaken.category = content::SkillCategory::Support;
    weaken.target = content::SkillTarget::SingleEnemy;
    weaken.statusEffect = content::StatusType::AttackDown;
    weaken.statusMagnitude = 50;
    weaken.statusDuration = 2;
    db.addSkill(weaken);

    content::ClassDef knight;
    knight.id = "knight";
    knight.name = "Knight";
    knight.baseStats = {120, 20, 4, 10, 8};
    db.addClass(knight);

    content::EnemyDef goblin;
    goblin.id = "goblin";
    goblin.name = "Goblin";
    goblin.stats = {60, 20, 0, 10, 9};
    db.addEnemy(goblin);

    content::ItemDef antidote;
    antidote.id = "antidote";
    antidote.name = "Antidote";
    antidote.type = content::ItemType::Consumable;
    antidote.effect = content::ConsumableEffect::Cure;
    db.addItem(antidote);

    content::BossDef brute;
    brute.id = "brute";
    brute.name = "Brute";
    brute.archetype = content::BossArchetype::Brute;
    brute.stats = {200, 24, 0, 10, 8};
    db.addBoss(brute);

    return db;
}

Party oneKnight(const content::ContentDatabase& db) {
    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Hero"));
    return p;
}

dungeon::EnemyTeam goblinTeam() {
    dungeon::EnemyTeam t;
    t.enemyIds = {"goblin"};
    return t;
}

}  // namespace

TEST_CASE("status: poison ticks each turn and then expires", "[status]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(), db);
    const content::SkillDef* venom = db.findSkill("venom");
    b.useSkill(0, 1, *venom);  // venom: authored 6/turn x3, but M35 doubles both

    // M35 balance: statuses last 2x their authored duration (3 -> 6) and poison
    // deals 2x its authored magnitude per tick (6 -> 12).
    REQUIRE(b.units[1].statuses.size() == 1);
    CHECK(b.units[1].statuses[0].turns == 6);
    const int hp0 = b.units[1].hp;
    b.tickStatuses(1);
    CHECK(b.units[1].hp == hp0 - 12);
    for (int i = 0; i < 5; ++i) {
        b.tickStatuses(1);  // ticks 2..6; poison expires after the 6th
    }
    CHECK(b.units[1].statuses.empty());
    const int afterSix = b.units[1].hp;
    b.tickStatuses(1);  // no status left
    CHECK(b.units[1].hp == afterSix);
}

TEST_CASE("status: defense up reduces incoming damage", "[status]") {
    const content::ContentDatabase db = makeDb();
    Battle baseline = buildBattle(oneKnight(db), goblinTeam(), db);
    baseline.attack(1, 0);  // goblin hits knight, no buff
    const int plain = baseline.units[0].maxHp - baseline.units[0].hp;

    Battle buffed = buildBattle(oneKnight(db), goblinTeam(), db);
    buffed.useSkill(0, 0, *db.findSkill("shieldup"));  // knight buffs own defense
    buffed.attack(1, 0);
    const int reduced = buffed.units[0].maxHp - buffed.units[0].hp;

    REQUIRE(reduced < plain);
}

TEST_CASE("status: attack down reduces outgoing damage", "[status]") {
    const content::ContentDatabase db = makeDb();
    Battle baseline = buildBattle(oneKnight(db), goblinTeam(), db);
    baseline.attack(0, 1);
    const int plain = baseline.units[1].maxHp - baseline.units[1].hp;

    Battle weak = buildBattle(oneKnight(db), goblinTeam(), db);
    weak.useSkill(1, 0, *db.findSkill("weaken"));  // goblin weakens the knight
    weak.attack(0, 1);
    const int reduced = weak.units[1].maxHp - weak.units[1].hp;

    REQUIRE(reduced < plain);
}

TEST_CASE("status: cure removes poison and debuffs", "[status]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(), db);
    b.useSkill(0, 1, *db.findSkill("venom"));
    REQUIRE_FALSE(b.units[1].statuses.empty());
    b.useItem(0, 1, *db.findItem("antidote"));
    REQUIRE(b.units[1].statuses.empty());
}

TEST_CASE("status: a Brute boss enrages below half HP", "[status]") {
    const content::ContentDatabase db = makeDb();
    dungeon::EnemyTeam bossTeam;
    bossTeam.isBoss = true;
    bossTeam.bossId = "brute";

    Battle b = buildBattle(oneKnight(db), bossTeam, db);
    REQUIRE(b.units[1].isBoss);
    REQUIRE(b.units[1].enrages);

    b.attack(1, 0);  // boss at full HP
    const int normal = b.units[0].maxHp - b.units[0].hp;

    b.units[0].hp = b.units[0].maxHp;       // reset target
    b.units[1].hp = b.units[1].maxHp / 3;   // boss below half -> enraged
    b.attack(1, 0);
    const int enraged = b.units[0].maxHp - b.units[0].hp;

    REQUIRE(enraged > normal);
}
