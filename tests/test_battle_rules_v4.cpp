#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"

// M43 battle rules v4: confusion forces a basic attack on BOTH sides (the rule
// lives in one shared function, so the Simulator and the battle screen cannot
// drift apart the way they had), revive-capable heal skills, effect-filtered
// battle-item targeting, and King-context item amounts. Everything here is pure
// and seeded — no new randomness was introduced.

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
    c.mp = c.maxMp = 30;
    return c;
}

Combatant enemyUnit(const std::string& name, int hp, int mag = 6) {
    Combatant c;
    c.side = Side::Enemy;
    c.name = name;
    c.sourceId = name;
    c.stats = {hp, 18, mag, 10, 7};
    c.maxHp = hp;
    c.hp = hp;
    c.mp = c.maxMp = 30;
    return c;
}

Battle board() {
    Battle b;
    b.units.push_back(partyMember("Aldo", 100, 25, 4, 8));   // 0
    b.units.push_back(partyMember("Bria", 100, 6, 25, 11));  // 1
    b.units.push_back(enemyUnit("Ogre", 400));               // 2
    b.units.push_back(enemyUnit("Hexer", 200, 22));          // 3
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 0xABCDEF12345ull;
    return b;
}

void afflict(Combatant& c, content::StatusType t, int turns, int magnitude = 0) {
    c.statuses.push_back({t, magnitude, turns});
}

content::SkillDef mkHeal(const std::string& id, int power, int mp, int revivePct = 0) {
    content::SkillDef s;
    s.id = id;
    s.name = id;
    s.category = content::SkillCategory::Heal;
    s.target = content::SkillTarget::SingleAlly;
    s.power = power;
    s.mpCost = mp;
    s.reviveHpPct = revivePct;
    return s;
}

content::ItemDef mkItem(const std::string& id, content::ConsumableEffect effect, int amount) {
    content::ItemDef d;
    d.id = id;
    d.name = id;
    d.type = content::ItemType::Consumable;
    d.effect = effect;
    d.effectAmount = amount;
    return d;
}

content::ContentDatabase shippedContent() {
    content::ContentDatabase db;
    content::LoadReport report;
    REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, report));
    return db;
}

bool contains(const std::vector<int>& v, int value) {
    return std::find(v.begin(), v.end(), value) != v.end();
}

Party smallParty(const content::ContentDatabase& db) {
    Party p;
    for (const char* cls : {"knight", "cleric"}) {
        if (const content::ClassDef* c = db.findClass(cls)) {
            p.members.push_back(createCharacter(*c, cls, 20));
        }
    }
    return p;
}

}  // namespace

// --- Confusion is enforced in shared code (audit #1 / #6) --------------------

TEST_CASE("rules v4: a confused enemy is forced to a basic attack", "[battle][confusion]") {
    content::ContentDatabase db;
    content::SkillDef curse;
    curse.id = "curse";
    curse.name = "Curse";
    curse.category = content::SkillCategory::Magic;
    curse.target = content::SkillTarget::SingleEnemy;
    curse.power = 30;
    curse.mpCost = 4;
    db.addSkill(curse);

    Battle b = board();
    b.units[3].skillIds = {"curse"};

    // Unafflicted, the caster casts.
    const EnemyChoice sane = chooseEnemyAction(b, 3, db);
    CHECK(sane.useSkill);

    // Confused, it cannot choose at all.
    afflict(b.units[3], content::StatusType::Confusion, 3);
    const EnemyChoice mad = chooseEnemyAction(b, 3, db);
    CHECK_FALSE(mad.useSkill);
    CHECK(mad.target >= 0);
}

TEST_CASE("rules v4: confusion immunity still lets a unit act", "[battle][confusion]") {
    content::ContentDatabase db;
    Battle b = board();
    b.units[3].confusionImmune = true;  // the King (M40)
    afflict(b.units[3], content::StatusType::Confusion, 3);
    const EnemyChoice choice = chooseEnemyAction(b, 3, db);
    CHECK(choice.target >= 0);
    CHECK_FALSE(isConfused(b.units[3]));  // immune: never treated as afflicted
}

TEST_CASE("rules v4: the confused action is a redirected swing at its own side",
          "[battle][confusion]") {
    Battle b = board();
    afflict(b.units[0], content::StatusType::Confusion, 3);
    const EnemyChoice choice = confusedChoice(b, 0);
    REQUIRE_FALSE(choice.useSkill);
    const int enemyBefore = b.units[2].hp;
    b.attack(0, choice.target);
    CHECK(b.units[2].hp == enemyBefore);  // the nominal foe is untouched
    CHECK((b.units[0].hp < b.units[0].maxHp || b.units[1].hp < b.units[1].maxHp));
}

TEST_CASE("rules v4: a confused party member acts identically in sim and live",
          "[battle][confusion][simulator]") {
    content::ContentDatabase db;
    content::SkillDef bolt;
    bolt.id = "bolt";
    bolt.name = "Bolt";
    bolt.category = content::SkillCategory::Magic;
    bolt.target = content::SkillTarget::SingleEnemy;
    bolt.power = 40;
    bolt.mpCost = 4;
    db.addSkill(bolt);

    // "Live": the battle screen's path — confusedChoice, then attack().
    Battle live = board();
    live.units[1].skillIds = {"bolt"};
    afflict(live.units[1], content::StatusType::Confusion, 4);
    const EnemyChoice choice = confusedChoice(live, 1);
    live.attack(1, choice.target);

    // "Sim": the same turn through the Simulator's own party AI. Both routes go
    // through the one shared rule, so the resulting boards must match.
    Battle sim = board();
    sim.units[1].skillIds = {"bolt"};
    afflict(sim.units[1], content::StatusType::Confusion, 4);
    const EnemyChoice simChoice = choosePartyAction(sim, 1, db);
    CHECK_FALSE(simChoice.useSkill);  // the sim no longer lets a confused caster cast
    CHECK(simChoice.target == choice.target);
    sim.attack(1, simChoice.target);

    REQUIRE(live.units.size() == sim.units.size());
    for (std::size_t i = 0; i < live.units.size(); ++i) {
        CHECK(live.units[i].hp == sim.units[i].hp);
        CHECK(live.units[i].mp == sim.units[i].mp);
    }
    CHECK(live.rollCursor == sim.rollCursor);
}

// --- Revive-capable heal skills (feature 8) ---------------------------------

TEST_CASE("rules v4: a revive-capable heal raises a KO'd ally at its fixed share",
          "[battle][revive]") {
    Battle b = board();
    b.units[0].hp = 0;  // Aldo is down
    const content::SkillDef renew = mkHeal("renew", 12, 9, 20);
    b.useSkill(1, 0, renew);
    CHECK(b.units[0].alive());
    CHECK(b.units[0].hp == b.units[0].maxHp * 20 / 100);  // the skill's power never applies
}

TEST_CASE("rules v4: a revive-capable heal still heals the living", "[battle][revive]") {
    Battle b = board();
    b.units[0].hp = 40;
    const content::SkillDef renew = mkHeal("renew", 12, 9, 20);
    b.useSkill(1, 0, renew);
    CHECK(b.units[0].hp > 40);
    CHECK(b.units[0].hp <= b.units[0].maxHp);
}

TEST_CASE("rules v4: an ordinary heal still cannot raise the fallen", "[battle][revive]") {
    Battle b = board();
    b.units[0].hp = 0;
    const content::SkillDef mend = mkHeal("mend", 20, 5);  // reviveHpPct 0
    b.useSkill(1, 0, mend);
    CHECK_FALSE(b.units[0].alive());
}

TEST_CASE("rules v4: only a revive-capable skill may be aimed at the fallen",
          "[battle][revive][targeting]") {
    Battle b = board();
    b.units[0].hp = 0;
    const content::SkillDef mend = mkHeal("mend", 20, 5);
    const content::SkillDef renew = mkHeal("renew", 12, 9, 20);
    const std::vector<int> plain = skillAllyTargets(b, Side::Party, mend);
    const std::vector<int> raising = skillAllyTargets(b, Side::Party, renew);
    CHECK_FALSE(contains(plain, 0));
    CHECK(contains(plain, 1));
    CHECK(contains(raising, 0));
    CHECK(contains(raising, 1));
}

// --- Battle-item targeting by effect (feature 9) ----------------------------

TEST_CASE("rules v4: heal/MP/cure items reach the living, revives only the fallen",
          "[battle][items][targeting]") {
    Battle b = board();
    b.units[0].hp = 0;

    const content::ItemDef potion = mkItem("potion", content::ConsumableEffect::Heal, 30);
    const content::ItemDef ether = mkItem("ether", content::ConsumableEffect::RestoreMp, 20);
    const content::ItemDef remedy = mkItem("remedy", content::ConsumableEffect::Cure, 0);
    const content::ItemDef tear = mkItem("tear", content::ConsumableEffect::Revive, 25);

    for (const content::ItemDef* living : {&potion, &ether, &remedy}) {
        const std::vector<int> targets = itemTargets(b, Side::Party, *living);
        CHECK_FALSE(contains(targets, 0));
        CHECK(contains(targets, 1));
    }
    const std::vector<int> raise = itemTargets(b, Side::Party, tear);
    CHECK(contains(raise, 0));
    CHECK_FALSE(contains(raise, 1));
}

TEST_CASE("rules v4: an item with no legal target has an empty candidate list",
          "[battle][items][targeting]") {
    Battle b = board();  // nobody is down
    const content::ItemDef tear = mkItem("tear", content::ConsumableEffect::Revive, 25);
    CHECK(itemTargets(b, Side::Party, tear).empty());
}

// --- Royal Snacks & the King context (feature 4) ----------------------------

TEST_CASE("rules v4: Royal Snacks are a nibble normally and a feast against the King",
          "[battle][items][king]") {
    content::ItemDef snacks = mkItem("royal_snacks", content::ConsumableEffect::Heal, 10);
    snacks.curesDebuffs = true;
    snacks.kingEffectAmount = 100;
    snacks.kingMpAmount = 10;

    Battle ordinary = board();
    ordinary.units[0].hp = 10;
    ordinary.units[0].mp = 0;
    ordinary.useItem(1, 0, snacks);
    CHECK(ordinary.units[0].hp == 20);
    CHECK(ordinary.units[0].mp == 0);  // no MP outside the King's hall

    Battle king = board();
    king.kingBattle = true;
    king.units[0].maxHp = 300;  // room for the feast: healing still caps at max HP
    king.units[0].hp = 10;
    king.units[0].mp = 0;
    king.useItem(1, 0, snacks);
    CHECK(king.units[0].hp == 110);
    CHECK(king.units[0].mp == 10);
}

TEST_CASE("rules v4: Royal Snacks lift ATK-/DEF- but leave real afflictions alone",
          "[battle][items]") {
    content::ItemDef snacks = mkItem("royal_snacks", content::ConsumableEffect::Heal, 10);
    snacks.curesDebuffs = true;

    Battle b = board();
    afflict(b.units[0], content::StatusType::AttackDown, 4, 30);
    afflict(b.units[0], content::StatusType::DefenseDown, 4, 30);
    afflict(b.units[0], content::StatusType::Poison, 4, 5);
    b.useItem(1, 0, snacks);
    CHECK_FALSE(hasStatus(b.units[0], content::StatusType::AttackDown));
    CHECK_FALSE(hasStatus(b.units[0], content::StatusType::DefenseDown));
    CHECK(hasStatus(b.units[0], content::StatusType::Poison));  // that is a Remedy's job
}

TEST_CASE("rules v4: the King flag is derived from the team, not rolled",
          "[battle][king][determinism]") {
    const content::ContentDatabase db = shippedContent();
    const Party party = smallParty(db);

    const dungeon::EnemyTeam king = kingTeam(db);
    CHECK(buildBattle(party, king, db).kingBattle);

    dungeon::EnemyTeam other = bossRushTeam(db, 0);
    REQUIRE_FALSE(other.bossId.empty());
    REQUIRE(other.bossId != std::string(kKingBossId));
    CHECK_FALSE(buildBattle(party, other, db).kingBattle);
}

// --- The shipped balance data (feature 5, 6, 7, 8) ---------------------------

TEST_CASE("rules v4: the shipped economy carries the M43 reprices", "[battle][balance][data]") {
    const content::ContentDatabase db = shippedContent();
    REQUIRE(db.findItem("antidote") != nullptr);
    CHECK(db.findItem("antidote")->value == 100);
    CHECK(db.findItem("ether")->value == 150);
    CHECK(db.findItem("hi_ether")->value == 500);
    CHECK(db.findItem("phoenix_tear")->value == 300);
    CHECK(db.findItem("phoenix_tear")->effectAmount == 25);

    const content::ItemDef* snacks = db.findItem("royal_snacks");
    REQUIRE(snacks != nullptr);
    CHECK(snacks->value == 250);
    CHECK(snacks->minTown == 1);
    CHECK(snacks->maxTown == 1);
    CHECK(snacks->curesDebuffs);
    CHECK(snacks->kingEffectAmount == 100);
    CHECK(snacks->kingMpAmount == 10);
    CHECK(snacks->description == "Bring Snacks!");
}

TEST_CASE("rules v4: Purify cures without healing and Renew can raise the fallen",
          "[battle][balance][data]") {
    const content::ContentDatabase db = shippedContent();
    const content::SkillDef* purify = db.findSkill("purify");
    REQUIRE(purify != nullptr);
    CHECK(purify->power == 0);
    CHECK(purify->controlEffect == content::SkillEffect::Cleanse);

    const content::SkillDef* renew = db.findSkill("renew");
    REQUIRE(renew != nullptr);
    CHECK(renew->reviveHpPct == 20);
    CHECK(renew->power == 12);  // weaker than Mend's 20: an emergency, not a heal

    const content::PassiveDef* clarity = db.findPassive("clarity");
    REQUIRE(clarity != nullptr);
    CHECK(clarity->magnitude == 2);
}
