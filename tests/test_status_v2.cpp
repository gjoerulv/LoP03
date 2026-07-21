#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"

// M35 status effects & battle rules v2: Confusion (basic-attacks own side),
// Silence (no MP-cost skills), Blind (physical attacks usually miss), and the
// seeded to-hit roll stream. All new randomness rides Battle.rollCursor (advanced
// only when a status gates a roll), so a status-free battle is byte-identical to
// the pre-M35 rules and replay is deterministic.

using namespace cd;
using namespace cd::battle;

namespace {

Combatant partyMember(const std::string& name, int hp, int atk, int mag, int spd) {
    Combatant c;
    c.side = Side::Party;
    c.name = name;
    c.sourceId = "hero";
    c.stats = {hp, atk, mag, 10, spd};
    c.maxHp = hp;
    c.hp = hp;
    c.mp = c.maxMp = 20;
    return c;
}

Combatant enemyUnit(const std::string& name, int hp) {
    Combatant c;
    c.side = Side::Enemy;
    c.name = name;
    c.sourceId = name;
    c.stats = {hp, 18, 6, 10, 7};
    c.maxHp = hp;
    c.hp = hp;
    c.mp = c.maxMp = 20;
    return c;
}

// Two party members (a bruiser and a caster) + one durable enemy; fixed seed.
Battle board() {
    Battle b;
    b.units.push_back(partyMember("Aldo", 100, 25, 4, 8));   // 0
    b.units.push_back(partyMember("Bria", 100, 6, 25, 11));  // 1
    b.units.push_back(enemyUnit("Ogre", 400));               // 2
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 0xABCDEF12345ull;
    return b;
}

void afflict(Combatant& c, content::StatusType t, int turns) {
    c.statuses.push_back({t, 0, turns});
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

TEST_CASE("status v2: a status-free attack draws no roll and never misses", "[status]") {
    Battle b = board();
    const int before = b.units[2].hp;
    b.attack(0, 2);
    CHECK(b.units[2].hp < before);  // the hit landed
    CHECK(b.lastMissed.empty());
    CHECK(b.rollCursor == 0);  // nothing gated a roll -> byte-identical to pre-M35
}

TEST_CASE("status v2: a confused unit attacks its own side, deterministically", "[status]") {
    Battle b = board();
    afflict(b.units[0], content::StatusType::Confusion, 3);
    const int enemyBefore = b.units[2].hp;
    b.attack(0, 2);  // aimed at the enemy, but confusion overrides it
    CHECK(b.units[2].hp == enemyBefore);  // the enemy is untouched
    const bool hitOwnSide = b.units[0].hp < b.units[0].maxHp || b.units[1].hp < b.units[1].maxHp;
    CHECK(hitOwnSide);  // a party member (self or ally) took the blow

    // Same seed + same board -> same victim (reproducible).
    Battle b2 = board();
    afflict(b2.units[0], content::StatusType::Confusion, 3);
    b2.attack(0, 2);
    CHECK(b.units[0].hp == b2.units[0].hp);
    CHECK(b.units[1].hp == b2.units[1].hp);
}

TEST_CASE("status v2: a lone confused unit strikes itself and is cured by the hit", "[status]") {
    Battle b;
    b.units.push_back(partyMember("Solo", 100, 25, 4, 8));
    b.units.push_back(enemyUnit("Ogre", 400));
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 7;
    afflict(b.units[0], content::StatusType::Confusion, 3);
    b.attack(0, 1);
    CHECK(b.units[0].hp < b.units[0].maxHp);      // only itself to hit
    CHECK(b.units[1].hp == b.units[1].maxHp);
    CHECK_FALSE(isConfused(b.units[0]));          // the hit snapped it out (M35)
}

TEST_CASE("status v2: taking damage cures confusion; a non-damaging turn does not", "[status]") {
    Battle b = board();
    afflict(b.units[2], content::StatusType::Confusion, 4);
    REQUIRE(isConfused(b.units[2]));
    b.attack(0, 2);  // a clean hit on the confused enemy
    CHECK_FALSE(isConfused(b.units[2]));  // damage cured it

    // Guarding (no damage) leaves an ally's confusion in place.
    Battle b2 = board();
    afflict(b2.units[1], content::StatusType::Confusion, 4);
    b2.guard(1);
    CHECK(isConfused(b2.units[1]));
}

TEST_CASE("status v2: applied durations are doubled; poison ticks at double magnitude", "[status]") {
    Battle b = board();
    content::SkillDef venom = mkSkill("venom", content::SkillCategory::Physical, 4, 0);
    venom.statusEffect = content::StatusType::Poison;
    venom.statusMagnitude = 5;
    venom.statusDuration = 3;
    b.useSkill(0, 2, venom);
    // duration 3 -> 6
    bool found = false;
    for (const auto& s : b.units[2].statuses) {
        if (s.type == content::StatusType::Poison) {
            CHECK(s.turns == 6);
            found = true;
        }
    }
    REQUIRE(found);
    const int hp = b.units[2].hp;
    b.tickStatuses(2);
    CHECK(b.units[2].hp == hp - 10);  // magnitude 5 doubled to 10 per tick
}

TEST_CASE("status v2: silence blocks MP-cost skills but not free ones", "[status]") {
    Battle b = board();
    afflict(b.units[0], content::StatusType::Silence, 3);

    const content::SkillDef fireball = mkSkill("fireball", content::SkillCategory::Magic, 30, 5);
    const int mpBefore = b.units[0].mp;
    const int enemyBefore = b.units[2].hp;
    const std::string log = b.useSkill(0, 2, fireball);
    CHECK(b.units[0].mp == mpBefore);       // no MP spent
    CHECK(b.units[2].hp == enemyBefore);    // no damage dealt
    CHECK(log.find("silenced") != std::string::npos);

    const content::SkillDef jab = mkSkill("jab", content::SkillCategory::Physical, 10, 0);
    b.useSkill(0, 2, jab);  // a 0-MP skill still works under silence
    CHECK(b.units[2].hp < enemyBefore);
}

TEST_CASE("status v2: canCast blocks only MP-cost skills under silence", "[status]") {
    Combatant free = partyMember("A", 100, 20, 4, 8);
    Combatant silenced = partyMember("B", 100, 20, 4, 8);
    afflict(silenced, content::StatusType::Silence, 3);
    const content::SkillDef mp = mkSkill("mp", content::SkillCategory::Magic, 20, 4);
    const content::SkillDef nofee = mkSkill("free", content::SkillCategory::Physical, 10, 0);
    CHECK(canCast(free, mp));
    CHECK(canCast(free, nofee));
    CHECK_FALSE(canCast(silenced, mp));  // the only false case
    CHECK(canCast(silenced, nofee));
}

TEST_CASE("status v2: a silenced enemy falls back to a basic attack", "[status]") {
    content::ContentDatabase db;
    db.addSkill(mkSkill("bolt", content::SkillCategory::Magic, 20, 5));

    Battle b = board();
    b.units[2].skillIds = {"bolt"};
    CHECK(chooseEnemyAction(b, 2, db).useSkill);  // not silenced -> casts

    afflict(b.units[2], content::StatusType::Silence, 3);
    CHECK_FALSE(chooseEnemyAction(b, 2, db).useSkill);  // silenced -> attacks instead
}

TEST_CASE("status v2: blind makes physical attacks usually miss; magic is unaffected", "[status]") {
    Battle b = board();
    afflict(b.units[0], content::StatusType::Blind, 999);

    int misses = 0;
    const int trials = 200;
    for (int i = 0; i < trials; ++i) {
        b.units[2].hp = b.units[2].maxHp;  // keep the target alive across trials
        b.attack(0, 2);
        if (!b.lastMissed.empty()) {
            ++misses;
        }
    }
    CHECK(misses > trials / 2);  // most attacks miss (75% target)
    CHECK(misses < trials);      // but not all
    CHECK(b.rollCursor == static_cast<std::uint64_t>(trials));  // exactly one to-hit roll per attack

    // A blinded caster's magic never misses and draws no to-hit roll.
    const std::uint64_t cursorBefore = b.rollCursor;
    const content::SkillDef bolt = mkSkill("bolt", content::SkillCategory::Magic, 20, 0);
    b.units[2].hp = b.units[2].maxHp;
    b.useSkill(0, 2, bolt);
    CHECK(b.lastMissed.empty());
    CHECK(b.units[2].hp < b.units[2].maxHp);
    CHECK(b.rollCursor == cursorBefore);
}

TEST_CASE("status v2: blind miss/hit pattern is deterministic per seed", "[status]") {
    const auto run = [] {
        Battle b = board();
        afflict(b.units[0], content::StatusType::Blind, 999);
        std::vector<int> pattern;
        for (int i = 0; i < 24; ++i) {
            b.units[2].hp = b.units[2].maxHp;
            b.attack(0, 2);
            pattern.push_back(b.lastMissed.empty() ? 0 : 1);
        }
        return pattern;
    };
    CHECK(run() == run());
}

TEST_CASE("status v2: a blinded physical skill can miss and then applies no status", "[status]") {
    // A physical skill that also poisons: on a Blind miss it deals no damage and
    // inflicts no poison. Find a seed offset where the first roll misses by taking
    // several attempts; determinism guarantees a stable outcome per battle.
    Battle b = board();
    afflict(b.units[0], content::StatusType::Blind, 999);
    content::SkillDef venomstrike = mkSkill("venomstrike", content::SkillCategory::Physical, 12, 0);
    venomstrike.statusEffect = content::StatusType::Poison;
    venomstrike.statusMagnitude = 5;
    venomstrike.statusDuration = 3;

    int missedApplications = 0;
    for (int i = 0; i < 40; ++i) {
        b.units[2].hp = b.units[2].maxHp;
        b.units[2].statuses.clear();
        b.useSkill(0, 2, venomstrike);
        if (!b.lastMissed.empty()) {
            ++missedApplications;
            CHECK(b.units[2].statuses.empty());               // a miss inflicts no poison
            CHECK(b.units[2].hp == b.units[2].maxHp);         // and no damage
        }
    }
    CHECK(missedApplications > 0);  // blind actually made some of them miss
}

TEST_CASE("status v2: cure strips confusion, silence, and blind (buffs survive)", "[status]") {
    Battle b = board();
    afflict(b.units[0], content::StatusType::Confusion, 3);
    afflict(b.units[0], content::StatusType::Silence, 3);
    afflict(b.units[0], content::StatusType::Blind, 3);
    afflict(b.units[0], content::StatusType::DefenseUp, 3);  // a buff must remain

    content::ItemDef antidote;
    antidote.effect = content::ConsumableEffect::Cure;
    b.useItem(0, 0, antidote);

    REQUIRE(b.units[0].statuses.size() == 1);
    CHECK(b.units[0].statuses[0].type == content::StatusType::DefenseUp);
}
