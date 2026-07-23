#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/Enums.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"

// M48 — elements (battle rules v8). Weakness x150 %, immunity 0, applied by one
// pure `elementModifier` inside the two damage helpers that BattleState and the
// Simulator both reach. Covers the modifier math, the after-the-floor placement
// that makes an immune hit truly 0, immunity blocking status riders, weapon
// element on basic attacks, the loader/validators, sim == live, and the shipped
// content's own invariants.

using namespace cd;
using namespace cd::battle;
using content::Element;

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

Combatant hero(const std::string& name) {
    Combatant c;
    c.side = Side::Party;
    c.name = name;
    c.sourceId = "hero";
    c.stats = {100, 30, 30, 10, 10};
    c.maxHp = c.hp = 100;
    c.mp = c.maxMp = 60;
    return c;
}

Combatant foe(const std::string& name) {
    Combatant c;
    c.side = Side::Enemy;
    c.name = name;
    c.sourceId = name;
    c.stats = {500, 10, 10, 10, 5};
    c.maxHp = c.hp = 500;
    c.mp = c.maxMp = 20;
    return c;
}

// Unit 0 attacker, unit 1 the tagged foe, unit 2 an untagged control.
Battle board() {
    Battle b;
    b.units.push_back(hero("Aldo"));
    b.units.push_back(foe("Tagged"));
    b.units.push_back(foe("Control"));
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 0xE1E3E7ull;
    return b;
}

content::SkillDef magicSkill(const std::string& id, Element e, int power) {
    content::SkillDef s;
    s.id = id;
    s.name = id;
    s.category = content::SkillCategory::Magic;
    s.target = content::SkillTarget::SingleEnemy;
    s.element = e;
    s.power = power;
    return s;
}

int hpLost(const Battle& b, std::size_t unit, int before) {
    return before - b.units[unit].hp;
}

content::LoadReport parseItemsFrom(const std::string& json, content::ContentDatabase& db) {
    content::LoadReport rep;
    content::parseItems(content::Json::parse(json), "test", db, rep);
    return rep;
}

content::LoadReport parseEnemiesFrom(const std::string& json, content::ContentDatabase& db) {
    content::LoadReport rep;
    content::parseEnemies(content::Json::parse(json), "test", db, rep);
    return rep;
}

}  // namespace

TEST_CASE("elements: the v8 element rules are in force", "[battle][elements]") {
    // The exact pin lives with the newest rules milestone's suite (M49 = 9), so
    // a later bump does not have to edit every past milestone's tests.
    CHECK(kBattleRulesVersion >= 8);
}

TEST_CASE("elements: modifier math", "[battle][elements]") {
    Combatant c = foe("Dummy");
    c.weaknesses = {Element::Fire};
    c.immunities = {Element::Ice};

    CHECK(elementModifier(c, Element::Fire) == 150);
    CHECK(elementModifier(c, Element::Ice) == 0);
    CHECK(elementModifier(c, Element::None) == 100);      // unelemented always lands
    CHECK(elementModifier(c, Element::Holy) == 100);      // neither weak nor immune
    CHECK(elementModifier(foe("Plain"), Element::Fire) == 100);  // untagged foe

    CHECK(isImmuneToElement(c, Element::Ice));
    CHECK_FALSE(isImmuneToElement(c, Element::Fire));
}

TEST_CASE("elements: a weak hit deals 150% and an immune hit deals nothing",
          "[battle][elements]") {
    const content::SkillDef fire = magicSkill("test_fire", Element::Fire, 20);
    const content::SkillDef plain = magicSkill("test_plain", Element::None, 20);

    // Baseline: the same skill on an untagged foe.
    Battle base = board();
    base.useSkill(0, 2, plain);
    const int normal = 500 - base.units[2].hp;
    REQUIRE(normal > 0);

    Battle weak = board();
    weak.units[1].weaknesses = {Element::Fire};
    weak.useSkill(0, 1, fire);
    CHECK(500 - weak.units[1].hp == normal * 150 / 100);

    Battle immune = board();
    immune.units[1].immunities = {Element::Fire};
    immune.useSkill(0, 1, fire);
    // Exactly zero — NOT the damage helpers' max(1, ...) floor, which is why the
    // modifier is applied after it.
    CHECK(immune.units[1].hp == 500);
}

TEST_CASE("elements: an immune target takes no status rider", "[battle][elements]") {
    content::SkillDef curse = magicSkill("test_curse", Element::Ice, 15);
    curse.statusEffect = content::StatusType::DefenseDown;
    curse.statusMagnitude = 20;
    curse.statusDuration = 3;

    Battle hit = board();
    hit.useSkill(0, 2, curse);  // untagged control: the rider lands
    CHECK_FALSE(hit.units[2].statuses.empty());

    Battle blocked = board();
    blocked.units[1].immunities = {Element::Ice};
    blocked.useSkill(0, 1, curse);
    CHECK(blocked.units[1].hp == 500);
    CHECK(blocked.units[1].statuses.empty());  // nothing of the spell reached it
}

TEST_CASE("elements: an immune basic attack applies no attack-status", "[battle][elements]") {
    // The Dragon's bite (M45) on a foe immune to the wielded weapon's element.
    Battle b = board();
    b.units[0].weaponElement = Element::Fire;
    b.units[0].attackStatuses.push_back({content::StatusType::Poison, 5, 3});
    b.units[1].immunities = {Element::Fire};

    b.attack(0, 1);
    CHECK(b.units[1].hp == 500);
    CHECK(b.units[1].statuses.empty());

    // The same attacker against an untagged foe still poisons it.
    b.attack(0, 2);
    CHECK(b.units[2].hp < 500);
    CHECK_FALSE(b.units[2].statuses.empty());
}

TEST_CASE("elements: a basic attack carries the wielder's weapon element",
          "[battle][elements]") {
    Battle plain = board();
    plain.attack(0, 1);  // no weapon element: a tagged foe is irrelevant
    const int unelemented = 500 - plain.units[1].hp;

    Battle weak = board();
    weak.units[0].weaponElement = Element::Holy;
    weak.units[1].weaknesses = {Element::Holy};
    weak.attack(0, 1);
    CHECK(500 - weak.units[1].hp == unelemented * 150 / 100);

    // Enemies hold no weapons, so their swings are never elemental.
    Battle enemySwing = board();
    enemySwing.units[0].weaknesses = {Element::Fire};
    enemySwing.attack(1, 0);
    CHECK(enemySwing.units[1].weaponElement == Element::None);
}

TEST_CASE("elements: an immune hit is marked, and it is not a miss", "[battle][elements]") {
    Battle b = board();
    b.units[0].weaponElement = Element::Fire;
    b.units[1].immunities = {Element::Fire};
    const std::string log = b.attack(0, 1);

    CHECK(b.lastImmune.size() == 1);
    CHECK(b.lastImmune[0] == 1);
    CHECK(b.lastMissed.empty());  // it connected; it just did nothing
    CHECK(log.find("immune") != std::string::npos);

    // Marks describe only the latest action.
    b.attack(0, 2);
    CHECK(b.lastImmune.empty());
}

TEST_CASE("elements: a weak hit is marked", "[battle][elements]") {
    Battle b = board();
    b.units[0].weaponElement = Element::Holy;
    b.units[1].weaknesses = {Element::Holy};
    b.attack(0, 1);
    CHECK(b.lastWeak.size() == 1);
    CHECK(b.lastWeak[0] == 1);
}

TEST_CASE("elements: a counter-attack carries the counter-attacker's weapon",
          "[battle][elements]") {
    Battle b = board();
    b.units[1].counterAttack = true;
    b.units[1].weaponElement = Element::Fire;  // an armed foe, for the rule's sake
    b.units[0].weaknesses = {Element::Fire};
    const int before = b.units[0].hp;
    b.attack(0, 1);
    const int withWeakness = before - b.units[0].hp;

    Battle plain = board();
    plain.units[1].counterAttack = true;
    plain.units[1].weaponElement = Element::Fire;
    const int plainBefore = plain.units[0].hp;
    plain.attack(0, 1);
    const int normal = plainBefore - plain.units[0].hp;

    REQUIRE(normal > 0);
    CHECK(withWeakness == normal * 150 / 100);
}

TEST_CASE("elements: guarding and affinity compose", "[battle][elements]") {
    const content::SkillDef fire = magicSkill("test_fire", Element::Fire, 20);

    Battle guarded = board();
    guarded.units[1].weaknesses = {Element::Fire};
    guarded.units[1].guarding = true;
    guarded.useSkill(0, 1, fire);
    const int guardedWeak = 500 - guarded.units[1].hp;

    Battle open = board();
    open.units[1].weaknesses = {Element::Fire};
    open.useSkill(0, 1, fire);
    const int openWeak = 500 - open.units[1].hp;

    REQUIRE(guardedWeak > 0);
    CHECK(guardedWeak < openWeak);  // the guard still halves a weak hit
}

TEST_CASE("elements: the sim and live play resolve a tagged hit identically",
          "[battle][elements][determinism]") {
    const content::ContentDatabase db = loadContent();
    const content::SkillDef* fireball = db.findSkill("fireball");
    REQUIRE(fireball != nullptr);
    REQUIRE(fireball->element == Element::Fire);

    Battle live = board();
    live.units[1].weaknesses = {Element::Fire};
    live.units[0].skillIds = {"fireball"};
    live.useSkill(0, 1, *fireball);

    Battle sim = board();
    sim.units[1].weaknesses = {Element::Fire};
    sim.units[0].skillIds = {"fireball"};
    EnemyChoice choice;
    choice.useSkill = true;
    choice.skillId = "fireball";
    choice.target = 1;
    applyChoice(sim, 0, choice, db);

    CHECK(sim.units[1].hp == live.units[1].hp);
    CHECK(sim.units[0].mp == live.units[0].mp);
    CHECK(sim.rollCursor == live.rollCursor);  // elements draw no randomness
}

TEST_CASE("elements: an untagged fight resolves exactly as before", "[battle][elements]") {
    // Every pre-M48 encounter must be byte-identical: no foe tagged, no weapon
    // element, so every modifier is 100.
    const content::SkillDef plain = magicSkill("test_plain", Element::None, 20);
    Battle b = board();
    const int before = b.units[1].hp;
    b.useSkill(0, 1, plain);
    const int dealt = hpLost(b, 1, before);
    CHECK(dealt > 0);
    CHECK(b.lastWeak.empty());
    CHECK(b.lastImmune.empty());
}

// --- Loader / validation ----------------------------------------------------

TEST_CASE("elements: the loader reads optional affinities", "[content][elements]") {
    content::ContentDatabase db;
    const content::LoadReport rep = parseEnemiesFrom(R"({
        "version": 1,
        "enemies": [
          { "id": "e1", "name": "Tagged", "role": "bruiser",
            "stats": { "hp": 10, "attack": 1, "magic": 1, "defense": 1, "speed": 1 },
            "weaknesses": ["fire"], "immunities": ["ice"] },
          { "id": "e2", "name": "Plain", "role": "bruiser",
            "stats": { "hp": 10, "attack": 1, "magic": 1, "defense": 1, "speed": 1 } }
        ]})",
                                                     db);
    CHECK(rep.errorCount() == 0);
    const content::EnemyDef* tagged = db.findEnemy("e1");
    REQUIRE(tagged != nullptr);
    CHECK(tagged->affinity.weakTo(Element::Fire));
    CHECK(tagged->affinity.immuneTo(Element::Ice));
    CHECK(tagged->affinity.any());

    const content::EnemyDef* plain = db.findEnemy("e2");
    REQUIRE(plain != nullptr);
    CHECK_FALSE(plain->affinity.any());  // absent fields = no affinity
}

TEST_CASE("elements: overlapping weakness and immunity is a content error",
          "[content][elements]") {
    content::ContentDatabase db;
    const content::LoadReport rep = parseEnemiesFrom(R"({
        "version": 1,
        "enemies": [
          { "id": "e1", "name": "Confused", "role": "bruiser",
            "stats": { "hp": 10, "attack": 1, "magic": 1, "defense": 1, "speed": 1 },
            "weaknesses": ["fire"], "immunities": ["fire"] }
        ]})",
                                                     db);
    CHECK(rep.errorCount() > 0);
    CHECK(db.findEnemy("e1") == nullptr);  // the invalid entry is not admitted
}

TEST_CASE("elements: unknown and 'none' element names are errors", "[content][elements]") {
    content::ContentDatabase db;
    const content::LoadReport unknown = parseEnemiesFrom(R"({
        "version": 1,
        "enemies": [
          { "id": "e1", "name": "Bad", "role": "bruiser",
            "stats": { "hp": 10, "attack": 1, "magic": 1, "defense": 1, "speed": 1 },
            "weaknesses": ["custard"] }
        ]})",
                                                        db);
    CHECK(unknown.errorCount() > 0);

    content::ContentDatabase db2;
    const content::LoadReport none = parseEnemiesFrom(R"({
        "version": 1,
        "enemies": [
          { "id": "e2", "name": "Nonesuch", "role": "bruiser",
            "stats": { "hp": 10, "attack": 1, "magic": 1, "defense": 1, "speed": 1 },
            "immunities": ["none"] }
        ]})",
                                                     db2);
    CHECK(none.errorCount() > 0);
}

TEST_CASE("elements: only a weapon may carry an element", "[content][elements]") {
    content::ContentDatabase weapons;
    const content::LoadReport ok = parseItemsFrom(R"({
        "version": 1,
        "items": [
          { "id": "w", "name": "Ember Blade", "type": "equipment", "slot": "weapon",
            "element": "fire" }
        ]})",
                                                  weapons);
    CHECK(ok.errorCount() == 0);
    REQUIRE(weapons.findItem("w") != nullptr);
    CHECK(weapons.findItem("w")->element == Element::Fire);

    content::ContentDatabase armor;
    const content::LoadReport bad = parseItemsFrom(R"({
        "version": 1,
        "items": [
          { "id": "a", "name": "Ember Plate", "type": "equipment", "slot": "armor",
            "element": "fire" }
        ]})",
                                                   armor);
    CHECK(bad.errorCount() > 0);
}

// --- Shipped content invariants --------------------------------------------

TEST_CASE("elements: shipped affinities are disjoint and sparse", "[content][elements][lint]") {
    const content::ContentDatabase db = loadContent();
    int taggedFoes = 0;
    for (const auto& [id, def] : db.enemies()) {
        INFO("enemy " << id);
        for (Element w : def.affinity.weaknesses) {
            CHECK_FALSE(def.affinity.immuneTo(w));
        }
        taggedFoes += def.affinity.any() ? 1 : 0;
    }
    for (const auto& [id, def] : db.bosses()) {
        INFO("boss " << id);
        for (Element w : def.affinity.weaknesses) {
            CHECK_FALSE(def.affinity.immuneTo(w));
        }
        taggedFoes += def.affinity.any() ? 1 : 0;
    }
    CHECK(taggedFoes > 0);  // the layer is actually used by the shipped content
}

TEST_CASE("elements: no weapon element can ever be nullified", "[content][elements][lint]") {
    // The M48 no-dead-weapon rule. A skill can be swapped for another, but a
    // basic attack cannot — and a skill-less class (the Dragon) has nothing else.
    // So no shipped foe may be immune to an element any shipped WEAPON carries.
    const content::ContentDatabase db = loadContent();
    std::vector<Element> weaponElements;
    for (const auto& [id, def] : db.items()) {
        if (def.element != Element::None) {
            INFO("item " << id);
            CHECK(def.slot == content::EquipSlot::Weapon);
            weaponElements.push_back(def.element);
        }
    }
    REQUIRE_FALSE(weaponElements.empty());

    for (Element e : weaponElements) {
        INFO("weapon element " << content::toString(e));
        for (const auto& [id, def] : db.enemies()) {
            INFO("enemy " << id);
            CHECK_FALSE(def.affinity.immuneTo(e));
        }
        for (const auto& [id, def] : db.bosses()) {
            INFO("boss " << id);
            CHECK_FALSE(def.affinity.immuneTo(e));
        }
    }
}

TEST_CASE("elements: shipped tags reach the battle model through buildBattle",
          "[content][elements][battle]") {
    // End-to-end evidence that the curation is actually wired: the Frost Monarch
    // is authored weak to fire and immune to ice, and a real built battle must
    // show both.
    const content::ContentDatabase db = loadContent();
    const content::SkillDef* fireball = db.findSkill("fireball");
    const content::SkillDef* blizzard = db.findSkill("blizzard");
    const content::SkillDef* arcane = db.findSkill("arcane_burst");  // untagged control
    REQUIRE(fireball != nullptr);
    REQUIRE(blizzard != nullptr);
    REQUIRE(arcane != nullptr);

    Party party;
    const content::ClassDef* mage = db.findClass("mage");
    REQUIRE(mage != nullptr);
    party.members.push_back(createCharacter(*mage, "Ilka", 50));

    dungeon::EnemyTeam team;
    team.bossId = "frost_monarch";
    team.statScalePct = 100;

    const auto bossHpAfter = [&](const content::SkillDef& skill) {
        battle::Battle b = battle::buildBattle(party, team, db);
        const std::size_t boss = 1;  // party member 0, boss 1
        REQUIRE(b.units[boss].sourceId == "frost_monarch");
        b.units[0].mp = b.units[0].maxMp = 999;
        const int before = b.units[boss].hp;
        b.useSkill(0, static_cast<int>(boss), skill);
        return before - b.units[boss].hp;
    };

    const int iceDealt = bossHpAfter(*blizzard);
    const int fireDealt = bossHpAfter(*fireball);
    const int plainDealt = bossHpAfter(*arcane);

    CHECK(iceDealt == 0);       // immune: an ice storm on a frozen sovereign
    CHECK(fireDealt > 0);
    CHECK(plainDealt > 0);
    // Fire is amplified relative to its own unmodified damage. Compare against a
    // hand-built copy of the same fight with the tags stripped.
    battle::Battle stripped = battle::buildBattle(party, team, db);
    stripped.units[1].weaknesses.clear();
    stripped.units[1].immunities.clear();
    stripped.units[0].mp = stripped.units[0].maxMp = 999;
    const int baseline = stripped.units[1].hp;
    stripped.useSkill(0, 1, *fireball);
    const int unmodified = baseline - stripped.units[1].hp;
    CHECK(fireDealt == unmodified * 150 / 100);
}

TEST_CASE("elements: every shipped weapon element is reachable as a weakness",
          "[content][elements][lint]") {
    // A weapon element that no foe is weak to is dead flavor; this keeps the
    // curated sets honest with each other.
    const content::ContentDatabase db = loadContent();
    for (const auto& [itemId, item] : db.items()) {
        if (item.element == Element::None) {
            continue;
        }
        bool anyWeak = false;
        for (const auto& [id, def] : db.enemies()) {
            anyWeak = anyWeak || def.affinity.weakTo(item.element);
        }
        for (const auto& [id, def] : db.bosses()) {
            anyWeak = anyWeak || def.affinity.weakTo(item.element);
        }
        INFO("weapon " << itemId << " element " << content::toString(item.element));
        CHECK(anyWeak);
    }
}
