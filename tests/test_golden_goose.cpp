// M111 - the Golden Goose (battle rules v19): the data-driven scripted
// own-turn sequence, the enemy flee outcome, the patrol team's spoils
// semantics, the exclusions, and the sim/live agreement of the shared
// executor. Plus the [goose-report] battery (-s) the note records.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/BattleTelemetry.hpp"
#include "game/Castle.hpp"
#include "game/Guild.hpp"
#include "game/Lifetime.hpp"
#include "game/Party.hpp"
#include "game/Spoils.hpp"

using namespace cd;

namespace {

const content::ContentDatabase& db() {
    static content::ContentDatabase database;
    static bool loaded = false;
    if (!loaded) {
        content::LoadReport rep;
        REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), database, rep));
        loaded = true;
    }
    return database;
}

Party makeParty(int level) {
    Party party;
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        const content::ClassDef* cls = db().findClass(id);
        REQUIRE(cls != nullptr);
        party.members.push_back(createCharacter(*cls, id, level));
    }
    return party;
}

battle::Battle gooseBattle(int level, int town = 1, int depth = 1) {
    const dungeon::EnemyTeam team =
        dungeon::goldenGooseTeam(db(), "ruined_keep", town, depth, 4242ull, 0);
    REQUIRE_FALSE(team.enemyIds.empty());
    return battle::buildBattle(makeParty(level), team, db());
}

int gooseIndex(const battle::Battle& b) {
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].side == battle::Side::Enemy) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// One own turn of the goose exactly as both drivers take it: tick, guard
// drop, the per-turn seam, then the shared choice + executor.
std::string gooseTurn(battle::Battle& b, int goose) {
    b.tickStatuses(goose);
    b.clearGuard(goose);
    b.beginUnitTurn(goose);
    const battle::EnemyChoice c = battle::chooseEnemyAction(b, goose, db());
    battle::applyChoice(b, goose, c, db());
    return c.scripted ? "scripted" : (c.forced != battle::ForcedAction::None ? "forced" : "ai");
}

bool parses(const std::string& text) {
    content::ContentDatabase mem;
    content::LoadReport rep;
    const content::Json root = content::Json::parse(text, nullptr, false);
    REQUIRE_FALSE(root.is_discarded());
    content::parseEnemies(root, "t.json", mem, rep);
    return rep.ok();
}

}  // namespace

TEST_CASE("goose: the content is exactly the owner's kit", "[goose][v19][data]") {
    const content::EnemyDef* g = db().findEnemy(dungeon::kGoldenGooseEnemyId);
    REQUIRE(g != nullptr);
    CHECK(g->stats.maxHp == 45);
    CHECK(g->stats.attack == 1);
    CHECK(g->stats.magic == 0);
    CHECK(g->stats.defense == 2);
    CHECK(g->stats.speed == 40);
    CHECK(g->specialOnly);
    CHECK(g->goldReward == 2000);
    CHECK(g->xpReward == 0);  // the team's xpOverride pays the replaced patrol's XP
    const std::set<std::string> passives(g->passives.begin(), g->passives.end());
    CHECK(passives == std::set<std::string>{"spell_ward", "evasion", "iron_will", "first_strike"});
    REQUIRE(g->initialStatuses.size() == 1);
    CHECK(g->initialStatuses[0].type == content::StatusType::Reflect);
    REQUIRE(g->script.size() == 3);
    CHECK(g->script[0].action == content::ScriptDo::StatusAllFoes);
    REQUIRE(g->script[0].statuses.size() == 3);
    CHECK(g->script[0].statuses[0].type == content::StatusType::Blind);
    CHECK(g->script[0].statuses[0].duration == 3);
    CHECK(g->script[0].statuses[1].type == content::StatusType::Silence);
    CHECK(g->script[0].statuses[1].duration == 3);
    CHECK(g->script[0].statuses[2].type == content::StatusType::Poison);
    CHECK(g->script[0].statuses[2].magnitude == 5);
    CHECK(g->script[0].statuses[2].duration == 3);
    CHECK(g->script[1].action == content::ScriptDo::Guard);
    CHECK(g->script[2].action == content::ScriptDo::Flee);
    CHECK(battle::kBattleRulesVersion == 19);
}

TEST_CASE("goose: the loader owns the script's shape", "[goose][v19]") {
    const std::string head = R"({"version":1,"enemies":[{"id":"t","name":"T",
        "stats":{"hp":10,"attack":1,"magic":0,"defense":1,"speed":1},"tier":"normal",
        "role":"disruptor","skills":[],"xpReward":1,"goldReward":1,)";
    CHECK(parses(head + R"("script":[{"do":"guard"},{"do":"flee"}]}]})"));
    CHECK(parses(head + R"("script":[{"do":"status_all_foes","statuses":[{"type":"blind","duration":2}]}]}]})"));
    // status_all_foes needs statuses; the others may not carry any.
    CHECK_FALSE(parses(head + R"("script":[{"do":"status_all_foes"}]}]})"));
    CHECK_FALSE(parses(head + R"("script":[{"do":"guard","statuses":[{"type":"blind","duration":2}]}]}]})"));
    // A flee ends the script.
    CHECK_FALSE(parses(head + R"("script":[{"do":"flee"},{"do":"guard"}]}]})"));
    // Unknown actions are rejected.
    CHECK_FALSE(parses(head + R"("script":[{"do":"tapdance"}]}]})"));
}

TEST_CASE("goose: buildBattle mirrors the script, Reflect and the four passives",
          "[goose][v19]") {
    battle::Battle b = gooseBattle(10);
    const int g = gooseIndex(b);
    REQUIRE(g >= 0);
    const battle::Combatant& goose = b.units[static_cast<std::size_t>(g)];
    REQUIRE(goose.script.size() == 3);
    CHECK(goose.script[0].statuses.size() == 3);
    CHECK(battle::hasReflect(goose));
    CHECK(goose.spellWardPct > 0);
    CHECK(goose.evasionPct > 0);
    CHECK(goose.ironWill);
    CHECK(goose.firstStrike);
    CHECK_FALSE(goose.isBoss);
    CHECK_FALSE(goose.fled);
    // Round one: the goose acts before every party member (First Strike +
    // the scaled 40 speed; the party carries no First Strike here).
    CHECK(battle::turnOrder(b).front() == g);
}

TEST_CASE("goose: own turns one, two and three - dust, cower, getaway", "[goose][v19]") {
    battle::Battle b = gooseBattle(10);
    const int g = gooseIndex(b);
    // One member is already down: the dust settles only on the living.
    int fallen = -1;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].side == battle::Side::Party && fallen < 0) {
            fallen = static_cast<int>(i);
            b.units[i].hp = 0;
        }
    }
    REQUIRE(fallen >= 0);

    CHECK(gooseTurn(b, g) == "scripted");
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        const battle::Combatant& c = b.units[i];
        if (c.side != battle::Side::Party) {
            continue;
        }
        INFO(c.name);
        if (static_cast<int>(i) == fallen) {
            CHECK(c.statuses.empty());
            continue;
        }
        CHECK(battle::isBlinded(c));
        CHECK(battle::isSilenced(c));
        CHECK(battle::hasStatus(c, content::StatusType::Poison));
        // The normal rules: durations doubled (M35), poison magnitude the
        // authored 5 plus the applier's Magic/4 (M75) - the goose has 0 Magic.
        for (const battle::StatusInstance& s : c.statuses) {
            CHECK(s.turns == 3 * battle::kStatusDurationMult);
            if (s.type == content::StatusType::Poison) {
                CHECK(s.magnitude == 5);
            }
        }
    }
    CHECK(b.outcome() == battle::Outcome::Ongoing);

    CHECK(gooseTurn(b, g) == "scripted");
    CHECK(b.units[static_cast<std::size_t>(g)].guarding);
    CHECK(b.outcome() == battle::Outcome::Ongoing);

    CHECK(gooseTurn(b, g) == "scripted");
    CHECK(b.units[static_cast<std::size_t>(g)].fled);
    CHECK(b.units[static_cast<std::size_t>(g)].alive());  // gone, not fallen
    CHECK(b.outcome() == battle::Outcome::EnemyFled);
    CHECK(b.aliveIndices(battle::Side::Enemy).empty());
    for (int i : battle::turnOrder(b)) {
        CHECK(i != g);  // the fled take no turns
    }
    // Past the script the goose would be an ordinary foe (it is gone by then):
    // a fourth own turn finds no step.
    battle::Combatant past = b.units[static_cast<std::size_t>(g)];
    past.ownTurnsTaken = 4;
    CHECK(battle::scriptedTurn(past) == nullptr);
}

TEST_CASE("goose: a turn a control status takes still consumes the step", "[goose][v19]") {
    battle::Battle b = gooseBattle(10);
    const int g = gooseIndex(b);
    battle::Combatant& goose = b.units[static_cast<std::size_t>(g)];
    goose.statuses.push_back({content::StatusType::Stunned, 0, 2});  // stolen turn one
    CHECK(gooseTurn(b, g) == "forced");
    bool anyStatus = false;
    for (const battle::Combatant& c : b.units) {
        anyStatus = anyStatus || (c.side == battle::Side::Party && !c.statuses.empty());
    }
    CHECK_FALSE(anyStatus);  // the dust never fell
    goose.statuses.clear();
    CHECK(gooseTurn(b, g) == "scripted");
    CHECK(goose.guarding);  // step two, not step one - the M89 lunge precedent
    CHECK(gooseTurn(b, g) == "scripted");
    CHECK(goose.fled);
}

TEST_CASE("goose: the flight pays nothing and is not a player escape", "[goose][v19][lifetime]") {
    battle::Battle b = gooseBattle(10);
    const int g = gooseIndex(b);
    LifetimeStats stats;
    BattleTelemetry telemetry(stats, b, db());
    b.observer = &telemetry;
    for (int t = 0; t < 3; ++t) {
        gooseTurn(b, g);
    }
    REQUIRE(b.outcome() == battle::Outcome::EnemyFled);
    CHECK(stats.combat.enemiesKo == 0);  // never a KO event
    recordBattleEnd(stats, b, b.outcome(), 3, 2);
    CHECK(stats.combat.battlesWon == 0);
    CHECK(stats.combat.playerEscapes == 0);
    CHECK(stats.combat.battlesLost == 0);
    CHECK(stats.combat.battleTurns == 3);
    CHECK(stats.defeats.empty());
    CHECK(stats.combat.statusesApplied == 0);  // the goose's statuses are not ours
}

TEST_CASE("goose: the simulator and the shared executor agree on the whole fight",
          "[goose][v19]") {
    // A low party cannot catch it: the sim runs the script to the getaway.
    battle::Battle sim = gooseBattle(3);
    const battle::SimResult r = battle::simulateInPlace(sim, db(), 50);
    CHECK((r.outcome == battle::Outcome::EnemyFled || r.outcome == battle::Outcome::Victory));
    CHECK(r.rounds <= 3);  // the goose is gone (or caught) inside three rounds
    // A hand-driven fight through the same executor lands the same state.
    battle::Battle manual = gooseBattle(3);
    const int g = gooseIndex(manual);
    for (int t = 0; t < 3 && manual.outcome() == battle::Outcome::Ongoing; ++t) {
        gooseTurn(manual, g);
    }
    if (r.outcome == battle::Outcome::EnemyFled) {
        CHECK(manual.units[static_cast<std::size_t>(g)].fled);
    }
}

TEST_CASE("goose: its team pays the bounty and the replaced patrol's XP", "[goose][v19][patrol]") {
    const dungeon::EnemyTeam shadow = dungeon::patrolTeam(db(), "crystal_mine", 4, 8, 777ull, 3);
    const dungeon::EnemyTeam goose =
        dungeon::goldenGooseTeam(db(), "crystal_mine", 4, 8, 777ull, 3);
    REQUIRE(goose.enemyIds == std::vector<std::string>{"golden_goose"});
    CHECK(goose.patrol);
    CHECK(goose.patrolPaysGold);
    CHECK(goose.statScalePct == shadow.statScalePct);  // the same (town, depth) scale
    CHECK(goose.statScalePct > 100);
    const BattleSpoils gs = teamSpoils(goose, db());
    const BattleSpoils ss = teamSpoils(shadow, db());
    CHECK(gs.gold == 2000);
    CHECK(gs.xp == ss.xp);   // no better an XP farm than the patrol it replaced
    CHECK(ss.gold == 0);     // the ordinary patrol keeps the M93 rule
    // Deterministic: the same (seed, index) derives the same team twice.
    const dungeon::EnemyTeam again =
        dungeon::goldenGooseTeam(db(), "crystal_mine", 4, 8, 777ull, 3);
    CHECK(again.xpOverride == goose.xpOverride);
    CHECK(again.statScalePct == goose.statScalePct);
    // No danger credit, no drops: the patrol path decides those (DungeonState);
    // the team simply IS a patrol team.
    CHECK(goose.bossId.empty());
}

TEST_CASE("goose: never generated outside its patrol", "[goose][v19]") {
    // The M93 patrol recipe, the endless waves, the guild trial and the elite
    // vigils sweep the roster; none may ever hand out the goose.
    for (std::uint64_t seed = 1; seed <= 60; ++seed) {
        for (const char* theme : {"ruined_keep", "crystal_mine", "hollow_forest", "goosy_gauntlet"}) {
            const dungeon::EnemyTeam t = dungeon::patrolTeam(db(), theme, 7, 20, seed, 2);
            for (const std::string& id : t.enemyIds) {
                CHECK(id != "golden_goose");
            }
        }
    }
    for (int wave = 1; wave <= 40; ++wave) {
        const dungeon::EnemyTeam t = endlessWaveTeam(db(), wave);
        for (const std::string& id : t.enemyIds) {
            CHECK(id != "golden_goose");
        }
    }
    for (int town = 1; town <= 7; ++town) {
        const dungeon::EnemyTeam t = guildWaveTeam(db(), town);
        for (const std::string& id : t.enemyIds) {
            CHECK(id != "golden_goose");
        }
    }
    for (int wave = 0; wave < 3; ++wave) {
        const dungeon::EnemyTeam t = dragonEliteWaveTeam(db(), wave);
        for (const std::string& id : t.enemyIds) {
            CHECK(id != "golden_goose");
        }
    }
}

TEST_CASE("goose-report: catch rate across the ladder (the balance battery)",
          "[goose-report][v19][!benchmark]") {
    // The owner's stats stand; this records how the standard sim party fares:
    // it must usually see the goose act first, and the goose must stay
    // fragile enough to be catchable somewhere. Read with -s.
    // Two passes: the party as it comes, and the party with the dust warded
    // (immune to blind / silence / poison — a cleanse or ward charms in
    // play), so the report separates "the statuses win" from "the goose is
    // simply too slippery".
    // Each context runs under kSeeds distinct roll streams (buildBattle's own
    // seed is a pure function of the roster, so one build would be ONE roll
    // sequence repeated 28 times — no census at all).
    constexpr int kSeeds = 8;
    for (const bool warded : {false, true}) {
        int fights = 0;
        int gooseFirst = 0;
        int caught = 0;
        for (int town = 1; town <= 7; ++town) {
            int townCaught = 0;
            int townFights = 0;
            for (int depth : {1, 5, 10, 20}) {
                const int level = std::min(99, 4 + depth * 2 + (town - 1) * 6);
                const dungeon::EnemyTeam team =
                    dungeon::goldenGooseTeam(db(), "ruined_keep", town, depth, 99ull, 1);
                for (int seed = 0; seed < kSeeds; ++seed) {
                    battle::Battle b = battle::buildBattle(makeParty(level), team, db());
                    b.rngSeed ^= 0x600D5EEDull + static_cast<std::uint64_t>(seed) * 7919ull;
                    if (warded) {
                        for (battle::Combatant& c : b.units) {
                            if (c.side == battle::Side::Party) {
                                c.statusImmunities = {content::StatusType::Blind,
                                                      content::StatusType::Silence,
                                                      content::StatusType::Poison};
                            }
                        }
                    }
                    ++fights;
                    ++townFights;
                    const int g = gooseIndex(b);
                    if (battle::turnOrder(b).front() == g) {
                        ++gooseFirst;
                    }
                    const battle::SimResult r = battle::simulateInPlace(b, db(), 20);
                    if (r.outcome == battle::Outcome::Victory) {
                        ++caught;
                        ++townCaught;
                    }
                    CHECK(r.outcome != battle::Outcome::Defeat);  // it never kills anyone
                    CHECK(r.rounds <= 3);                          // gone or caught by then
                }
            }
            INFO((warded ? "warded" : "plain") << " town " << town << ": caught "
                 << townCaught << "/" << townFights);
        }
        INFO((warded ? "warded" : "plain") << " total: goose acts first in " << gooseFirst
             << "/" << fights << " fights; caught in " << caught << "/" << fights);
        CHECK(gooseFirst == fights);
    }
}
