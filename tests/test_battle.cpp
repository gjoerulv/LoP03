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
    strike.mpCost = 0;
    db.addSkill(strike);

    content::SkillDef fireball;
    fireball.id = "fireball";
    fireball.name = "Fireball";
    fireball.category = content::SkillCategory::Magic;
    fireball.target = content::SkillTarget::SingleEnemy;
    fireball.power = 14;
    fireball.mpCost = 6;
    db.addSkill(fireball);

    content::SkillDef mend;
    mend.id = "mend";
    mend.name = "Mend";
    mend.category = content::SkillCategory::Heal;
    mend.target = content::SkillTarget::SingleAlly;
    mend.power = 20;
    mend.mpCost = 5;
    db.addSkill(mend);

    content::ClassDef knight;
    knight.id = "knight";
    knight.name = "Knight";
    knight.baseStats = {120, 18, 4, 16, 8};
    knight.startingSkills = {"strike", "mend"};
    db.addClass(knight);

    content::EnemyDef goblin;
    goblin.id = "goblin";
    goblin.name = "Goblin";
    goblin.stats = {30, 10, 0, 5, 9};
    goblin.skills = {"strike"};
    db.addEnemy(goblin);

    content::ItemDef potion;
    potion.id = "potion";
    potion.name = "Potion";
    potion.type = content::ItemType::Consumable;
    potion.effect = content::ConsumableEffect::Heal;
    potion.effectAmount = 30;
    db.addItem(potion);

    content::ItemDef phoenix;
    phoenix.id = "phoenix_tear";
    phoenix.name = "Phoenix Tear";
    phoenix.type = content::ItemType::Consumable;
    phoenix.effect = content::ConsumableEffect::Revive;
    phoenix.effectAmount = 50;
    db.addItem(phoenix);

    return db;
}

Party oneKnight(const content::ContentDatabase& db) {
    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Hero"));
    return p;
}

dungeon::EnemyTeam goblinTeam(int count) {
    dungeon::EnemyTeam t;
    t.name = "Goblins";
    for (int i = 0; i < count; ++i) {
        t.enemyIds.push_back("goblin");
    }
    return t;
}

}  // namespace

TEST_CASE("battle: build creates party and enemy combatants", "[battle]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(1), db);
    REQUIRE(b.units.size() == 2);
    REQUIRE(b.units[0].side == Side::Party);
    REQUIRE(b.units[1].side == Side::Enemy);
    REQUIRE(b.units[0].hp == 120);
    REQUIRE(b.units[1].hp == 30);
}

TEST_CASE("battle: duplicate enemies get suffixed names", "[battle]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(2), db);
    REQUIRE(b.units[1].name == "Goblin A");
    REQUIRE(b.units[2].name == "Goblin B");
}

TEST_CASE("battle: physical attack uses defense and guard deterministically", "[battle]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(1), db);
    // 18 attack - 5/2 defense = 16.
    b.attack(0, 1);
    REQUIRE(b.units[1].hp == 30 - 16);

    Battle g = buildBattle(oneKnight(db), goblinTeam(1), db);
    g.units[1].guarding = true;  // halves to 8
    g.attack(0, 1);
    REQUIRE(g.units[1].hp == 30 - 8);
}

TEST_CASE("battle: magic skill spends MP and ignores half the defense", "[battle]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(1), db);
    const content::SkillDef* fireball = db.findSkill("fireball");
    // magic 4 + 14 - 5/4(=1) = 17.
    b.useSkill(0, 1, *fireball);
    REQUIRE(b.units[1].hp == 30 - 17);
    REQUIRE(b.units[0].mp == 0);  // 4 MP - 6 cost, clamped at 0
}

TEST_CASE("battle: heal restores but never resurrects", "[battle]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(1), db);
    const content::SkillDef* mend = db.findSkill("mend");
    b.units[0].hp = 50;
    b.useSkill(0, 0, *mend);  // 20 + magic(4)/2 = 22
    REQUIRE(b.units[0].hp == 72);

    b.units[0].hp = 0;
    b.useSkill(0, 0, *mend);
    REQUIRE(b.units[0].hp == 0);  // heal does nothing to a KO'd unit
}

TEST_CASE("battle: KO and outcomes", "[battle]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(1), db);
    REQUIRE(b.outcome() == Outcome::Ongoing);

    b.units[1].hp = 5;
    b.attack(0, 1);
    REQUIRE(b.units[1].hp == 0);
    REQUIRE_FALSE(b.units[1].alive());
    REQUIRE(b.outcome() == Outcome::Victory);

    Battle d = buildBattle(oneKnight(db), goblinTeam(1), db);
    d.units[0].hp = 0;
    REQUIRE(d.outcome() == Outcome::Defeat);
}

TEST_CASE("battle: a revive item brings back a KO'd ally", "[battle]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(1), db);
    const content::ItemDef* phoenix = db.findItem("phoenix_tear");
    b.units[0].hp = 0;
    b.useItem(0, 0, *phoenix);  // revive to 50% of 120
    REQUIRE(b.units[0].alive());
    REQUIRE(b.units[0].hp == 60);
}

TEST_CASE("battle: turn order favors higher speed", "[battle]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(1), db);
    // Goblin speed 9 > knight speed 8.
    const std::vector<int> order = turnOrder(b);
    REQUIRE(order.front() == 1);
}

TEST_CASE("battle: enemy AI targets a living party member", "[battle]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(1), db);
    const EnemyChoice choice = chooseEnemyAction(b, 1, db);
    REQUIRE(choice.target == 0);
}
