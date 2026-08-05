// M75 — Battle rules v15: Reflect / Sleep / Curse, poison scaling, the
// strengthened ATK+/-/DEF+/- application, MP damage, battle-start statuses,
// the deterministic trigger framework, sleep-aware enemy manners, the
// Deadly-Spoon shrug, worn element resistance, and the loader rules guarding
// all of it. Model tests build combatants by hand; loader tests parse
// in-memory JSON, the test_content_validation idiom.

#include <catch2/catch_test_macros.hpp>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"

using namespace cd;

namespace {

content::Json parse(const char* text) { return content::Json::parse(text, nullptr, false); }

battle::Combatant unit(battle::Side side, const char* name, int hp, int atk, int mag, int def,
                       int spd, int mp = 50) {
    battle::Combatant c;
    c.side = side;
    c.name = name;
    c.hp = c.maxHp = hp;
    c.mp = c.maxMp = mp;
    c.stats.maxHp = hp;
    c.stats.attack = atk;
    c.stats.magic = mag;
    c.stats.defense = def;
    c.stats.speed = spd;
    return c;
}

// A two-sided battle: [0] party attacker, [1] enemy defender. Bland stats so
// expected damage is easy to derive: (atk + power) - def/2 for physical.
battle::Battle duel() {
    battle::Battle b;
    b.units.push_back(unit(battle::Side::Party, "Hero", 100, 20, 30, 8, 10));
    b.units.push_back(unit(battle::Side::Enemy, "Foe", 100, 12, 10, 10, 8));
    b.threat.assign(b.units.size(), 0);
    return b;
}

content::SkillDef skill(const char* id, content::SkillCategory cat, content::SkillTarget tgt,
                        int power, int mpCost = 0) {
    content::SkillDef s;
    s.id = id;
    s.name = id;
    s.category = cat;
    s.target = tgt;
    s.power = power;
    s.mpCost = mpCost;
    return s;
}

int statusTurns(const battle::Combatant& c, content::StatusType t) {
    for (const battle::StatusInstance& s : c.statuses) {
        if (s.type == t) {
            return s.turns;
        }
    }
    return 0;
}

int statusMagnitude(const battle::Combatant& c, content::StatusType t) {
    for (const battle::StatusInstance& s : c.statuses) {
        if (s.type == t) {
            return s.magnitude;
        }
    }
    return 0;
}

}  // namespace

TEST_CASE("v15: the rules version reached 15", "[v15]") {
    CHECK(battle::kBattleRulesVersion == 15);
}

// --- the rebalanced buffs -----------------------------------------------------

TEST_CASE("v15: a status-free strike keeps the historical formula", "[v15]") {
    battle::Battle b = duel();
    b.attack(0, 1);
    // (20 + 0) - 10/2 = 15.
    CHECK(b.units[1].hp == 100 - 15);
}

TEST_CASE("v15: ATK+ scales the whole offensive term, DEF+/- the final hit", "[v15]") {
    {
        battle::Battle b = duel();
        b.units[0].statuses.push_back({content::StatusType::AttackUp, 50, 5});
        b.attack(0, 1);
        // (20 + 0) * 150% - 5 = 25.
        CHECK(b.units[1].hp == 100 - 25);
    }
    {
        battle::Battle b = duel();
        b.units[1].statuses.push_back({content::StatusType::DefenseUp, 50, 5});
        b.attack(0, 1);
        // 15 * 100/150 = 10.
        CHECK(b.units[1].hp == 100 - 10);
    }
    {
        battle::Battle b = duel();
        b.units[1].statuses.push_back({content::StatusType::DefenseDown, 30, 5});
        b.attack(0, 1);
        // 15 * 100/70 = 21.
        CHECK(b.units[1].hp == 100 - 21);
    }
}

// --- Curse --------------------------------------------------------------------

TEST_CASE("v15: a cursed attacker deals half and pays double MP", "[v15]") {
    battle::Battle b = duel();
    b.units[0].statuses.push_back({content::StatusType::Curse, 0, 9});
    CHECK(battle::isCursed(b.units[0]));

    b.attack(0, 1);
    CHECK(b.units[1].hp == 100 - 7);  // 15 halved, integer floor

    const content::SkillDef spark =
        skill("spark", content::SkillCategory::Magic, content::SkillTarget::SingleEnemy, 10, 6);
    CHECK(battle::mpCostFor(b.units[0], spark) == 12);
    const int mpBefore = b.units[0].mp;
    b.useSkill(0, 1, spark);
    CHECK(b.units[0].mp == mpBefore - 12);
}

TEST_CASE("v15: a Curse lasts half again as long as any other status", "[v15]") {
    battle::Battle b = duel();
    content::SkillDef hex = skill("hex", content::SkillCategory::Support,
                                  content::SkillTarget::SingleEnemy, 0, 0);
    hex.statusEffect = content::StatusType::Curse;
    hex.statusDuration = 2;
    b.useSkill(0, 1, hex);
    // 2 authored x2 (the M35 mult) x1.5 (the owner's Curse rule) = 6.
    CHECK(statusTurns(b.units[1], content::StatusType::Curse) == 6);

    content::SkillDef sunder = skill("sunder", content::SkillCategory::Support,
                                     content::SkillTarget::SingleEnemy, 0, 0);
    sunder.statusEffect = content::StatusType::DefenseDown;
    sunder.statusMagnitude = 30;
    sunder.statusDuration = 2;
    b.useSkill(0, 1, sunder);
    CHECK(statusTurns(b.units[1], content::StatusType::DefenseDown) == 4);  // the control
}

TEST_CASE("v15: only uncurse and a curesCurse item lift a Curse", "[v15]") {
    battle::Battle b = duel();
    b.units.push_back(unit(battle::Side::Party, "Ally", 80, 10, 10, 8, 9));
    b.threat.assign(b.units.size(), 0);
    battle::Combatant& ally = b.units[2];
    ally.statuses.push_back({content::StatusType::Curse, 0, 9});
    ally.statuses.push_back({content::StatusType::Poison, 5, 9});
    ally.statuses.push_back({content::StatusType::Sleep, 0, 9});

    // A pure cleanse (the Purify shape) lifts poison and sleep, never the Curse.
    content::SkillDef purify = skill("purify", content::SkillCategory::Heal,
                                     content::SkillTarget::SingleAlly, 0, 0);
    purify.controlEffect = content::SkillEffect::Cleanse;
    b.useSkill(0, 2, purify);
    CHECK_FALSE(battle::hasStatus(ally, content::StatusType::Poison));
    CHECK_FALSE(battle::hasStatus(ally, content::StatusType::Sleep));
    CHECK(battle::hasStatus(ally, content::StatusType::Curse));

    // A Cure item lifts every negative status — except the Curse.
    content::ItemDef remedy;
    remedy.id = "remedy";
    remedy.name = "Remedy";
    remedy.effect = content::ConsumableEffect::Cure;
    ally.statuses.push_back({content::StatusType::Blind, 0, 9});
    b.useItem(0, 2, remedy);
    CHECK_FALSE(battle::hasStatus(ally, content::StatusType::Blind));
    CHECK(battle::hasStatus(ally, content::StatusType::Curse));

    // The uncurse skill effect is one of the two removers.
    content::SkillDef uncurse = skill("uncurse", content::SkillCategory::Support,
                                      content::SkillTarget::SingleAlly, 0, 0);
    uncurse.controlEffect = content::SkillEffect::Uncurse;
    b.useSkill(0, 2, uncurse);
    CHECK_FALSE(battle::hasStatus(ally, content::StatusType::Curse));

    // The curesCurse item flag is the other.
    ally.statuses.push_back({content::StatusType::Curse, 0, 9});
    content::ItemDef taxes;
    taxes.id = "holy_taxes";
    taxes.name = "Holy Taxes";
    taxes.curesCurse = true;
    b.useItem(0, 2, taxes);
    CHECK_FALSE(battle::hasStatus(ally, content::StatusType::Curse));
}

// --- Sleep --------------------------------------------------------------------

TEST_CASE("v15: sleep skips the turn, damage wakes, poison does not", "[v15]") {
    battle::Battle b = duel();
    b.units[1].statuses.push_back({content::StatusType::Sleep, 0, 9});
    CHECK(battle::isAsleep(b.units[1]));
    CHECK(battle::forcedActionFor(b.units[1]) == battle::ForcedAction::Skip);

    // The poison tick does NOT wake (it bypasses the damage chokepoint).
    b.units[1].statuses.push_back({content::StatusType::Poison, 5, 9});
    b.tickStatuses(1);
    CHECK(b.units[1].hp < 100);
    CHECK(battle::hasStatus(b.units[1], content::StatusType::Sleep));

    // A real hit does.
    b.attack(0, 1);
    CHECK_FALSE(battle::hasStatus(b.units[1], content::StatusType::Sleep));
}

// --- Reflect ------------------------------------------------------------------

TEST_CASE("v15: reflect bounces hostile magic (and its riders) onto the caster", "[v15]") {
    battle::Battle b = duel();
    b.units[1].statuses.push_back({content::StatusType::Reflect, 0, 9});

    content::SkillDef bolt =
        skill("bolt", content::SkillCategory::Magic, content::SkillTarget::SingleEnemy, 10, 0);
    bolt.statusEffect = content::StatusType::Poison;
    bolt.statusMagnitude = 4;
    bolt.statusDuration = 2;
    const std::uint64_t cursorBefore = b.rollCursor;
    b.useSkill(0, 1, bolt);
    // The foe is untouched; the caster ate its own spell: (30+10) - 8/4 = 38.
    CHECK(b.units[1].hp == 100);
    CHECK(b.units[0].hp == 100 - 38);
    CHECK(battle::hasStatus(b.units[0], content::StatusType::Poison));
    CHECK_FALSE(battle::hasStatus(b.units[1], content::StatusType::Poison));
    CHECK(b.rollCursor == cursorBefore);  // the mirror takes no roll

    // Physical skills do not bounce.
    const content::SkillDef stab = skill("stab", content::SkillCategory::Physical,
                                         content::SkillTarget::SingleEnemy, 5, 0);
    b.useSkill(0, 1, stab);
    CHECK(b.units[1].hp < 100);
}

TEST_CASE("v15: break_reflect shatters the mirror", "[v15]") {
    battle::Battle b = duel();
    b.units[1].statuses.push_back({content::StatusType::Reflect, 0, 9});
    content::SkillDef breaker = skill("breaker", content::SkillCategory::Physical,
                                      content::SkillTarget::SingleEnemy, 0, 0);
    breaker.controlEffect = content::SkillEffect::BreakReflect;
    b.useSkill(0, 1, breaker);
    CHECK_FALSE(battle::hasStatus(b.units[1], content::StatusType::Reflect));
}

// --- poison scaling & MP damage ----------------------------------------------

TEST_CASE("v15: poison magnitude gains the applier's Magic over 4", "[v15]") {
    battle::Battle b = duel();  // Hero MAG 30
    content::SkillDef venom = skill("venom", content::SkillCategory::Physical,
                                    content::SkillTarget::SingleEnemy, 0, 0);
    venom.statusEffect = content::StatusType::Poison;
    venom.statusMagnitude = 6;
    venom.statusDuration = 2;
    b.useSkill(0, 1, venom);
    CHECK(statusMagnitude(b.units[1], content::StatusType::Poison) == 6 + 30 / 4);

    const int hpBefore = b.units[1].hp;
    b.tickStatuses(1);
    CHECK(hpBefore - b.units[1].hp == (6 + 30 / 4) * battle::kPoisonDamageMult);
}

TEST_CASE("v15: mpDamagePct drains a share of the dealt HP damage", "[v15]") {
    battle::Battle b = duel();
    content::SkillDef sap =
        skill("sap", content::SkillCategory::Physical, content::SkillTarget::SingleEnemy, 10, 0);
    sap.mpDamagePct = 25;
    b.useSkill(0, 1, sap);
    // dealt = (20+10) - 5 = 25; MP loss = 25/4 = 6.
    CHECK(b.units[1].hp == 100 - 25);
    CHECK(b.units[1].mp == 50 - 6);
}

// --- triggers -----------------------------------------------------------------

TEST_CASE("v15: first_time_hp_below_pct fires once at the bearer's turn", "[v15]") {
    battle::Battle b = duel();
    battle::TriggerRule tr;
    tr.when = content::TriggerWhen::FirstTimeHpBelowPct;
    tr.threshold = 50;
    tr.action = content::TriggerDo::StatusSelf;
    tr.status = content::StatusType::Reflect;
    tr.duration = 2;
    b.units[1].triggers.push_back(tr);

    CHECK(b.beginUnitTurn(1).empty());  // above half: nothing
    b.units[1].hp = 40;
    const std::string fired = b.beginUnitTurn(1);
    CHECK_FALSE(fired.empty());
    CHECK(battle::hasReflect(b.units[1]));
    CHECK(b.beginUnitTurn(1).empty());  // once means once
}

TEST_CASE("v15: every_nth_hit_taken stuns the attacker on the Nth hit", "[v15]") {
    battle::Battle b = duel();
    b.units[1].hp = b.units[1].maxHp = b.units[1].stats.maxHp = 1000;  // survive the count
    battle::TriggerRule tr;
    tr.when = content::TriggerWhen::EveryNthHitTaken;
    tr.threshold = 4;
    tr.action = content::TriggerDo::StatusAttacker;
    tr.status = content::StatusType::Stunned;
    tr.duration = 2;
    b.units[1].triggers.push_back(tr);

    for (int i = 0; i < 3; ++i) {
        b.attack(0, 1);
        CHECK_FALSE(battle::hasStatus(b.units[0], content::StatusType::Stunned));
    }
    b.attack(0, 1);  // the fourth
    CHECK(battle::hasStatus(b.units[0], content::StatusType::Stunned));
}

TEST_CASE("v15: every_nth_own_turn drains the foes' MP on schedule", "[v15]") {
    battle::Battle b = duel();
    battle::TriggerRule tr;
    tr.when = content::TriggerWhen::EveryNthOwnTurn;
    tr.threshold = 2;
    tr.action = content::TriggerDo::DrainFoeMp;
    tr.mpDrainPct = 100;
    b.units[1].triggers.push_back(tr);

    b.beginUnitTurn(1);
    CHECK(b.units[0].mp == 50);  // turn 1 of 2
    b.beginUnitTurn(1);
    CHECK(b.units[0].mp == 0);  // the second turn fires
}

TEST_CASE("v15: first_time_ally_felled scales the bearer's stats", "[v15]") {
    battle::Battle b = duel();
    b.units.push_back(unit(battle::Side::Enemy, "Minion", 40, 8, 6, 6, 7));
    b.threat.assign(b.units.size(), 0);
    battle::TriggerRule tr;
    tr.when = content::TriggerWhen::FirstTimeAllyFelled;
    tr.action = content::TriggerDo::ScaleStatsSelf;
    tr.scaleAttackPct = 150;
    b.units[1].triggers.push_back(tr);

    CHECK(b.beginUnitTurn(1).empty());  // the minion still stands
    b.units[2].hp = 0;
    CHECK_FALSE(b.beginUnitTurn(1).empty());
    CHECK(b.units[1].stats.attack == 18);  // 12 * 150%
    CHECK(b.beginUnitTurn(1).empty());     // once
}

TEST_CASE("v15: status_boss aids the bearer's own boss", "[v15]") {
    battle::Battle b = duel();
    b.units[1].isBoss = true;
    b.units.push_back(unit(battle::Side::Enemy, "Minion", 40, 8, 6, 6, 7));
    b.threat.assign(b.units.size(), 0);
    battle::TriggerRule tr;
    tr.when = content::TriggerWhen::FirstTimeHpBelowPct;
    tr.threshold = 50;
    tr.action = content::TriggerDo::StatusBoss;
    tr.status = content::StatusType::Reflect;
    tr.duration = 2;
    b.units[2].triggers.push_back(tr);

    b.units[2].hp = 10;  // the minion is hurt; its king gets the mirror
    b.beginUnitTurn(2);
    CHECK(battle::hasReflect(b.units[1]));
    CHECK_FALSE(battle::hasReflect(b.units[2]));
}

// --- sleep-aware manners, the Spoon shrug, element resist ---------------------

TEST_CASE("v15: avoidSleepingTargets spares sleepers while another stands", "[v15]") {
    content::ContentDatabase db;  // empty: the AI falls back to basic attacks
    battle::Battle b;
    b.units.push_back(unit(battle::Side::Party, "Asleep", 100, 10, 10, 8, 9));
    b.units.push_back(unit(battle::Side::Party, "Awake", 100, 10, 10, 8, 9));
    b.units.push_back(unit(battle::Side::Enemy, "Watcher", 80, 12, 8, 8, 8));
    b.threat.assign(b.units.size(), 0);
    b.units[0].statuses.push_back({content::StatusType::Sleep, 0, 9});
    b.units[2].avoidSleepingTargets = true;

    const battle::EnemyChoice polite = battle::chooseEnemyAction(b, 2, db);
    CHECK(polite.target == 1);

    // Everyone asleep: the manners no longer apply.
    b.units[1].statuses.push_back({content::StatusType::Sleep, 0, 9});
    const battle::EnemyChoice fair = battle::chooseEnemyAction(b, 2, db);
    CHECK((fair.target == 0 || fair.target == 1));
}

TEST_CASE("v15: noStunWhileAllFoesSleep shelves the stun skill", "[v15]") {
    content::ContentDatabase db;
    {
        content::LoadReport rep;
        const content::Json root = parse(R"({"version":1,"skills":[
            {"id":"allstun","name":"Sheet Storm","category":"physical",
             "target":"all_enemies","power":5,
             "statusEffect":"stunned","statusDuration":2}]})");
        content::parseSkills(root, "mem", db, rep);
        REQUIRE(rep.ok());
    }
    battle::Battle b;
    b.units.push_back(unit(battle::Side::Party, "Asleep", 100, 10, 10, 8, 9));
    b.units.push_back(unit(battle::Side::Enemy, "Duck", 200, 14, 10, 10, 8));
    b.threat.assign(b.units.size(), 0);
    b.units[0].statuses.push_back({content::StatusType::Sleep, 0, 9});
    b.units[1].skillIds = {"allstun"};

    battle::EnemyChoice rude = battle::chooseEnemyAction(b, 1, db);
    CHECK(rude.useSkill);  // without the manners the stun is the best pick

    b.units[1].noStunWhileAllFoesSleep = true;
    battle::EnemyChoice polite = battle::chooseEnemyAction(b, 1, db);
    CHECK_FALSE(polite.useSkill);  // shelved: it swings instead
}

TEST_CASE("v15: an immuneToStatScale boss shrugs off the Spoon", "[v15]") {
    battle::Battle b = duel();
    b.units[1].statScaleImmune = true;
    content::ItemDef spoon;
    spoon.id = "spoon";
    spoon.name = "Deadly Spoon";
    spoon.battleTarget = content::BattleTarget::Enemy;
    spoon.statScalePct = 50;

    CHECK_FALSE(battle::itemAffects(b, 1, spoon));  // the caller keeps the item
    b.useItem(0, 1, spoon);
    CHECK(b.units[1].stats.attack == 12);  // unchanged
    CHECK_FALSE(b.units[1].statDiminished);
}

TEST_CASE("v15: worn element resistance reduces what remains", "[v15]") {
    battle::Combatant d = unit(battle::Side::Party, "Warded", 100, 10, 10, 8, 9);
    d.elementResist[static_cast<std::size_t>(content::Element::Fire)] = 50;
    CHECK(battle::elementModifier(d, content::Element::Fire) == 50);   // neutral halved
    CHECK(battle::elementModifier(d, content::Element::Ice) == 100);   // untouched
    d.weaknesses.push_back(content::Element::Fire);
    CHECK(battle::elementModifier(d, content::Element::Fire) == 75);   // weak resisted
    d.immunities.push_back(content::Element::Fire);
    CHECK(battle::elementModifier(d, content::Element::Fire) == 0);    // immunity absolute
}

TEST_CASE("v15: statusImmunities blocks at the chokepoint", "[v15]") {
    battle::Battle b = duel();
    b.units[1].statusImmunities.push_back(content::StatusType::Sleep);
    content::SkillDef lullaby = skill("lullaby", content::SkillCategory::Support,
                                      content::SkillTarget::SingleEnemy, 0, 0);
    lullaby.statusEffect = content::StatusType::Sleep;
    lullaby.statusDuration = 2;
    b.useSkill(0, 1, lullaby);
    CHECK_FALSE(battle::hasStatus(b.units[1], content::StatusType::Sleep));
    CHECK(battle::isImmuneTo(b.units[1], content::StatusType::Sleep));
}

// --- buildBattle integration: initialStatuses + the prebuilt clone ------------

TEST_CASE("v15: initialStatuses and the clone slot resolve at buildBattle", "[v15]") {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::parseClasses(parse(R"({"version":1,"classes":[
        {"id":"hero","name":"Hero","baseStats":{"hp":100,"attack":20,"magic":10,
         "defense":8,"speed":10}}]})"),
                          "mem", db, rep);
    content::parseBosses(parse(R"({"version":1,"bosses":[
        {"id":"wyrm","name":"Wyrm","archetype":"brute",
         "stats":{"hp":200,"attack":20,"magic":16,"defense":12,"speed":11},
         "initialStatuses":[{"type":"reflect","duration":2}],
         "statusImmunities":["sleep","stunned"],
         "immuneToStatScale":true,
         "triggers":[{"when":"first_time_hp_below_pct","threshold":10,
                      "do":"summon_clone","cloneHpPct":5}]}]})"),
                         "mem", db, rep);
    REQUIRE(rep.ok());

    Party party;
    party.members.push_back(createCharacter(*db.findClass("hero"), "Hero", 1));
    dungeon::EnemyTeam team;
    team.isBoss = true;
    team.bossId = "wyrm";

    battle::Battle b = battle::buildBattle(party, team, db);
    REQUIRE(b.units.size() == 3);  // hero, wyrm, the dead clone slot

    const battle::Combatant& wyrm = b.units[1];
    CHECK(battle::hasReflect(wyrm));  // battle-start status
    CHECK(wyrm.statScaleImmune);
    CHECK(battle::isImmuneTo(wyrm, content::StatusType::Sleep));
    CHECK_FALSE(battle::isImmuneTo(wyrm, content::StatusType::Poison));

    const battle::Combatant& clone = b.units[2];
    CHECK(clone.summonSlot);
    CHECK_FALSE(clone.alive());
    CHECK(clone.maxHp == 200 * 5 / 100);
    CHECK(clone.triggers.empty());
    CHECK_FALSE(clone.isBoss);
    CHECK(b.outcome() == battle::Outcome::Ongoing);  // the dead slot blocks nothing

    // The summon trigger raises it.
    battle::Battle raised = b;
    raised.units[1].hp = 10;
    raised.beginUnitTurn(1);
    CHECK(raised.units[2].alive());
    CHECK(raised.units[2].hp == raised.units[2].maxHp);
}

// --- loader rules -------------------------------------------------------------

TEST_CASE("v15: loader rejects misauthored new fields", "[v15]") {
    const auto skillErrors = [](const char* json) {
        content::ContentDatabase db;
        content::LoadReport rep;
        content::parseSkills(parse(json), "mem", db, rep);
        return !rep.ok();
    };
    // MP damage on a heal.
    CHECK(skillErrors(R"({"version":1,"skills":[
        {"id":"x","name":"X","category":"heal","target":"single_ally","mpDamagePct":25}]})"));
    // A magic reflect-breaker would bounce off the mirror.
    CHECK(skillErrors(R"({"version":1,"skills":[
        {"id":"x","name":"X","category":"magic","target":"single_enemy",
         "control":"break_reflect"}]})"));
    // An uncurse aimed at the enemy.
    CHECK(skillErrors(R"({"version":1,"skills":[
        {"id":"x","name":"X","category":"support","target":"single_enemy",
         "control":"uncurse"}]})"));

    const auto enemyErrors = [](const char* json) {
        content::ContentDatabase db;
        content::LoadReport rep;
        content::parseEnemies(parse(json), "mem", db, rep);
        return !rep.ok();
    };
    // status_attacker without the hit condition.
    CHECK(enemyErrors(R"({"version":1,"enemies":[
        {"id":"e","name":"E","role":"bruiser",
         "stats":{"hp":10,"attack":1,"magic":1,"defense":1,"speed":1},
         "triggers":[{"when":"every_nth_own_turn","threshold":2,
                      "do":"status_attacker","status":"stunned","duration":2}]}]})"));
    // summon_clone is boss-only.
    CHECK(enemyErrors(R"({"version":1,"enemies":[
        {"id":"e","name":"E","role":"bruiser",
         "stats":{"hp":10,"attack":1,"magic":1,"defense":1,"speed":1},
         "triggers":[{"when":"first_time_hp_below_pct","threshold":50,
                      "do":"summon_clone","cloneHpPct":5}]}]})"));
    // A status action without a status.
    CHECK(enemyErrors(R"({"version":1,"enemies":[
        {"id":"e","name":"E","role":"bruiser",
         "stats":{"hp":10,"attack":1,"magic":1,"defense":1,"speed":1},
         "triggers":[{"when":"first_time_hp_below_pct","threshold":50,
                      "do":"status_self"}]}]})"));
    // 'none' has no place in an immunity list.
    CHECK(enemyErrors(R"({"version":1,"enemies":[
        {"id":"e","name":"E","role":"bruiser",
         "stats":{"hp":10,"attack":1,"magic":1,"defense":1,"speed":1},
         "statusImmunities":["none"]}]})"));

    const auto itemErrors = [](const char* json) {
        content::ContentDatabase db;
        content::LoadReport rep;
        content::parseItems(parse(json), "mem", db, rep);
        return !rep.ok();
    };
    // A resist percent without its elements (and vice versa).
    CHECK(itemErrors(R"({"version":1,"items":[
        {"id":"i","name":"I","type":"equipment","slot":"accessory","resistPct":50}]})"));
    CHECK(itemErrors(R"({"version":1,"items":[
        {"id":"i","name":"I","type":"equipment","slot":"accessory",
         "resistElements":["fire"]}]})"));
    // Worn resistance belongs to equipment.
    CHECK(itemErrors(R"({"version":1,"items":[
        {"id":"i","name":"I","type":"consumable","resistPct":50,
         "resistElements":["fire"]}]})"));
}

TEST_CASE("v15: well-formed new fields load", "[v15]") {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::parseItems(parse(R"({"version":1,"items":[
        {"id":"ember_ward","name":"Ember Ward","type":"equipment","slot":"accessory",
         "resistPct":50,"resistElements":["fire"]},
        {"id":"holy_taxes","name":"Holy Taxes","type":"consumable","curesCurse":true}]})"),
                        "mem", db, rep);
    REQUIRE(rep.ok());
    const content::ItemDef* ward = db.findItem("ember_ward");
    REQUIRE(ward != nullptr);
    CHECK(ward->resistPct == 50);
    REQUIRE(ward->resistElements.size() == 1);
    CHECK(ward->resistElements[0] == content::Element::Fire);
    const content::ItemDef* taxes = db.findItem("holy_taxes");
    REQUIRE(taxes != nullptr);
    CHECK(taxes->curesCurse);
}
