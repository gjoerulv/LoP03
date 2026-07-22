#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"
#include "game/Profile.hpp"
#include "score/Scoring.hpp"

// M45 — the King's reward classes. Everything the three classes do is data plus a
// generic flag: the cross-save unlock, the equip bans, the AoE basic attack with
// attack-applied statuses, the uncontrolled (Jester) turn shared by every decider,
// the Goose's enemy-buffing kindness, and the additive class score modifier.

using namespace cd;
using namespace cd::battle;
namespace fs = std::filesystem;

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    REQUIRE(content::loadAll(fs::path(CRYSTAL_TEST_DATA_DIR), db, rep));
    return db;
}

Combatant partyMember(const std::string& name, int hp, int atk, int spd) {
    Combatant c;
    c.side = Side::Party;
    c.name = name;
    c.sourceId = "hero";
    c.stats = {hp, atk, 12, 10, spd};
    c.maxHp = hp;
    c.hp = hp;
    c.mp = c.maxMp = 40;
    return c;
}

Combatant enemyUnit(const std::string& name, int hp) {
    Combatant c;
    c.side = Side::Enemy;
    c.name = name;
    c.sourceId = name;
    c.stats = {hp, 14, 8, 6, 9};
    c.maxHp = hp;
    c.hp = hp;
    c.mp = c.maxMp = 20;
    return c;
}

// One hero and three foes: enough to tell a sweep from a single strike.
Battle board() {
    Battle b;
    b.units.push_back(partyMember("Hero", 300, 40, 20));  // 0
    b.units.push_back(enemyUnit("Foe A", 200));           // 1
    b.units.push_back(enemyUnit("Foe B", 200));           // 2
    b.units.push_back(enemyUnit("Foe C", 200));           // 3
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 0xC1A55E5ull;
    b.turnsTaken = 1;
    return b;
}

Party partyOf(const content::ContentDatabase& db, const std::vector<const char*>& classIds) {
    Party p;
    for (const char* id : classIds) {
        if (const content::ClassDef* c = db.findClass(id)) {
            p.members.push_back(createCharacter(*c, id, 30));
        }
    }
    return p;
}

fs::path scratchFile(const char* name) {
    const fs::path p = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove(p, ec);
    return p;
}

}  // namespace

// --- The cross-save unlock ---------------------------------------------------

TEST_CASE("classes: the profile starts locked and round-trips once won", "[classes][profile]") {
    const fs::path file = scratchFile("cd_profile_roundtrip.json");
    {
        ProfileStore store(file);
        content::LoadReport rep;
        REQUIRE(store.load(rep));       // a missing file is a fresh profile, not an error
        CHECK_FALSE(store.classesUnlocked());
        CHECK(store.recordKingDefeated());        // first kill changes it
        CHECK_FALSE(store.recordKingDefeated());  // a second kill is not news
    }
    {
        ProfileStore reloaded(file);
        content::LoadReport rep;
        REQUIRE(reloaded.load(rep));
        CHECK(reloaded.classesUnlocked());
    }
    std::error_code ec;
    fs::remove(file, ec);
}

TEST_CASE("classes: a malformed profile locks rather than crashes", "[classes][profile]") {
    ProfileData data;
    data.kingDefeated = true;
    content::LoadReport rep;
    CHECK_FALSE(parseProfileText("{ this is not json", data, rep));
    CHECK_FALSE(data.kingDefeated);  // reset to the safe state
    CHECK_FALSE(rep.ok());

    ProfileData other;
    other.kingDefeated = true;
    content::LoadReport rep2;
    CHECK_FALSE(parseProfileText(R"({"version":99,"kingDefeated":true})", other, rep2));
    CHECK_FALSE(other.kingDefeated);  // a foreign version is not trusted
}

TEST_CASE("classes: serialize/parse is a faithful round trip", "[classes][profile]") {
    ProfileData data;
    data.kingDefeated = true;
    ProfileData back;
    content::LoadReport rep;
    REQUIRE(parseProfileText(serializeProfile(data), back, rep));
    CHECK(back.kingDefeated);
    CHECK(rep.ok());
}

// --- The shipped class data --------------------------------------------------

TEST_CASE("classes: the three reward classes ship locked, with their quirks",
          "[classes][data]") {
    const content::ContentDatabase db = loadContent();
    for (const char* id : {"dragon", "jester", "goose"}) {
        const content::ClassDef* cls = db.findClass(id);
        INFO(id);
        REQUIRE(cls != nullptr);
        CHECK(cls->unlockedByKing);
        CHECK(cls->scoreModPct != 0);
    }
    // None of the six originals is affected by any of it.
    for (const char* id : {"knight", "ranger", "mage", "cleric", "rogue", "guardian"}) {
        const content::ClassDef* cls = db.findClass(id);
        INFO(id);
        REQUIRE(cls != nullptr);
        CHECK_FALSE(cls->unlockedByKing);
        CHECK_FALSE(cls->attackHitsAll);
        CHECK_FALSE(cls->uncontrolled);
        CHECK(cls->attackStatuses.empty());
        CHECK(cls->equipBans.empty());
        CHECK(cls->scoreModPct == 0);
    }

    const content::ClassDef* dragon = db.findClass("dragon");
    CHECK(dragon->attackHitsAll);
    CHECK(dragon->attackStatuses.size() == 2);
    CHECK_FALSE(dragon->canEquip(content::EquipSlot::Armor));
    CHECK(dragon->canEquip(content::EquipSlot::Weapon));
    CHECK(dragon->scoreModPct == -20);

    const content::ClassDef* jester = db.findClass("jester");
    CHECK(jester->uncontrolled);
    CHECK_FALSE(jester->canEquip(content::EquipSlot::Weapon));
    CHECK(jester->scoreModPct == 5);

    const content::ClassDef* goose = db.findClass("goose");
    CHECK_FALSE(goose->canEquip(content::EquipSlot::Weapon));
    CHECK_FALSE(goose->canEquip(content::EquipSlot::Armor));
    CHECK_FALSE(goose->canEquip(content::EquipSlot::Accessory));
    CHECK(goose->scoreModPct == 5);
}

TEST_CASE("classes: equip bans are enforced through one shared rule", "[classes][equip]") {
    const content::ContentDatabase db = loadContent();
    const Party p = partyOf(db, {"dragon", "jester", "goose", "knight"});
    REQUIRE(p.members.size() == 4);
    CHECK_FALSE(canEquipSlot(p.members[0], content::EquipSlot::Armor, db));   // Dragon
    CHECK(canEquipSlot(p.members[0], content::EquipSlot::Weapon, db));
    CHECK_FALSE(canEquipSlot(p.members[1], content::EquipSlot::Weapon, db));  // Jester
    CHECK(canEquipSlot(p.members[1], content::EquipSlot::Armor, db));
    for (content::EquipSlot slot : {content::EquipSlot::Weapon, content::EquipSlot::Armor,
                                    content::EquipSlot::Accessory}) {
        CHECK_FALSE(canEquipSlot(p.members[2], slot, db));  // Goose: a goose
        CHECK(canEquipSlot(p.members[3], slot, db));        // Knight: unaffected
    }
}

// --- The Dragon's sweep ------------------------------------------------------

TEST_CASE("classes: an AoE basic attack strikes every living foe", "[classes][battle]") {
    Battle b = board();
    b.units[0].attackHitsAll = true;
    b.attack(0, 1);
    for (int i = 1; i <= 3; ++i) {
        INFO("foe " << i);
        CHECK(b.units[static_cast<std::size_t>(i)].hp < b.units[static_cast<std::size_t>(i)].maxHp);
    }
}

TEST_CASE("classes: the sweep applies its statuses per connecting hit", "[classes][battle]") {
    Battle b = board();
    b.units[0].attackHitsAll = true;
    b.units[0].attackStatuses = {{content::StatusType::Poison, 6, 2},
                                 {content::StatusType::Blind, 0, 2}};
    b.attack(0, 1);
    for (int i = 1; i <= 3; ++i) {
        INFO("foe " << i);
        CHECK(hasStatus(b.units[static_cast<std::size_t>(i)], content::StatusType::Poison));
        CHECK(hasStatus(b.units[static_cast<std::size_t>(i)], content::StatusType::Blind));
    }
}

TEST_CASE("classes: a missed sweep hit lands no status on that target",
          "[classes][battle]") {
    Battle b = board();
    b.units[0].attackHitsAll = true;
    b.units[0].attackStatuses = {{content::StatusType::Poison, 6, 2}};
    // Blind the attacker: the to-hit roll is taken per target, so some connect and
    // some do not — and only the ones that connect are poisoned.
    b.units[0].statuses.push_back({content::StatusType::Blind, 0, 4});
    b.attack(0, 1);
    for (int i = 1; i <= 3; ++i) {
        const Combatant& foe = b.units[static_cast<std::size_t>(i)];
        const bool missed =
            std::find(b.lastMissed.begin(), b.lastMissed.end(), i) != b.lastMissed.end();
        INFO("foe " << i << (missed ? " (missed)" : " (hit)"));
        CHECK(hasStatus(foe, content::StatusType::Poison) == !missed);
    }
}

TEST_CASE("classes: a confused sweeper still lashes at one of its own",
          "[classes][battle]") {
    Battle b = board();
    b.units.push_back(partyMember("Ally", 300, 10, 8));  // 4: something to hit
    b.threat.assign(b.units.size(), 0);
    b.units[0].attackHitsAll = true;
    b.units[0].statuses.push_back({content::StatusType::Confusion, 0, 4});
    b.attack(0, 1);
    for (int i = 1; i <= 3; ++i) {
        CHECK(b.units[static_cast<std::size_t>(i)].hp ==
              b.units[static_cast<std::size_t>(i)].maxHp);  // no foe was swept
    }
    CHECK((b.units[0].hp < b.units[0].maxHp || b.units[4].hp < b.units[4].maxHp));
}

// --- The Jester's uncontrolled turn -----------------------------------------

TEST_CASE("classes: an uncontrolled turn is identical in sim and live",
          "[classes][battle][jester]") {
    const content::ContentDatabase db = loadContent();
    Battle b = board();
    b.units[0].uncontrolled = true;
    b.units[0].skillIds = {"fireball", "mend", "weaken"};

    // Every decider derives the same turn from the same pure hash.
    const EnemyChoice live = uncontrolledChoice(b, 0, db);
    const EnemyChoice sim = choosePartyAction(b, 0, db);
    CHECK(sim.useSkill == live.useSkill);
    CHECK(sim.skillId == live.skillId);
    CHECK(sim.target == live.target);
}

TEST_CASE("classes: the uncontrolled pick is seeded, not sticky",
          "[classes][battle][jester]") {
    const content::ContentDatabase db = loadContent();
    Battle b = board();
    b.units[0].uncontrolled = true;
    b.units[0].skillIds = {"fireball", "mend", "weaken", "spark"};

    // Same board and round -> same turn, every time (reproducible).
    for (int i = 0; i < 5; ++i) {
        CHECK(uncontrolledChoice(b, 0, db).skillId == uncontrolledChoice(b, 0, db).skillId);
    }
    // Across rounds it actually varies (it is chaotic, not merely random-looking).
    std::vector<std::string> seen;
    for (int round = 1; round <= 12; ++round) {
        b.turnsTaken = round;
        const EnemyChoice c = uncontrolledChoice(b, 0, db);
        seen.push_back(c.useSkill ? c.skillId : "<attack>");
    }
    std::sort(seen.begin(), seen.end());
    seen.erase(std::unique(seen.begin(), seen.end()), seen.end());
    CHECK(seen.size() >= 3);
}

TEST_CASE("classes: an uncontrolled unit never picks what it cannot cast",
          "[classes][battle][jester]") {
    const content::ContentDatabase db = loadContent();
    Battle b = board();
    b.units[0].uncontrolled = true;
    b.units[0].skillIds = {"fireball", "inferno", "mend"};
    b.units[0].mp = 0;  // nothing is affordable: it must swing
    for (int round = 1; round <= 20; ++round) {
        b.turnsTaken = round;
        CHECK_FALSE(uncontrolledChoice(b, 0, db).useSkill);
    }
    // Silenced, MP-cost skills are equally out of reach.
    b.units[0].mp = b.units[0].maxMp;
    b.units[0].statuses.push_back({content::StatusType::Silence, 0, 40});
    for (int round = 1; round <= 20; ++round) {
        b.turnsTaken = round;
        CHECK_FALSE(uncontrolledChoice(b, 0, db).useSkill);
    }
}

TEST_CASE("classes: an uncontrolled turn always has a living target",
          "[classes][battle][jester]") {
    const content::ContentDatabase db = loadContent();
    Battle b = board();
    b.units[0].uncontrolled = true;
    b.units[0].skillIds = {"fireball"};
    b.units[2].hp = 0;
    b.units[3].hp = 0;
    for (int round = 1; round <= 20; ++round) {
        b.turnsTaken = round;
        const EnemyChoice c = uncontrolledChoice(b, 0, db);
        REQUIRE(c.target >= 0);
        CHECK(b.units[static_cast<std::size_t>(c.target)].alive());
    }
}

TEST_CASE("classes: a jest is presentation only and never moves the roll stream",
          "[classes][battle][jester]") {
    Battle b = board();
    const std::uint64_t cursorBefore = b.rollCursor;
    int jests = 0;
    for (int round = 1; round <= 400; ++round) {
        b.turnsTaken = round;
        int line = -1;
        if (jestThisTurn(b, 0, 12, line)) {
            ++jests;
            CHECK(line >= 0);
            CHECK(line < 12);
        }
    }
    CHECK(b.rollCursor == cursorBefore);  // never advanced: it is decoration
    INFO("jests in 400 rounds: " << jests);
    CHECK(jests > 20);   // roughly 15%...
    CHECK(jests < 100);  // ...not always, not never
    // And it is reproducible for a given round.
    b.turnsTaken = 7;
    int a = -1;
    int c = -1;
    CHECK(jestThisTurn(b, 0, 12, a) == jestThisTurn(b, 0, 12, c));
    CHECK(a == c);
}

// --- The Goose's kindness ----------------------------------------------------

TEST_CASE("classes: a goose heal cheers the enemies up too", "[classes][battle][goose]") {
    const content::ContentDatabase db = loadContent();
    const content::SkillDef* honk = db.findSkill("honking_comfort");
    REQUIRE(honk != nullptr);
    REQUIRE(honk->alsoBuffsEnemies);

    Battle b = board();
    b.units[0].hp = 100;
    b.useSkill(0, 0, *honk);
    CHECK(b.units[0].hp > 100);  // the ally really is healed
    for (int i = 1; i <= 3; ++i) {
        INFO("foe " << i);
        CHECK(hasStatus(b.units[static_cast<std::size_t>(i)], honk->statusEffect));
    }
}

TEST_CASE("classes: an ordinary heal buffs nobody's enemies", "[classes][battle][goose]") {
    const content::ContentDatabase db = loadContent();
    const content::SkillDef* mend = db.findSkill("mend");
    REQUIRE(mend != nullptr);
    REQUIRE_FALSE(mend->alsoBuffsEnemies);
    Battle b = board();
    b.units[0].hp = 100;
    b.useSkill(0, 0, *mend);
    for (int i = 1; i <= 3; ++i) {
        CHECK(b.units[static_cast<std::size_t>(i)].statuses.empty());
    }
}

TEST_CASE("classes: the goose ultimate is a level-30, 30 MP, all-enemy curse",
          "[classes][data][goose]") {
    const content::ContentDatabase db = loadContent();
    const content::SkillDef* ult = db.findSkill("everyone_is_welcome");
    REQUIRE(ult != nullptr);
    CHECK(ult->mpCost == 30);
    CHECK(ult->target == content::SkillTarget::AllEnemies);
    const content::ClassDef* goose = db.findClass("goose");
    REQUIRE(goose != nullptr);
    bool atThirty = false;
    for (const content::LearnEntry& e : goose->learnset) {
        if (e.skill == "everyone_is_welcome") {
            atThirty = e.level == 30;
        }
    }
    CHECK(atThirty);
}

// --- Scoring -----------------------------------------------------------------

TEST_CASE("classes: the score modifier is additive across the party", "[classes][scoring]") {
    const content::ContentDatabase db = loadContent();
    CHECK(partyClassModPct(partyOf(db, {"knight", "mage", "cleric", "rogue"}), db) == 0);
    CHECK(partyClassModPct(partyOf(db, {"dragon", "knight", "knight", "knight"}), db) == -20);
    CHECK(partyClassModPct(partyOf(db, {"dragon", "dragon", "dragon", "dragon"}), db) == -80);
    CHECK(partyClassModPct(partyOf(db, {"jester", "goose", "knight", "knight"}), db) == 10);
    CHECK(partyClassModPct(partyOf(db, {"dragon", "jester", "goose", "knight"}), db) == -10);
}

TEST_CASE("classes: the modifier moves the score and shows its own row",
          "[classes][scoring]") {
    score::RunSummary run;
    run.completed = true;
    run.battleTurns = 20;
    run.chestsOpened = 2;
    run.dangerDefeated = 5;

    const int plain = score::computeScore(run);
    run.classModPct = -80;  // an all-Dragon party
    const score::ScoreBreakdown dragons = score::scoreBreakdown(run);
    CHECK(dragons.classMod < 0);
    CHECK(dragons.total < plain);

    run.classModPct = 10;  // a Jester + a Goose
    const score::ScoreBreakdown fools = score::scoreBreakdown(run);
    CHECK(fools.classMod > 0);
    CHECK(fools.total > plain);

    run.classModPct = 0;
    CHECK(score::scoreBreakdown(run).classMod == 0);
    CHECK(score::computeScore(run) == plain);  // unchanged for the original classes
}

TEST_CASE("classes: an all-Dragon run still cannot score below zero", "[classes][scoring]") {
    score::RunSummary run;
    run.completed = true;
    run.battleTurns = 300;  // a disastrous run
    run.escapes = 10;
    run.classModPct = -80;
    CHECK(score::computeScore(run) >= 0);
}

// --- Battle integration ------------------------------------------------------

TEST_CASE("classes: the reward classes fight through buildBattle without special cases",
          "[classes][battle]") {
    const content::ContentDatabase db = loadContent();
    const Party p = partyOf(db, {"dragon", "jester", "goose", "knight"});
    dungeon::EnemyTeam team;
    team.enemyIds = {"goblin_grunt", "goblin_grunt"};
    team.statScalePct = 100;
    Battle b = buildBattle(p, team, db);
    REQUIRE(b.units.size() >= 4);
    CHECK(b.units[0].attackHitsAll);            // Dragon
    CHECK(b.units[0].attackStatuses.size() == 2);
    CHECK_FALSE(b.units[0].uncontrolled);
    CHECK(b.units[1].uncontrolled);             // Jester
    CHECK_FALSE(b.units[1].attackHitsAll);
    CHECK_FALSE(b.units[2].uncontrolled);       // Goose: ordinary turns, awful skills
    CHECK_FALSE(b.units[3].attackHitsAll);      // Knight: untouched
    CHECK(b.units[3].attackStatuses.empty());
}

// On-demand battery for the milestone note and owner review:
//   crystal_tests.exe "[classes-report]" -s
TEST_CASE("classes report: how the reward parties actually fare", "[.][classes-report]") {
    const content::ContentDatabase db = loadContent();
    struct Row {
        const char* label;
        std::vector<const char*> classes;
    };
    const Row rows[] = {
        {"baseline (knight/ranger/mage/cleric)", {"knight", "ranger", "mage", "cleric"}},
        {"all Dragon", {"dragon", "dragon", "dragon", "dragon"}},
        {"all Jester", {"jester", "jester", "jester", "jester"}},
        {"all Goose", {"goose", "goose", "goose", "goose"}},
        {"one of each + knight", {"dragon", "jester", "goose", "knight"}},
    };
    std::cout << "party | classMod% | depth 1 | depth 5 | depth 10 (outcome 1=win 2=loss / rounds)\n";
    for (const Row& row : rows) {
        const Party p = partyOf(db, row.classes);
        std::cout << row.label << " | " << partyClassModPct(p, db) << "% |";
        for (int depth : {1, 5, 10}) {
            const dungeon::Dungeon d = dungeon::generate(4242, depth, db, "ruined_keep");
            int bossTeam = -1;
            for (const dungeon::Room& r : d.rooms) {
                if (r.type == dungeon::RoomType::Boss) {
                    bossTeam = r.teamIndex;
                }
            }
            if (bossTeam < 0) {
                std::cout << " - |";
                continue;
            }
            Battle b = buildBattle(p, d.teams[static_cast<std::size_t>(bossTeam)], db);
            const SimResult r = simulate(b, db, 300);
            std::cout << " " << static_cast<int>(r.outcome) << "/" << r.rounds << " |";
        }
        std::cout << "\n";
    }
    SUCCEED();
}
