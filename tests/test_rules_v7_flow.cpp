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
#include "game/Party.hpp"
#include "states/QuitPrompt.hpp"
#include "ui/Menu.hpp"

// M47 — rules & flow pass (battle rules v7).
//
// 1. The `cleanse` skill control lifts AFFLICTIONS only (Poison/Blind/Silence/
//    Confusion); ATK-/DEF- survive it, while cure ITEMS still lift everything.
//    Proven through Battle::useSkill, the one path BattleState and the Simulator
//    both take, plus an explicit sim-vs-live agreement check.
// 2. Losing (or fleeing) a castle challenge clamps the party instead of healing
//    it: survivors at 1 HP, the fallen stay fallen, MP untouched, and a total
//    wipe leaves exactly one member on their feet.
// 3. The pause menus' quit prompt is a three-answer model whose safe row is
//    "Keep Playing".

using namespace cd;
using namespace cd::battle;

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
    c.stats = {120, 20, 20, 12, 10};
    c.maxHp = c.hp = 120;
    c.mp = c.maxMp = 60;
    return c;
}

Combatant ogre() {
    Combatant c;
    c.side = Side::Enemy;
    c.name = "Ogre";
    c.sourceId = "ogre";
    c.stats = {400, 18, 6, 10, 7};
    c.maxHp = c.hp = 400;
    c.mp = c.maxMp = 20;
    return c;
}

// Unit 0 = the caster, unit 1 = an ally carrying one of every relevant status,
// unit 2 = a punching bag so the battle is never already over.
Battle loadedBoard() {
    Battle b;
    b.units.push_back(hero("Cleric"));
    b.units.push_back(hero("Ally"));
    b.units.push_back(ogre());
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 0x5EEDC0FFEEull;

    Combatant& ally = b.units[1];
    ally.statuses.push_back({content::StatusType::Poison, 6, 4});
    ally.statuses.push_back({content::StatusType::Blind, 0, 4});
    ally.statuses.push_back({content::StatusType::Silence, 0, 4});
    ally.statuses.push_back({content::StatusType::Confusion, 0, 4});
    ally.statuses.push_back({content::StatusType::AttackDown, 20, 4});
    ally.statuses.push_back({content::StatusType::DefenseDown, 20, 4});
    ally.statuses.push_back({content::StatusType::AttackUp, 15, 4});
    return b;
}

bool has(const Combatant& c, content::StatusType t) {
    return std::any_of(c.statuses.begin(), c.statuses.end(),
                       [t](const StatusInstance& s) { return s.type == t; });
}

std::vector<content::StatusType> statusTypes(const Combatant& c) {
    std::vector<content::StatusType> out;
    for (const StatusInstance& s : c.statuses) {
        out.push_back(s.type);
    }
    return out;
}

Character member(const char* name, int maxHp, int hp, int maxMp, int mp) {
    Character c;
    c.name = name;
    c.classId = "knight";
    c.maxHp = maxHp;
    c.hp = hp;
    c.maxMp = maxMp;
    c.mp = mp;
    return c;
}

}  // namespace

TEST_CASE("rules v7: the cleanse narrowing is in force", "[battle][rules]") {
    // The v7 rules are still in force at or above their own version. The EXACT
    // pin lives with the newest rules milestone's tests (M48 = 8), so a later
    // bump does not have to edit every past milestone's suite.
    CHECK(kBattleRulesVersion >= 7);
}

TEST_CASE("rules v7: Purify lifts afflictions and leaves the stat debuffs",
          "[battle][rules][status]") {
    const content::ContentDatabase db = loadContent();
    const content::SkillDef* purify = db.findSkill("purify");
    REQUIRE(purify != nullptr);
    REQUIRE(purify->controlEffect == content::SkillEffect::Cleanse);
    REQUIRE(purify->power == 0);  // M43: it already healed nothing

    Battle b = loadedBoard();
    b.useSkill(0, 1, *purify);
    const Combatant& ally = b.units[1];

    CHECK_FALSE(has(ally, content::StatusType::Poison));
    CHECK_FALSE(has(ally, content::StatusType::Blind));
    CHECK_FALSE(has(ally, content::StatusType::Silence));
    CHECK_FALSE(has(ally, content::StatusType::Confusion));
    // The point of the milestone: a cleanse is not a debuff eraser any more.
    CHECK(has(ally, content::StatusType::AttackDown));
    CHECK(has(ally, content::StatusType::DefenseDown));
    // Buffs were never touched by a cleanse and still are not.
    CHECK(has(ally, content::StatusType::AttackUp));
    // Purify heals nothing, and a cleanse costs no HP either way.
    CHECK(ally.hp == ally.maxHp);
}

TEST_CASE("rules v7: Purify reaches the whole party (all_allies)", "[battle][rules][status]") {
    const content::ContentDatabase db = loadContent();
    const content::SkillDef* purify = db.findSkill("purify");
    REQUIRE(purify != nullptr);

    Battle b = loadedBoard();
    b.units[0].statuses.push_back({content::StatusType::Poison, 6, 4});
    b.units[0].statuses.push_back({content::StatusType::DefenseDown, 20, 4});
    b.useSkill(0, 1, *purify);  // aimed at the ally; all_allies covers the caster too

    CHECK_FALSE(has(b.units[0], content::StatusType::Poison));
    CHECK(has(b.units[0], content::StatusType::DefenseDown));
}

TEST_CASE("rules v7: the cleanse resolves identically in the sim and in live play",
          "[battle][rules][determinism]") {
    const content::ContentDatabase db = loadContent();
    const content::SkillDef* purify = db.findSkill("purify");
    REQUIRE(purify != nullptr);

    // Live play: BattleState calls Battle::useSkill directly.
    Battle live = loadedBoard();
    live.useSkill(0, 1, *purify);

    // The Simulator: choices go through applyChoice, which calls the same
    // Battle::useSkill. One rule, one code path — this test exists to fail loudly
    // if a future change ever forks them.
    Battle sim = loadedBoard();
    EnemyChoice choice;
    choice.useSkill = true;
    choice.skillId = "purify";
    choice.target = 1;
    applyChoice(sim, 0, choice, db);

    CHECK(statusTypes(sim.units[1]) == statusTypes(live.units[1]));
    CHECK(sim.units[1].hp == live.units[1].hp);
    CHECK(sim.units[0].mp == live.units[0].mp);
    CHECK(sim.rollCursor == live.rollCursor);
}

TEST_CASE("rules v7: cure items still lift everything", "[battle][rules][status]") {
    const content::ContentDatabase db = loadContent();
    const content::ItemDef* remedy = db.findItem("antidote");  // shipped name: "Remedy"
    REQUIRE(remedy != nullptr);
    REQUIRE(remedy->effect == content::ConsumableEffect::Cure);

    Battle b = loadedBoard();
    b.useItem(0, 1, *remedy);
    const Combatant& ally = b.units[1];

    // The narrowed cleanse must not have leaked into the item path.
    CHECK_FALSE(has(ally, content::StatusType::Poison));
    CHECK_FALSE(has(ally, content::StatusType::Blind));
    CHECK_FALSE(has(ally, content::StatusType::Silence));
    CHECK_FALSE(has(ally, content::StatusType::Confusion));
    CHECK_FALSE(has(ally, content::StatusType::AttackDown));
    CHECK_FALSE(has(ally, content::StatusType::DefenseDown));
    CHECK(has(ally, content::StatusType::AttackUp));
}

TEST_CASE("castle defeat: survivors drop to 1 HP and the fallen stay fallen", "[game][castle]") {
    Party p;
    p.members.push_back(member("Standing", 100, 87, 30, 21));
    p.members.push_back(member("Fallen", 100, 0, 30, 12));
    p.members.push_back(member("Grazed", 100, 1, 30, 0));
    p.members.push_back(member("Bleeding", 100, 4, 30, 30));

    clampCastleDefeat(p);

    CHECK(p.members[0].hp == 1);
    CHECK(p.members[1].hp == 0);  // KO'd members are NOT raised
    CHECK(p.members[2].hp == 1);
    CHECK(p.members[3].hp == 1);
    // MP is untouched — the party keeps whatever it had when the fight ended.
    CHECK(p.members[0].mp == 21);
    CHECK(p.members[1].mp == 12);
    CHECK(p.members[2].mp == 0);
    CHECK(p.members[3].mp == 30);
    // No gold is taken (the castle never charged any).
    CHECK(p.gold == 0);
}

TEST_CASE("castle defeat: a total wipe leaves exactly one member standing", "[game][castle]") {
    Party p;
    p.members.push_back(member("First", 100, 0, 30, 5));
    p.members.push_back(member("Second", 100, 0, 30, 9));
    p.members.push_back(member("Third", 100, 0, 30, 0));

    clampCastleDefeat(p);

    CHECK(p.members[0].hp == 1);  // first in party order, so an Inn is reachable
    CHECK(p.members[1].hp == 0);
    CHECK(p.members[2].hp == 0);
    CHECK(p.members[0].mp == 5);
}

TEST_CASE("castle defeat: an empty party is a no-op", "[game][castle]") {
    Party p;
    clampCastleDefeat(p);  // must not touch memory it does not own
    CHECK(p.members.empty());
}

TEST_CASE("castle defeat: a lone survivor and a lone casualty", "[game][castle]") {
    Party alive;
    alive.members.push_back(member("Only", 100, 100, 30, 30));
    clampCastleDefeat(alive);
    CHECK(alive.members[0].hp == 1);

    Party dead;
    dead.members.push_back(member("Only", 100, 0, 30, 30));
    clampCastleDefeat(dead);
    CHECK(dead.members[0].hp == 1);  // the full-wipe guarantee
}

TEST_CASE("quit prompt: three answers with Keep Playing as the safe row", "[ui][states]") {
    const std::vector<std::string> labels = quit::answerLabelList();
    REQUIRE(labels.size() == static_cast<std::size_t>(quit::kAnswerCount));
    CHECK(labels[quit::kQuitToTitle] == "Quit to Title");
    CHECK(labels[quit::kQuitGame] == "Quit Game");
    CHECK(labels[quit::kKeepPlaying] == "Keep Playing");
    // The cursor and Cancel must never resolve to a destructive answer.
    CHECK(quit::kSafeAnswer == quit::kKeepPlaying);

    // The two bodies must say what THIS screen actually loses.
    CHECK(std::string(quit::kTownBody).find("save") != std::string::npos);
    CHECK(std::string(quit::kDungeonBody).find("autosave") != std::string::npos);
}

TEST_CASE("quit prompt: the three-answer menu navigates and wraps", "[ui][states]") {
    std::vector<ui::MenuItem> items;
    for (const std::string& label : quit::answerLabelList()) {
        items.push_back({label, true});
    }
    ui::Menu menu(std::move(items));
    menu.setCursor(quit::kSafeAnswer);
    REQUIRE(menu.cursor() == quit::kKeepPlaying);

    menu.moveUp();
    CHECK(menu.cursor() == quit::kQuitGame);
    menu.moveUp();
    CHECK(menu.cursor() == quit::kQuitToTitle);
    menu.moveUp();  // wraps back to the bottom
    CHECK(menu.cursor() == quit::kKeepPlaying);
    menu.moveDown();
    CHECK(menu.cursor() == quit::kQuitToTitle);
}
