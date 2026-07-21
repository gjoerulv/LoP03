#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"

// M36 passive skills: the 10 deterministic sim hooks, resolution from data, and
// the byte-identical guarantee for a battle with no passive present.

using namespace cd;
using namespace cd::battle;

namespace {

Combatant party(const std::string& name, int hp, int atk, int mag, int def, int spd) {
    Combatant c;
    c.side = Side::Party;
    c.name = name;
    c.sourceId = "hero";
    c.stats = {hp, atk, mag, def, spd};
    c.maxHp = hp;
    c.hp = hp;
    c.mp = c.maxMp = 20;
    return c;
}

Combatant enemyUnit(const std::string& name, int hp, int atk) {
    Combatant c;
    c.side = Side::Enemy;
    c.name = name;
    c.sourceId = name;
    c.stats = {hp, atk, 8, 10, 7};
    c.maxHp = hp;
    c.hp = hp;
    c.mp = c.maxMp = 20;
    return c;
}

// One party bruiser + one durable enemy; fixed seed.
Battle duel(int partyAtk = 25, int enemyHp = 400) {
    Battle b;
    b.units.push_back(party("Hero", 120, partyAtk, 6, 10, 12));
    b.units.push_back(enemyUnit("Ogre", enemyHp, 20));
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 0x1234ABCDull;
    return b;
}

content::SkillDef mkSkill(const std::string& id, content::SkillCategory cat, int power, int mp) {
    content::SkillDef s;
    s.id = id;
    s.name = id;
    s.category = cat;
    s.target = content::SkillTarget::SingleEnemy;
    s.power = power;
    s.mpCost = mp;
    return s;
}

}  // namespace

TEST_CASE("passive: a battle with no passive draws no roll and deals base damage", "[passive]") {
    Battle b = duel();
    const int before = b.units[1].hp;
    b.attack(0, 1);
    CHECK(b.units[1].hp < before);
    CHECK(b.rollCursor == 0);  // no passive/status gated a roll -> identical to v2
    CHECK(b.lastMissed.empty());
}

TEST_CASE("passive: Evasion misses physical attacks; a Blind attacker always misses", "[passive]") {
    Battle b = duel();
    b.units[1].evasionPct = 25;
    int misses = 0;
    const int trials = 200;
    for (int i = 0; i < trials; ++i) {
        b.units[1].hp = b.units[1].maxHp;
        b.attack(0, 1);
        if (!b.lastMissed.empty()) {
            ++misses;
        }
    }
    CHECK(misses > 0);
    CHECK(misses < trials / 2);  // ~25%, well under half
    CHECK(b.rollCursor == static_cast<std::uint64_t>(trials));

    // Blind attacker vs an evader -> 100% miss (owner rule).
    Battle b2 = duel();
    b2.units[1].evasionPct = 25;
    b2.units[0].statuses.push_back({content::StatusType::Blind, 0, 999});
    for (int i = 0; i < 20; ++i) {
        b2.units[1].hp = b2.units[1].maxHp;
        b2.attack(0, 1);
        CHECK_FALSE(b2.lastMissed.empty());  // every attack misses
    }
}

TEST_CASE("passive: Spell Ward fizzles some hostile magic; no ward means no roll", "[passive]") {
    Battle b = duel();
    b.units[1].spellWardPct = 25;
    const content::SkillDef bolt = mkSkill("bolt", content::SkillCategory::Magic, 20, 0);
    int warded = 0;
    const int trials = 200;
    for (int i = 0; i < trials; ++i) {
        const int hp = b.units[1].hp = b.units[1].maxHp;
        b.useSkill(0, 1, bolt);
        if (b.units[1].hp == hp) {
            ++warded;  // no damage -> the spell fizzled
        }
    }
    CHECK(warded > 0);
    CHECK(warded < trials / 2);

    Battle clean = duel();
    clean.useSkill(0, 1, bolt);  // no spell ward -> no roll
    CHECK(clean.rollCursor == 0);
}

TEST_CASE("passive: Thorns reflects a share of physical damage to the attacker", "[passive]") {
    Battle b = duel();
    b.units[1].thornsPct = 20;
    const int attackerBefore = b.units[0].hp;
    b.attack(0, 1);
    CHECK(b.units[0].hp < attackerBefore);  // the attacker took thorns damage
}

TEST_CASE("passive: Lifedrink heals a share of the physical damage dealt", "[passive]") {
    Battle b = duel();
    b.units[0].hp = 40;  // hurt, so the heal is visible below the cap
    b.units[0].lifedrinkPct = 15;
    const int before = b.units[0].hp;
    b.attack(0, 1);
    CHECK(b.units[0].hp > before);  // healed from the damage it dealt
}

TEST_CASE("passive: Clarity regenerates MP each round and grants Silence immunity", "[passive]") {
    Battle b = duel();
    b.units[0].clarityMp = 3;
    b.units[0].silenceImmune = true;
    b.units[0].mp = 10;
    b.beginRound();
    CHECK(b.units[0].mp == 13);

    b.units[0].statuses.push_back({content::StatusType::Silence, 0, 3});
    CHECK_FALSE(isSilenced(b.units[0]));  // immune -> not silenced
    const content::SkillDef spell = mkSkill("spell", content::SkillCategory::Magic, 10, 4);
    CHECK(canCast(b.units[0], spell));  // can still cast
}

TEST_CASE("passive: Iron Will survives a lethal blow once, then falls", "[passive]") {
    Battle b = duel(500, 400);  // a hard-hitting hero
    b.units[1].ironWill = true;
    b.units[1].hp = 30;
    b.attack(0, 1);  // a blow that would kill
    CHECK(b.units[1].hp == 1);        // survived at 1 HP
    CHECK(b.units[1].ironWillUsed);
    b.units[1].hp = 30;
    b.attack(0, 1);                    // second lethal blow
    CHECK_FALSE(b.units[1].alive());  // no second save
}

TEST_CASE("passive: First Strike acts first in round 1 and boosts the first hit", "[passive]") {
    Battle b;
    b.units.push_back(party("Slow", 120, 25, 6, 10, 5));   // 0: slow, First Strike
    b.units.push_back(enemyUnit("Fast", 400, 20));         // 1: faster
    b.units[1].stats.speed = 30;
    b.units[0].firstStrike = true;
    b.units[0].firstStrikeBonusPct = 50;
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 5;

    std::vector<int> order = turnOrder(b);
    CHECK(order.front() == 0);  // the First-Strike unit goes first despite lower speed

    // The first damaging action is boosted; a later one is not.
    Battle b2 = b;
    const int e0 = b2.units[1].hp;
    b2.attack(0, 1);
    const int firstHit = e0 - b2.units[1].hp;
    b2.units[0].actedOnce = false;  // allow a "second" comparison hit
    const int e1 = b2.units[1].hp;
    b2.attack(0, 1);
    const int secondHit = e1 - b2.units[1].hp;
    CHECK(firstHit > secondHit);  // +50% only on the first
}

TEST_CASE("passive: Bodyguard soaks a share of a hit on the weakest ally", "[passive]") {
    Battle b;
    b.units.push_back(enemyUnit("Attacker", 400, 40));   // 0: enemy hits the party
    b.units.push_back(party("Weak", 100, 10, 4, 8, 9));  // 1: lowest HP -> protected
    b.units.push_back(party("Guard", 200, 10, 4, 8, 9)); // 2: bodyguard
    b.units[1].hp = 20;                                   // clearly the weakest
    b.units[2].bodyguardPct = 25;
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 9;

    const int guardBefore = b.units[2].hp;
    const int weakBefore = b.units[1].hp;
    b.attack(0, 1);  // enemy strikes the weakest ally
    CHECK(b.units[2].hp < guardBefore);        // the guard soaked a share
    CHECK(b.units[1].hp < weakBefore);         // the weak ally still took the rest
}

TEST_CASE("passive: Keen Senses is Blind-immune and hits debuffed targets harder", "[passive]") {
    // Blind immunity.
    Battle b = duel();
    b.units[0].blindImmune = true;
    b.units[0].statuses.push_back({content::StatusType::Blind, 0, 999});
    CHECK_FALSE(isBlinded(b.units[0]));
    for (int i = 0; i < 10; ++i) {
        b.units[1].hp = b.units[1].maxHp;
        b.attack(0, 1);
        CHECK(b.lastMissed.empty());  // immune -> never a blind miss
    }

    // Bonus vs a debuffed target: isolate the +10% by holding the debuff fixed
    // and toggling only Keen Senses.
    Battle noKeen = duel();
    noKeen.units[1].statuses.push_back({content::StatusType::DefenseDown, 25, 3});
    const int n0 = noKeen.units[1].hp;
    noKeen.attack(0, 1);
    const int without = n0 - noKeen.units[1].hp;

    Battle keen = duel();
    keen.units[0].keenSensesPct = 10;
    keen.units[1].statuses.push_back({content::StatusType::DefenseDown, 25, 3});
    const int k0 = keen.units[1].hp;
    keen.attack(0, 1);
    const int with = k0 - keen.units[1].hp;
    CHECK(with > without);  // +10% strictly more vs the same debuffed target
}

TEST_CASE("passive: Counter Attack retaliates once per round", "[passive]") {
    Battle b;
    b.units.push_back(enemyUnit("Attacker", 400, 30));
    b.units.push_back(party("Defender", 300, 22, 4, 8, 9));
    b.units[1].counterAttack = true;
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 3;

    const int attackerBefore = b.units[0].hp;
    b.attack(0, 1);  // enemy hits the defender, who counters
    CHECK(b.units[0].hp < attackerBefore);
    CHECK(b.units[1].counteredThisRound);

    const int afterFirst = b.units[0].hp;
    b.attack(0, 1);  // a second hit the same round -> no second counter
    CHECK(b.units[0].hp == afterFirst);

    b.beginRound();  // rearm
    CHECK_FALSE(b.units[1].counteredThisRound);
    b.attack(0, 1);
    CHECK(b.units[0].hp < afterFirst);  // counters again next round
}

TEST_CASE("passive: buildBattle resolves a character's equipped passive", "[passive]") {
    content::ContentDatabase db;
    content::ClassDef knight;
    knight.id = "knight";
    knight.name = "Knight";
    knight.baseStats = {120, 20, 4, 12, 8};
    db.addClass(knight);
    content::PassiveDef ev;
    ev.id = "evasion";
    ev.name = "Evasion";
    ev.hook = content::PassiveHook::Evasion;
    ev.magnitude = 25;
    db.addPassive(ev);

    Party p;
    Character c = createCharacter(knight, "Hero");
    c.equippedPassive = "evasion";
    p.members.push_back(c);

    dungeon::EnemyTeam team;  // empty enemy team is fine for resolution
    Battle b = buildBattle(p, team, db);
    REQUIRE(b.units.size() == 1);
    CHECK(b.units[0].evasionPct == 25);
    REQUIRE(b.units[0].passiveIds.size() == 1);
    CHECK(b.units[0].passiveIds[0] == "evasion");
}
