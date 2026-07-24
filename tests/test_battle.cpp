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

// M53 debug god mode. The flag only exists in non-shipping builds (it is
// compiled out of Release), so these cases are guarded the same way — Release
// still builds and runs the rest of the suite.
#ifndef CRYSTAL_SHIPPING_BUILD
TEST_CASE("battle: god mode keeps the party alive but still kills enemies", "[battle][debug]") {
    const content::ContentDatabase db = makeDb();

    // Baseline (flag off, the default): a lethal hit KOs the party unit.
    Battle base = buildBattle(oneKnight(db), goblinTeam(1), db);
    base.units[0].hp = 1;
    base.attack(1, 0);  // goblin strikes the knight for 2 -> would be 0
    CHECK(base.units[0].hp == 0);
    CHECK_FALSE(base.units[0].alive());

    // Flag on: the same lethal hit leaves the party unit at 1 HP.
    Battle god = buildBattle(oneKnight(db), goblinTeam(1), db);
    god.debugPartyUnkillable = true;
    god.units[0].hp = 1;
    god.attack(1, 0);
    CHECK(god.units[0].hp == 1);
    CHECK(god.units[0].alive());

    // God mode never shields ENEMIES: a lethal party hit still kills the goblin.
    Battle kill = buildBattle(oneKnight(db), goblinTeam(1), db);
    kill.debugPartyUnkillable = true;
    kill.units[1].hp = 1;
    kill.attack(0, 1);  // knight strikes the goblin
    CHECK(kill.units[1].hp == 0);
    CHECK_FALSE(kill.units[1].alive());
}

TEST_CASE("battle: god mode clamps the poison tick that bypasses applyDamage",
          "[battle][debug]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(1), db);
    b.debugPartyUnkillable = true;
    b.units[0].hp = 1;
    b.units[0].statuses.push_back({content::StatusType::Poison, 50, 3});  // 100 dmg
    b.tickStatuses(0);
    CHECK(b.units[0].hp == 1);  // would have dropped to 0 without the clamp
    CHECK(b.units[0].alive());
}

TEST_CASE("battle: god mode does not consume Iron Will", "[battle][debug]") {
    const content::ContentDatabase db = makeDb();
    Battle b = buildBattle(oneKnight(db), goblinTeam(1), db);
    b.debugPartyUnkillable = true;
    b.units[0].ironWill = true;
    b.units[0].hp = 1;
    b.attack(1, 0);  // lethal; god mode handles it before Iron Will is checked
    CHECK(b.units[0].hp == 1);
    CHECK_FALSE(b.units[0].ironWillUsed);  // still available
}

TEST_CASE("battle: god mode off leaves the roll stream byte-identical", "[battle][debug]") {
    const content::ContentDatabase db = makeDb();
    // A blinded attacker draws from the roll stream. Whether the flag is left at
    // its default or set false explicitly, the outcome and rollCursor must match.
    Battle a = buildBattle(oneKnight(db), goblinTeam(1), db);
    Battle b = buildBattle(oneKnight(db), goblinTeam(1), db);
    b.debugPartyUnkillable = false;  // explicit; a leaves it defaulted
    a.units[0].statuses.push_back({content::StatusType::Blind, 0, 3});
    b.units[0].statuses.push_back({content::StatusType::Blind, 0, 3});
    a.attack(0, 1);
    b.attack(0, 1);
    CHECK(a.rollCursor == b.rollCursor);
    CHECK(a.units[1].hp == b.units[1].hp);
}
#endif  // CRYSTAL_SHIPPING_BUILD
