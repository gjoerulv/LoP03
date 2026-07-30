#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "game/Milestones.hpp"
#include "game/Party.hpp"
#include "save/SaveSystem.hpp"

// M63 — class level milestones (battle rules v14): the milestones.json data
// shape, the pending/chosen logic, the refreshCharacter stat effects, the
// buildBattle resolution, each new engine behaviour, sim==live agreement, and
// the save round-trip with defensive drops.

using namespace cd;
using namespace cd::battle;

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

Combatant hero(const std::string& name, int hp = 120, int atk = 25, int mag = 10) {
    Combatant c;
    c.side = Side::Party;
    c.name = name;
    c.sourceId = "hero";
    c.stats = {hp, atk, mag, 10, 12};
    c.maxHp = c.hp = hp;
    c.mp = c.maxMp = 40;
    return c;
}

Combatant ogre(int hp = 400, int atk = 20) {
    Combatant c;
    c.side = Side::Enemy;
    c.name = "Ogre";
    c.sourceId = "ogre";
    c.stats = {hp, atk, 8, 10, 7};
    c.maxHp = c.hp = hp;
    c.mp = c.maxMp = 20;
    return c;
}

Battle duel() {
    Battle b;
    b.units.push_back(hero("Hero"));
    b.units.push_back(ogre());
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 0x63C0FFEEull;
    return b;
}

int enemyHp(const Battle& b) { return b.units[1].hp; }

}  // namespace

// ---------------------------------------------------------------- data shape

TEST_CASE("milestones: the shipped table is 9 classes x 3 tiers x a/b", "[milestone][content]") {
    const content::ContentDatabase db = loadContent();
    CHECK(db.milestoneCount() == 54);
    for (const auto& [classId, cls] : db.classes()) {
        for (int tier : kMilestoneTiers) {
            const auto pair = db.milestonePair(classId, tier);
            INFO(classId << " tier " << tier);
            REQUIRE(pair.first != nullptr);   // option a
            REQUIRE(pair.second != nullptr);  // option b
            CHECK(pair.first->option == "a");
            CHECK(pair.second->option == "b");
            CHECK(pair.first->effect != content::MilestoneEffect::None);
            CHECK(pair.second->effect != content::MilestoneEffect::None);
            CHECK_FALSE(pair.first->name.empty());
            CHECK_FALSE(pair.second->description.empty());
        }
    }
    CHECK(kBattleRulesVersion >= 14);
}

TEST_CASE("milestones: the loader rejects bad tiers, options and lone pairs",
          "[milestone][content]") {
    const auto parse = [](const char* body) {
        content::ContentDatabase db;
        content::LoadReport rep;
        // A real class for the reference check.
        content::ClassDef cls;
        cls.id = "knight";
        cls.name = "Knight";
        db.addClass(cls);
        content::parseMilestones(nlohmann::json::parse(body), "milestones.json", db, rep);
        content::validateReferences(db, rep);
        return rep.errorCount();
    };
    // A valid complete pair parses clean.
    CHECK(parse(R"({"version":1,"milestones":[
        {"id":"a1","classId":"knight","level":10,"option":"a","name":"A","description":"d","effect":"stat_max_hp_pct","magnitude":10},
        {"id":"b1","classId":"knight","level":10,"option":"b","name":"B","description":"d","effect":"grant_counter"}]})") == 0);
    // Bad tier.
    CHECK(parse(R"({"version":1,"milestones":[
        {"id":"a1","classId":"knight","level":15,"option":"a","name":"A","description":"d","effect":"grant_counter"}]})") > 0);
    // Bad option.
    CHECK(parse(R"({"version":1,"milestones":[
        {"id":"a1","classId":"knight","level":10,"option":"c","name":"A","description":"d","effect":"grant_counter"}]})") > 0);
    // Unknown effect.
    CHECK(parse(R"({"version":1,"milestones":[
        {"id":"a1","classId":"knight","level":10,"option":"a","name":"A","description":"d","effect":"win_always"}]})") > 0);
    // Unknown class.
    CHECK(parse(R"({"version":1,"milestones":[
        {"id":"a1","classId":"paladin","level":10,"option":"a","name":"A","description":"d","effect":"grant_counter"},
        {"id":"b1","classId":"paladin","level":10,"option":"b","name":"B","description":"d","effect":"grant_counter"}]})") > 0);
    // A lone option is an incomplete choice.
    CHECK(parse(R"({"version":1,"milestones":[
        {"id":"a1","classId":"knight","level":10,"option":"a","name":"A","description":"d","effect":"grant_counter"}]})") > 0);
}

// ------------------------------------------------------- pending / stat side

TEST_CASE("milestones: pending tiers follow the level, oldest first", "[milestone]") {
    const content::ContentDatabase db = loadContent();
    Character c = createCharacter(*db.findClass("knight"), "Rolan", 9);
    CHECK(pendingMilestoneTier(c, db) == 0);
    c.level = 10;
    CHECK(pendingMilestoneTier(c, db) == 10);
    c.level = 35;  // an old save far past every tier: 10 asks first
    CHECK(pendingMilestoneTier(c, db) == 10);
    c.milestone10 = "knight_10_a";
    CHECK(pendingMilestoneTier(c, db) == 20);
    c.milestone20 = "knight_20_b";
    CHECK(pendingMilestoneTier(c, db) == 30);
    c.milestone30 = "knight_30_a";
    CHECK(pendingMilestoneTier(c, db) == 0);
    CHECK(chosenMilestone(c, 20, db)->name == "Executioner");
}

TEST_CASE("milestones: stat bonuses land in refreshCharacter", "[milestone]") {
    const content::ContentDatabase db = loadContent();
    Character plain = createCharacter(*db.findClass("knight"), "Plain", 10);
    Character tough = createCharacter(*db.findClass("knight"), "Tough", 10);
    tough.milestone10 = "knight_10_b";  // Battle-Hardened: +10% max HP
    refreshCharacter(plain, db);
    refreshCharacter(tough, db);
    CHECK(tough.maxHp == plain.maxHp + plain.maxHp / 10);

    Character deep = createCharacter(*db.findClass("mage"), "Deep", 10);
    Character base = createCharacter(*db.findClass("mage"), "Base", 10);
    deep.milestone10 = "mage_10_a";  // Deep Reserves: +20% max MP
    refreshCharacter(base, db);
    refreshCharacter(deep, db);
    CHECK(deep.maxMp == base.maxMp + base.maxMp * 20 / 100);
    CHECK(deep.maxHp == base.maxHp);  // nothing else moved
}

TEST_CASE("milestones: buildBattle resolves the chosen battle effects", "[milestone]") {
    const content::ContentDatabase db = loadContent();
    Party p;
    Character c = createCharacter(*db.findClass("knight"), "Rolan", 30);
    c.milestone10 = "knight_10_a";  // Heavy Swing
    c.milestone20 = "knight_20_a";  // Riposte (Counter)
    c.milestone30 = "knight_30_a";  // Bulwark (opening DEF+)
    refreshCharacter(c, db);
    p.members.push_back(c);
    dungeon::EnemyTeam team;
    const Battle b = buildBattle(p, team, db);
    REQUIRE(b.units.size() == 1);
    CHECK(b.units[0].basicAttackPct == 15);
    CHECK(b.units[0].counterAttack);
    CHECK(hasStatus(b.units[0], content::StatusType::DefenseUp));  // Bulwark, 2 ticks

    // A member with NO choices resolves fully inert.
    Party p2;
    p2.members.push_back(createCharacter(*db.findClass("knight"), "Blank", 30));
    const Battle b2 = buildBattle(p2, team, db);
    CHECK(b2.units[0].basicAttackPct == 0);
    CHECK_FALSE(b2.units[0].counterAttack);
    CHECK(b2.units[0].statuses.empty());
}

// ------------------------------------------------------------ engine effects

TEST_CASE("milestone fx: basic attack bonus and execute compose", "[milestone][battle]") {
    Battle plain = duel();
    plain.attack(0, 1);
    const int plainDmg = 400 - enemyHp(plain);

    Battle heavy = duel();
    heavy.units[0].basicAttackPct = 20;
    heavy.attack(0, 1);
    const int heavyDmg = 400 - enemyHp(heavy);
    CHECK(heavyDmg == plainDmg * 120 / 100);

    Battle exec = duel();
    exec.units[1].hp = 150;  // below half of 400
    exec.units[0].executePct = 20;
    exec.attack(0, 1);
    CHECK(150 - enemyHp(exec) == plainDmg * 120 / 100);
}

TEST_CASE("milestone fx: deep guard blocks more than half", "[milestone][battle]") {
    Battle plain = duel();
    plain.units[0].guarding = true;
    plain.attack(1, 0);
    const int hitGuarded = 120 - plain.units[0].hp;

    Battle deep = duel();
    deep.units[0].guarding = true;
    deep.units[0].guardBlockPct = 65;
    deep.attack(1, 0);
    const int hitDeep = 120 - deep.units[0].hp;
    CHECK(hitDeep < hitGuarded);
}

TEST_CASE("milestone fx: the first hit glances off an Immovable guardian",
          "[milestone][battle]") {
    Battle b = duel();
    b.units[0].firstHitImmune = true;
    b.attack(1, 0);
    CHECK(b.units[0].hp == 120);  // the first blow deals 0
    b.attack(1, 0);
    CHECK(b.units[0].hp < 120);  // the shield is spent
}

TEST_CASE("milestone fx: Iron Constitution survives then surges", "[milestone][battle]") {
    Battle b = duel();
    b.units[0].hp = 5;
    b.units[0].ironWill = true;
    b.units[0].ironWillHealPct = 25;
    b.attack(1, 0);  // lethal
    CHECK(b.units[0].alive());
    CHECK(b.units[0].hp == 1 + 120 * 25 / 100);
}

TEST_CASE("milestone fx: double strike and the reduced sweep", "[milestone][battle]") {
    Battle plain = duel();
    plain.attack(0, 1);
    const int one = 400 - enemyHp(plain);

    Battle twice = duel();
    twice.units[0].doubleStrikePct = 50;
    twice.attack(0, 1);
    CHECK(400 - enemyHp(twice) == one + std::max(1, one * 50 / 100));

    Battle sweep = duel();
    sweep.units.push_back(ogre());  // a second foe
    sweep.threat.assign(sweep.units.size(), 0);
    sweep.units[0].attackHitsAll = true;
    sweep.units[0].sweepScalePct = 60;
    sweep.attack(0, 1);
    const int perFoe = std::max(1, one * 60 / 100);
    CHECK(400 - sweep.units[1].hp == perFoe);
    CHECK(400 - sweep.units[2].hp == perFoe);
}

TEST_CASE("milestone fx: Purifying Light and Blessed Renew", "[milestone][battle]") {
    const content::ContentDatabase db = loadContent();
    const content::SkillDef* purify = db.findSkill("purify");
    const content::SkillDef* renew = db.findSkill("renew");
    REQUIRE(purify != nullptr);
    REQUIRE(renew != nullptr);
    REQUIRE(renew->reviveHpPct == 20);

    Battle b = duel();
    b.units[0].hp = 50;
    b.units[0].purifyHeals = true;
    b.useSkill(0, 0, *purify);
    CHECK(b.units[0].hp == 50 + b.units[0].stats.magic / 2);  // the pre-M62 heal, by choice

    Battle r = duel();
    r.units.push_back(hero("Fallen"));
    r.threat.assign(r.units.size(), 0);
    r.units[2].hp = 0;
    r.units[0].reviveAtPct = 35;
    r.useSkill(0, 2, *renew);
    CHECK(r.units[2].hp == std::max(1, r.units[2].maxHp * 35 / 100));
}

TEST_CASE("milestone fx: lingering statuses, the taunt curse, and generosity",
          "[milestone][battle]") {
    const content::ContentDatabase db = loadContent();

    // Lingering Hex: an authored 3-turn status normally sticks 6 ticks; the
    // bonus makes it 7.
    content::SkillDef hex;
    hex.id = "hex";
    hex.name = "Hex";
    hex.category = content::SkillCategory::Support;
    hex.target = content::SkillTarget::SingleEnemy;
    hex.statusEffect = content::StatusType::AttackDown;
    hex.statusMagnitude = 10;
    hex.statusDuration = 3;
    Battle b = duel();
    b.units[0].statusTurnsBonus = 1;
    b.useSkill(0, 1, hex);
    REQUIRE(b.units[1].statuses.size() == 1);
    CHECK(b.units[1].statuses[0].turns == 3 * kStatusDurationMult + 1);

    // Intimidating Taunt: the goad also lands ATK- on every foe.
    const content::SkillDef* taunt = db.findSkill("taunt");
    REQUIRE(taunt != nullptr);
    Battle t = duel();
    t.units[0].tauntDebuffPct = 10;
    t.units[0].mp = 40;
    t.useSkill(0, 0, *taunt);
    CHECK(hasStatus(t.units[1], content::StatusType::AttackDown));

    // Selective Generosity: the goose's heal no longer braces the enemies.
    const content::SkillDef* mending = db.findSkill("generous_mending");
    REQUIRE(mending != nullptr);
    REQUIRE(mending->alsoBuffsEnemies);
    Battle g = duel();
    g.units[0].noEnemyBuff = true;
    g.units[0].mp = 40;
    g.useSkill(0, 0, *mending);
    CHECK_FALSE(hasStatus(g.units[1], content::StatusType::DefenseUp));
}

TEST_CASE("milestone fx: standing ovation and the last laugh", "[milestone][battle]") {
    // The Jester fells a foe: the party rallies.
    Battle kill = duel();
    kill.units.push_back(hero("Ally"));
    kill.threat.assign(kill.units.size(), 0);
    kill.units[0].onKillPartyAtkUpPct = 10;
    kill.units[1].hp = 1;
    kill.attack(0, 1);
    REQUIRE_FALSE(kill.units[1].alive());
    CHECK(hasStatus(kill.units[0], content::StatusType::AttackUp));
    CHECK(hasStatus(kill.units[2], content::StatusType::AttackUp));

    // The Jester falls: every foe is cursed — including via a poison tick.
    Battle die = duel();
    die.units[0].onDeathFoeDebuffPct = 10;
    die.units[0].hp = 1;
    die.attack(1, 0);
    REQUIRE_FALSE(die.units[0].alive());
    CHECK(hasStatus(die.units[1], content::StatusType::AttackDown));
    CHECK(hasStatus(die.units[1], content::StatusType::DefenseDown));

    Battle poison = duel();
    poison.units[0].onDeathFoeDebuffPct = 10;
    poison.units[0].hp = 1;
    poison.units[0].statuses.push_back({content::StatusType::Poison, 6, 4});
    poison.tickStatuses(0);
    REQUIRE_FALSE(poison.units[0].alive());
    CHECK(hasStatus(poison.units[1], content::StatusType::AttackDown));
}

TEST_CASE("milestone fx: the weakness override and the gold bonus helper",
          "[milestone][battle]") {
    Battle plain = duel();
    plain.units[1].weaknesses = {content::Element::Fire};
    plain.units[0].weaponElement = content::Element::Fire;
    plain.attack(0, 1);
    const int weak = 400 - enemyHp(plain);

    Battle tuned = duel();
    tuned.units[1].weaknesses = {content::Element::Fire};
    tuned.units[0].weaponElement = content::Element::Fire;
    tuned.units[0].weaknessBonusPct = 170;
    tuned.attack(0, 1);
    CHECK(400 - enemyHp(tuned) > weak);

    const content::ContentDatabase db = loadContent();
    std::vector<Character> members;
    Character rogue = createCharacter(*db.findClass("rogue"), "Sly", 10);
    rogue.milestone10 = "rogue_10_b";  // Cutpurse +15%
    members.push_back(rogue);
    CHECK(partyGoldBonusPct(members, db) == 15);
    members[0].hp = 0;  // a fallen cutpurse pockets nothing
    CHECK(partyGoldBonusPct(members, db) == 0);
}

TEST_CASE("milestones: sim and live resolve a milestone-heavy turn identically",
          "[milestone][determinism]") {
    const content::ContentDatabase db = loadContent();
    const auto board = [&] {
        Battle b = duel();
        b.units[0].basicAttackPct = 15;
        b.units[0].doubleStrikePct = 50;
        b.units[0].executePct = 20;
        return b;
    };
    Battle live = board();
    live.attack(0, 1);

    Battle sim = board();
    EnemyChoice choice;
    choice.target = 1;
    applyChoice(sim, 0, choice, db);

    CHECK(sim.units[1].hp == live.units[1].hp);
    CHECK(sim.rollCursor == live.rollCursor);
}

TEST_CASE("milestones: choices survive the save and stale ids drop", "[milestone][save]") {
    const content::ContentDatabase db = loadContent();
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "crystal_milestone_save_test";
    std::filesystem::remove_all(dir);
    save::SaveSystem saves(db, dir);

    Party p;
    Character c = createCharacter(*db.findClass("guardian"), "Wall", 30);
    c.milestone10 = "guardian_10_a";
    c.milestone20 = "guardian_20_b";
    c.milestone30 = "guardian_30_a";
    refreshCharacter(c, db);
    p.members.push_back(c);

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep));
    REQUIRE(loaded.members.size() == 1);
    CHECK(loaded.members[0].milestone10 == "guardian_10_a");
    CHECK(loaded.members[0].milestone20 == "guardian_20_b");
    CHECK(loaded.members[0].milestone30 == "guardian_30_a");
    CHECK(loaded.members[0].maxHp == c.maxHp);  // Stalwart's +15% re-derived

    // A stale or foreign id is dropped on load, so the tier simply re-asks.
    p.members[0].milestone20 = "knight_20_a";  // wrong class
    p.members[0].milestone30 = "gone_forever";  // unknown id
    REQUIRE(saves.save(save::SaveSlot::Manual2, p, rep));
    Party dropped;
    REQUIRE(saves.load(save::SaveSlot::Manual2, dropped, rep));
    CHECK(dropped.members[0].milestone10 == "guardian_10_a");
    CHECK(dropped.members[0].milestone20.empty());
    CHECK(dropped.members[0].milestone30.empty());
    CHECK(pendingMilestoneTier(dropped.members[0], db) == 20);
    std::filesystem::remove_all(dir);
}
