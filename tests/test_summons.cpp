// M95 (rules v17) — summons: three once-per-run legends paid by the treasure
// digs, gated by ONE shared rule (menu, both AIs, useSkill, Simulator), whose
// terror rides the existing immunity chokepoint.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "game/Party.hpp"
#include "game/TreasureMap.hpp"

using namespace cd;

namespace {

const content::ContentDatabase& db() {
    static content::ContentDatabase database = [] {
        content::ContentDatabase d;
        content::LoadReport rep;
        REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), d, rep));
        return d;
    }();
    return database;
}

const char* kSummonSkills[] = {"summon_goose", "summon_sentinel", "summon_spring"};
const char* kSummonScrolls[] = {"summon_scroll_goose", "summon_scroll_sentinel",
                                "summon_scroll_spring"};

Party summonParty() {
    Party p;
    p.members.push_back(createCharacter(*db().findClass("mage"), "Mira", 40));
    p.members.push_back(createCharacter(*db().findClass("cleric"), "Ana", 40));
    p.members[0].extraSkills = {"summon_goose", "summon_sentinel"};
    p.members[1].extraSkills = {"summon_spring"};
    for (Character& c : p.members) {
        refreshCharacter(c, db());
        c.mp = c.maxMp;
    }
    return p;
}

}  // namespace

TEST_CASE("summons: the three legends are authored as briefed", "[summons]") {
    for (const char* id : kSummonSkills) {
        INFO(id);
        const content::SkillDef* s = db().findSkill(id);
        REQUIRE(s != nullptr);
        CHECK(s->oncePerRun);
        CHECK_FALSE(s->summonName.empty());
        CHECK(s->mpCost >= 70);  // very expensive by design
        CHECK(s->power > 30);    // and very strong
    }
    // The two offensive legends terrify; the Spring mends the whole party.
    CHECK(db().findSkill("summon_goose")->statusEffect == content::StatusType::Terrified);
    CHECK(db().findSkill("summon_goose")->target == content::SkillTarget::AllEnemies);
    CHECK(db().findSkill("summon_sentinel")->statusEffect == content::StatusType::Terrified);
    CHECK(db().findSkill("summon_sentinel")->element == content::Element::Holy);
    CHECK(db().findSkill("summon_spring")->category == content::SkillCategory::Heal);
    CHECK(db().findSkill("summon_spring")->target == content::SkillTarget::AllAllies);

    // Their scrolls teach them, are valueless (never sold/dropped), and the
    // treasure pool pays them AFTER the six Lost Scrolls, in order.
    for (const char* id : kSummonScrolls) {
        INFO(id);
        const content::ItemDef* it = db().findItem(id);
        REQUIRE(it != nullptr);
        CHECK(it->type == content::ItemType::Scroll);
        CHECK(it->value == 0);
    }
    const auto& pool = treasureScrollPool();
    REQUIRE(pool.size() == 9);
    CHECK(std::string(pool[6]) == "summon_scroll_goose");
    CHECK(std::string(pool[8]) == "summon_scroll_spring");
    // Award order: with the six Lost Scrolls taken, the Goose comes next.
    std::vector<std::string> awarded(pool.begin(), pool.begin() + 6);
    CHECK(nextTreasureScroll(awarded) == "summon_scroll_goose");
}

TEST_CASE("summons: once per run, shared everywhere, written back", "[summons]") {
    Party party = summonParty();
    dungeon::EnemyTeam team;
    team.name = "Test Foes";
    team.enemyIds = {"goblin", "goblin"};
    if (db().findEnemy("goblin") == nullptr) {
        for (const auto& [id, def] : db().enemies()) {  // any two plain foes
            if (!def.bossOnly) {
                team.enemyIds = {id, id};
                break;
            }
        }
    }

    battle::Battle b = battle::buildBattle(party, team, db());
    const content::SkillDef& goose = *db().findSkill("summon_goose");
    CHECK_FALSE(battle::summonSpent(b, goose));

    // First cast: real, recorded, and the foes cower (unless immune).
    const std::string first = b.useSkill(0, 2, goose);
    CHECK_FALSE(first.empty());
    CHECK(battle::summonSpent(b, goose));
    // Second cast: refused by the shared rule, nothing spent.
    const int mpBefore = b.units[0].mp;
    const std::string second = b.useSkill(0, 2, goose);
    CHECK(second.find("already answered") != std::string::npos);
    CHECK(b.units[0].mp == mpBefore);

    // The ledger persists into the NEXT battle of the same run...
    party.usedSummons = b.usedSummons;  // (the battle screen's writeback)
    battle::Battle b2 = battle::buildBattle(party, team, db());
    CHECK(battle::summonSpent(b2, goose));
    CHECK_FALSE(battle::summonSpent(b2, *db().findSkill("summon_spring")));
    // ...and a fresh run clears it.
    party.usedSummons.clear();
    battle::Battle b3 = battle::buildBattle(party, team, db());
    CHECK_FALSE(battle::summonSpent(b3, goose));
}

TEST_CASE("summons: terror lands on the plain and dies on the immune", "[summons]") {
    Party party = summonParty();
    // A plain dungeon boss cowers (no terrify immunity authored).
    dungeon::EnemyTeam plain;
    for (const auto& [id, def] : db().bosses()) {
        if (def.statusImmunities.empty() && !def.immuneToAfflictions && def.guildTown == 0) {
            plain.bossId = id;
            plain.name = def.name;
            break;
        }
    }
    REQUIRE_FALSE(plain.bossId.empty());
    plain.isBoss = true;
    battle::Battle b = battle::buildBattle(party, plain, db());
    b.useSkill(0, 2, *db().findSkill("summon_goose"));
    CHECK(battle::hasStatus(b.units[2], content::StatusType::Terrified));

    // The Dragon (listed) and the Duck (blanket) shrug it off entirely.
    for (const char* bossId : {"the_dragon", "the_deadly_duck"}) {
        if (db().findBoss(bossId) == nullptr) {
            continue;  // id drift guard; the Dragon id is pinned elsewhere
        }
        Party p2 = summonParty();
        dungeon::EnemyTeam t;
        t.bossId = bossId;
        t.isBoss = true;
        t.name = bossId;
        battle::Battle bi = battle::buildBattle(p2, t, db());
        bi.useSkill(0, 2, *db().findSkill("summon_sentinel"));
        CHECK_FALSE(battle::hasStatus(bi.units[2], content::StatusType::Terrified));
    }
}

TEST_CASE("summons: no enemy AI ever answers a call", "[summons]") {
    // Even a foe HANDED a summon skill skips it (the belt over the content
    // rule that no enemy learns one).
    Party party = summonParty();
    dungeon::EnemyTeam team;
    for (const auto& [id, def] : db().enemies()) {
        if (!def.bossOnly) {
            team.enemyIds = {id};
            team.name = def.name;
            break;
        }
    }
    battle::Battle b = battle::buildBattle(party, team, db());
    const int foe = 2;
    b.units[foe].skillIds.insert(b.units[foe].skillIds.begin(), "summon_goose");
    b.units[foe].mp = b.units[foe].maxMp = 999;
    b.beginUnitTurn(foe);
    const battle::EnemyChoice c = battle::chooseEnemyAction(b, foe, db());
    CHECK(c.skillId != "summon_goose");
}
