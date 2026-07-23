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
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"
#include "game/WorldLadder.hpp"  // kTownCount

// M49 — the King's Court (battle rules v9). The King fights with two Royal
// Guards; a deterministic revive clock calls them back on the 5th of HIS turns
// with both down, repeatably; the Boss Rush fields every boss's authored
// minions; and a `bossOnly` enemy can never be generated into a dungeon or an
// endless wave.

using namespace cd;
using namespace cd::battle;

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

Combatant king(int reviveTurns) {
    Combatant c;
    c.side = Side::Enemy;
    c.name = "The Hollow King";
    c.sourceId = "the_hollow_king";
    c.isBoss = true;
    c.stats = {900, 30, 30, 20, 20};
    c.maxHp = c.hp = 900;
    c.mp = c.maxMp = 200;
    c.reviveMinionTurns = reviveTurns;
    return c;
}

Combatant guard(const std::string& name) {
    Combatant c;
    c.side = Side::Enemy;
    c.name = name;
    c.sourceId = name;
    c.stats = {100, 10, 10, 10, 5};
    c.maxHp = c.hp = 100;
    c.mp = c.maxMp = 30;
    return c;
}

Combatant hero() {
    Combatant c;
    c.side = Side::Party;
    c.name = "Aldo";
    c.sourceId = "knight";
    c.stats = {200, 20, 10, 10, 10};
    c.maxHp = c.hp = 200;
    c.mp = c.maxMp = 20;
    return c;
}

// Unit 0 hero, unit 1 King, units 2-3 his court.
Battle court(int reviveTurns = 5) {
    Battle b;
    b.units.push_back(hero());
    b.units.push_back(king(reviveTurns));
    b.units.push_back(guard("Sword"));
    b.units.push_back(guard("Staff"));
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 0xC0117A11ull;
    return b;
}

void fell(Battle& b, std::size_t unit) { b.units[unit].hp = 0; }

// Takes `n` King turns, returning the announcement of the turn that spoke.
std::string takeKingTurns(Battle& b, int n) {
    std::string spoke;
    for (int i = 0; i < n; ++i) {
        const std::string line = b.beginUnitTurn(1);
        if (!line.empty()) {
            spoke = line;
        }
    }
    return spoke;
}

}  // namespace

TEST_CASE("court: the battle-rules version is 9", "[court][battle]") {
    CHECK(kBattleRulesVersion == 9);
}

TEST_CASE("court: the clock fires on the King's 5th turn with both guards down",
          "[court][battle]") {
    Battle b = court();
    fell(b, 2);
    fell(b, 3);

    // Four turns of counting: nothing yet.
    CHECK(takeKingTurns(b, 4).empty());
    CHECK_FALSE(b.units[2].alive());
    CHECK_FALSE(b.units[3].alive());

    const std::string line = takeKingTurns(b, 1);
    CHECK_FALSE(line.empty());
    CHECK(b.units[2].hp == b.units[2].maxHp);  // raised whole
    CHECK(b.units[3].hp == b.units[3].maxHp);
}

TEST_CASE("court: the clock repeats, every time the condition recurs", "[court][battle]") {
    Battle b = court();
    fell(b, 2);
    fell(b, 3);
    REQUIRE_FALSE(takeKingTurns(b, 5).empty());  // first revival

    fell(b, 2);
    fell(b, 3);
    CHECK(takeKingTurns(b, 4).empty());          // counts again from zero
    CHECK_FALSE(takeKingTurns(b, 1).empty());    // and fires again
    CHECK(b.units[2].hp == b.units[2].maxHp);
    CHECK(b.units[3].hp == b.units[3].maxHp);
}

TEST_CASE("court: one guard standing resets the clock", "[court][battle]") {
    Battle b = court();
    fell(b, 2);
    fell(b, 3);
    takeKingTurns(b, 4);  // one turn from firing

    b.units[2].hp = 1;    // revived by any other means: the count restarts
    CHECK(takeKingTurns(b, 1).empty());
    fell(b, 2);
    CHECK(takeKingTurns(b, 4).empty());        // a full five needed again
    CHECK_FALSE(takeKingTurns(b, 1).empty());
}

TEST_CASE("court: the clock never fires while a guard lives", "[court][battle]") {
    Battle b = court();
    fell(b, 2);  // one down, one up
    CHECK(takeKingTurns(b, 20).empty());
    CHECK_FALSE(b.units[2].alive());  // and the fallen one stays fallen
}

TEST_CASE("court: a boss without the clock never revives anyone", "[court][battle]") {
    Battle b = court(0);  // reviveMinionTurns = 0, every boss but the King
    fell(b, 2);
    fell(b, 3);
    CHECK(takeKingTurns(b, 20).empty());
    CHECK_FALSE(b.units[2].alive());
}

TEST_CASE("court: a king with no court counts nothing", "[court][battle]") {
    Battle b;
    b.units.push_back(hero());
    b.units.push_back(king(5));
    b.threat.assign(b.units.size(), 0);
    // No minions at all: the clock must not tick on an empty set (and must not
    // reach for a unit that is not there).
    CHECK(takeKingTurns(b, 20).empty());
}

TEST_CASE("court: the revived guards come back unafflicted", "[court][battle]") {
    Battle b = court();
    b.units[2].statuses.push_back({content::StatusType::Poison, 6, 4});
    b.units[3].statuses.push_back({content::StatusType::DefenseDown, 20, 4});
    fell(b, 2);
    fell(b, 3);
    REQUIRE_FALSE(takeKingTurns(b, 5).empty());
    CHECK(b.units[2].statuses.empty());
    CHECK(b.units[3].statuses.empty());
}

TEST_CASE("court: the clock ticks identically in the sim and in live play",
          "[court][battle][determinism]") {
    const content::ContentDatabase db = loadContent();

    // The live driver calls beginUnitTurn directly; the Simulator calls it from
    // simulateInPlace. Drive the same board both ways and compare the moment the
    // court returns.
    Battle live = court();
    fell(live, 2);
    fell(live, 3);
    int liveFiredOn = 0;
    for (int turn = 1; turn <= 8; ++turn) {
        if (!live.beginUnitTurn(1).empty() && liveFiredOn == 0) {
            liveFiredOn = turn;
        }
    }

    Battle sim = court();
    fell(sim, 2);
    fell(sim, 3);
    int simFiredOn = 0;
    for (int turn = 1; turn <= 8; ++turn) {
        if (!sim.beginUnitTurn(1).empty() && simFiredOn == 0) {
            simFiredOn = turn;
        }
    }

    CHECK(liveFiredOn == 5);
    CHECK(simFiredOn == liveFiredOn);
    CHECK(live.units[2].hp == sim.units[2].hp);
    CHECK(live.rollCursor == sim.rollCursor);  // the clock draws no randomness
    (void)db;
}

// --- Teams -----------------------------------------------------------------

TEST_CASE("court: the King fields his two Royal Guards", "[court][castle]") {
    const content::ContentDatabase db = loadContent();
    const dungeon::EnemyTeam team = kingTeam(db);
    REQUIRE(team.bossId == std::string(kKingBossId));
    CHECK(team.enemyIds.size() == 2);
    CHECK(std::find(team.enemyIds.begin(), team.enemyIds.end(), "royal_guard_sword") !=
          team.enemyIds.end());
    CHECK(std::find(team.enemyIds.begin(), team.enemyIds.end(), "royal_guard_staff") !=
          team.enemyIds.end());

    // And the built battle really contains them, with their passives resolved.
    Party p;
    const content::ClassDef* knight = db.findClass("knight");
    REQUIRE(knight != nullptr);
    p.members.push_back(createCharacter(*knight, "Aldo", 50));
    const Battle b = buildBattle(p, team, db);

    int guards = 0;
    for (const Combatant& u : b.units) {
        if (u.sourceId == "royal_guard_sword") {
            ++guards;
            CHECK(u.ironWill);
            CHECK(u.evasionPct > 0);
        }
        if (u.sourceId == "royal_guard_staff") {
            ++guards;
            CHECK(u.spellWardPct > 0);
            CHECK(u.evasionPct > 0);
        }
        if (u.isBoss) {
            CHECK(u.reviveMinionTurns == 5);  // the clock came from the BossDef
        }
    }
    CHECK(guards == 2);
}

TEST_CASE("court: the Boss Rush fields each boss's own minions", "[court][castle]") {
    const content::ContentDatabase db = loadContent();
    const std::vector<std::string> order = bossRushOrder(db);
    REQUIRE_FALSE(order.empty());

    int withMinions = 0;
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
        const dungeon::EnemyTeam t = bossRushTeam(db, i);
        const content::BossDef* def = db.findBoss(t.bossId);
        REQUIRE(def != nullptr);
        INFO("rush boss " << t.bossId);
        CHECK(t.enemyIds == def->minions);  // exactly what it brings in a dungeon
        withMinions += def->minions.empty() ? 0 : 1;
    }
    CHECK(withMinions > 0);  // the feature is actually exercised by the roster
}

// --- The guards never leak --------------------------------------------------

TEST_CASE("court: a bossOnly enemy is never generated into a dungeon", "[court][dungeon]") {
    const content::ContentDatabase db = loadContent();
    std::vector<std::string> bossOnly;
    for (const auto& [id, def] : db.enemies()) {
        if (def.bossOnly) {
            bossOnly.push_back(id);
        }
    }
    REQUIRE(bossOnly.size() == 2);  // the two Royal Guards

    const char* themes[] = {"ruined_keep", "crystal_mine", "hollow_forest"};
    for (const char* theme : themes) {
        for (int town = 1; town <= kTownCount; ++town) {
            for (int seed = 0; seed < 12; ++seed) {
                const dungeon::Dungeon d = dungeon::generate(
                    static_cast<std::uint64_t>(seed * 7919 + town), 1 + seed % 12, db, theme, town);
                for (const dungeon::EnemyTeam& t : d.teams) {
                    for (const std::string& id : t.enemyIds) {
                        INFO(theme << " town " << town << " seed " << seed << " -> " << id);
                        CHECK(std::find(bossOnly.begin(), bossOnly.end(), id) == bossOnly.end());
                    }
                }
            }
        }
    }
}

TEST_CASE("court: a bossOnly enemy never appears in an endless wave", "[court][castle]") {
    const content::ContentDatabase db = loadContent();
    for (int wave = 0; wave < 80; ++wave) {
        const dungeon::EnemyTeam t = endlessWaveTeam(db, wave);
        for (const std::string& id : t.enemyIds) {
            const content::EnemyDef* def = db.findEnemy(id);
            REQUIRE(def != nullptr);
            INFO("wave " << wave << " -> " << id);
            CHECK_FALSE(def->bossOnly);
        }
    }
}

TEST_CASE("court: the guards are authored as the King's alone", "[court][content]") {
    const content::ContentDatabase db = loadContent();
    for (const char* id : {"royal_guard_sword", "royal_guard_staff"}) {
        const content::EnemyDef* def = db.findEnemy(id);
        REQUIRE(def != nullptr);
        INFO(id);
        CHECK(def->bossOnly);
        CHECK_FALSE(def->skills.empty());
        CHECK_FALSE(def->passives.empty());
        // The M48 no-dead-weapon rule: no guard may be immune to anything.
        CHECK(def->affinity.immunities.empty());

        // Referenced by exactly one boss — the King.
        int referencedBy = 0;
        for (const auto& [bossId, boss] : db.bosses()) {
            if (std::find(boss.minions.begin(), boss.minions.end(), id) != boss.minions.end()) {
                ++referencedBy;
                CHECK(bossId == std::string(kKingBossId));
            }
        }
        CHECK(referencedBy == 1);
    }
}

TEST_CASE("court: the guards buff the whole court, not just themselves", "[court][content]") {
    // The enemy AI aims an ally-facing support skill at the CASTER, so a
    // single_ally buff would never reach the King. Both guards must therefore
    // carry all_allies buffs for the mechanic to exist at all.
    const content::ContentDatabase db = loadContent();
    for (const char* id : {"royal_guard_sword", "royal_guard_staff"}) {
        const content::EnemyDef* def = db.findEnemy(id);
        REQUIRE(def != nullptr);
        bool anyPartyWideBuff = false;
        for (const std::string& sid : def->skills) {
            const content::SkillDef* s = db.findSkill(sid);
            if (s != nullptr && s->category == content::SkillCategory::Support &&
                s->target == content::SkillTarget::AllAllies &&
                s->statusEffect != content::StatusType::None) {
                anyPartyWideBuff = true;
            }
        }
        INFO(id);
        CHECK(anyPartyWideBuff);
    }
}
