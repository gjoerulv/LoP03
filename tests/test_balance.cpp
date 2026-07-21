#include <catch2/catch_test_macros.hpp>

#ifdef CRYSTAL_TEST_DATA_DIR
#include <filesystem>
#include <string>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "danger/DangerRating.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"
#include "score/Scoring.hpp"

using namespace cd;

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

Party makeParty(const content::ContentDatabase& db, int level) {
    Party p;
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        if (const content::ClassDef* cls = db.findClass(id)) {
            p.members.push_back(createCharacter(*cls, id, level));
        }
    }
    return p;
}

int firstGateTeam(const dungeon::Dungeon& d) {
    for (const dungeon::Room& r : d.rooms) {
        for (int dir = 0; dir < dungeon::kDirCount; ++dir) {
            const dungeon::Door& door = r.doors[static_cast<std::size_t>(dir)];
            if (door.gated && door.teamIndex >= 0) {
                return door.teamIndex;
            }
        }
    }
    return -1;
}

int totalThreat(const dungeon::Dungeon& d, const content::ContentDatabase& db) {
    int sum = 0;
    for (const dungeon::EnemyTeam& t : d.teams) {
        sum += danger::teamThreat(t, db);
    }
    return sum;
}

// A strong-but-buyable endgame loadout (M32 town-ladder battery): the realistic
// in-game power ceiling used to check the top town is clearable, not trivial.
void gearUp(Party& party, const content::ContentDatabase& db) {
    for (Character& c : party.members) {
        if (c.classId == "mage") {
            c.weapon = "crystal_wand";
            c.armor = "mage_robe";
        } else if (c.classId == "cleric") {
            c.weapon = "holy_mace";
            c.armor = "chain_mail";
        } else {
            c.weapon = "war_hammer";
            c.armor = "plate_armor";
        }
        c.accessory = "titan_heart";  // +50 HP survives the top-town stat scale
        refreshCharacter(c, db);
    }
}

// Full legendary loadout (M34 black-market gear), the aspirational endgame kit.
void legendaryUp(Party& party, const content::ContentDatabase& db) {
    for (Character& c : party.members) {
        c.weapon = (c.classId == "mage") ? "stormcaller_rod" : "dawnforged_blade";
        c.armor = "aegis_eternal";
        c.accessory = "titanforged_heart";
        refreshCharacter(c, db);
    }
}

}  // namespace

TEST_CASE("balance: a starting party can clear a depth-1 gate", "[balance]") {
    const content::ContentDatabase db = loadContent();
    const dungeon::Dungeon d = dungeon::generate(2024, 1, db, "ruined_keep");
    const int gate = firstGateTeam(d);
    REQUIRE(gate >= 0);

    battle::Battle b = battle::buildBattle(makeParty(db, 1), d.teams[static_cast<std::size_t>(gate)], db);
    const battle::SimResult r = battle::simulate(b, db);
    REQUIRE(r.outcome == battle::Outcome::Victory);
    REQUIRE(r.partyAlive >= 1);
}

TEST_CASE("balance: deeper dungeons are more threatening", "[balance]") {
    const content::ContentDatabase db = loadContent();
    const dungeon::Dungeon shallow = dungeon::generate(99, 1, db, "ruined_keep");
    const dungeon::Dungeon deep = dungeon::generate(99, 8, db, "ruined_keep");
    REQUIRE(totalThreat(deep, db) > totalThreat(shallow, db));
}

TEST_CASE("balance: difficulty rises with depth for the same party", "[balance]") {
    const content::ContentDatabase db = loadContent();

    const dungeon::Dungeon d1 = dungeon::generate(555, 1, db, "ruined_keep");
    battle::Battle b1 = battle::buildBattle(makeParty(db, 3), d1.teams[static_cast<std::size_t>(firstGateTeam(d1))], db);
    const battle::SimResult shallow = battle::simulate(b1, db);

    const dungeon::Dungeon d8 = dungeon::generate(555, 8, db, "ruined_keep");
    battle::Battle b8 = battle::buildBattle(makeParty(db, 3), d8.teams[static_cast<std::size_t>(firstGateTeam(d8))], db);
    const battle::SimResult deep = battle::simulate(b8, db);

    // The same level-3 party fares worse against a depth-8 gate than a depth-1 one.
    REQUIRE(deep.partyHpFraction() <= shallow.partyHpFraction());
}

TEST_CASE("balance: a prepared party can defeat a depth-1 boss", "[balance]") {
    const content::ContentDatabase db = loadContent();
    const dungeon::Dungeon d = dungeon::generate(7777, 1, db, "ruined_keep");
    const int bossTeam = d.rooms[static_cast<std::size_t>(d.bossRoom)].teamIndex;
    REQUIRE(bossTeam >= 0);

    battle::Battle b = battle::buildBattle(makeParty(db, 6), d.teams[static_cast<std::size_t>(bossTeam)], db);
    const battle::SimResult r = battle::simulate(b, db);
    REQUIRE(r.outcome == battle::Outcome::Victory);
}

TEST_CASE("balance: a geared level-1 party can defeat a depth-1 boss", "[balance][diag]") {
    const content::ContentDatabase db = loadContent();
    Party party = makeParty(db, 1);
    // Equip a strong-but-buyable loadout (the realistic in-game ceiling).
    for (Character& c : party.members) {
        c.weapon = (c.classId == "mage") ? "crystal_wand" : "steel_sword";
        c.armor = (c.classId == "mage") ? "mage_robe" : "chain_mail";
        c.accessory = "titan_heart";
        refreshCharacter(c, db);
    }
    const dungeon::Dungeon d = dungeon::generate(7777, 1, db, "ruined_keep");
    const int bossTeam = d.rooms[static_cast<std::size_t>(d.bossRoom)].teamIndex;
    battle::Battle b = battle::buildBattle(party, d.teams[static_cast<std::size_t>(bossTeam)], db);
    const battle::SimResult r = battle::simulate(b, db);
    INFO("boss outcome=" << static_cast<int>(r.outcome) << " rounds=" << r.rounds
                         << " partyHp%=" << r.partyHpFraction());
    REQUIRE(r.outcome == battle::Outcome::Victory);
}

TEST_CASE("balance: a simulated full clear produces a sane score", "[balance]") {
    const content::ContentDatabase db = loadContent();
    const dungeon::Dungeon d = dungeon::generate(31415, 1, db, "ruined_keep");
    const Party basis = makeParty(db, 6);

    score::RunSummary run;
    run.completed = true;
    run.noDeath = true;

    // Walk the main path, fighting each gate, then the boss.
    bool clearedAll = true;
    for (std::size_t i = 0; i < d.teams.size(); ++i) {
        battle::Battle b = battle::buildBattle(basis, d.teams[i], db);
        const battle::SimResult r = battle::simulate(b, db);
        if (r.outcome != battle::Outcome::Victory) {
            clearedAll = false;
            break;
        }
        run.battleTurns += r.rounds;
        run.dangerDefeated += danger::tierWeight(danger::assess(d.teams[i], d.depth, db));
    }
    REQUIRE(clearedAll);

    const int total = score::computeScore(run);
    REQUIRE(total > 0);
    REQUIRE(run.battleTurns > 0);
}

// ---- M32 town-ladder balance battery ----

TEST_CASE("balance: a higher town is more threatening at the same depth", "[balance][townladder]") {
    const content::ContentDatabase db = loadContent();
    const dungeon::Dungeon t2 = dungeon::generate(2024, 5, db, "ruined_keep", 2);
    const dungeon::Dungeon t5 = dungeon::generate(2024, 5, db, "ruined_keep", 5);
    const dungeon::Dungeon t7 = dungeon::generate(2024, 5, db, "ruined_keep", 7);
    // Same seed/depth: only the town multiplier changes, so threat rises with town.
    REQUIRE(totalThreat(t5, db) > totalThreat(t2, db));
    REQUIRE(totalThreat(t7, db) > totalThreat(t5, db));
}

TEST_CASE("balance: a modest party clears an early town", "[balance][townladder]") {
    const content::ContentDatabase db = loadContent();
    const dungeon::Dungeon d = dungeon::generate(7777, 2, db, "ruined_keep", 2);
    const int bossTeam = d.rooms[static_cast<std::size_t>(d.bossRoom)].teamIndex;
    REQUIRE(bossTeam >= 0);
    battle::Battle b =
        battle::buildBattle(makeParty(db, 10), d.teams[static_cast<std::size_t>(bossTeam)], db);
    const battle::SimResult r = battle::simulate(b, db);
    INFO("town2 boss outcome=" << static_cast<int>(r.outcome) << " rounds=" << r.rounds
                               << " partyHp%=" << r.partyHpFraction());
    REQUIRE(r.outcome == battle::Outcome::Victory);
}

TEST_CASE("balance: a strong endgame party clears the top town", "[balance][townladder][diag]") {
    const content::ContentDatabase db = loadContent();
    Party party = makeParty(db, 50);  // level cap
    gearUp(party, db);
    const dungeon::Dungeon d = dungeon::generate(7777, 3, db, "ruined_keep", 7);
    const int bossTeam = d.rooms[static_cast<std::size_t>(d.bossRoom)].teamIndex;
    REQUIRE(bossTeam >= 0);
    battle::Battle b =
        battle::buildBattle(party, d.teams[static_cast<std::size_t>(bossTeam)], db);
    const battle::SimResult r = battle::simulate(b, db);
    INFO("town7 boss outcome=" << static_cast<int>(r.outcome) << " rounds=" << r.rounds
                               << " partyHp%=" << r.partyHpFraction());
    REQUIRE(r.outcome == battle::Outcome::Victory);
}

// ---- M34 legendary-gear balance ----

TEST_CASE("balance: legendary gear out-stats the best epic gear", "[balance][blackmarket]") {
    const content::ContentDatabase db = loadContent();
    // A deterministic stat comparison (combat outcomes carry enmity/turn-order
    // noise): the same character in the best epic-tier loadout vs full
    // legendaries. Legendaries must be a clear step up so the market is worth it.
    Character epic = createCharacter(*db.findClass("knight"), "Epic", 30);
    epic.weapon = "sunforged_greatblade";  // M37: the best epic weapon (town 7)
    epic.armor = "bulwark_of_ages";        // M37: the best epic armor (town 7)
    epic.accessory = "titan_heart";
    refreshCharacter(epic, db);

    Character leg = createCharacter(*db.findClass("knight"), "Legend", 30);
    leg.weapon = "worldbreaker_axe";  // M37: the strongest legendary weapon
    leg.armor = "aegis_eternal";
    leg.accessory = "titanforged_heart";
    refreshCharacter(leg, db);

    CHECK(leg.stats.attack > epic.stats.attack);
    CHECK(leg.stats.defense > epic.stats.defense);
    CHECK(leg.maxHp > epic.maxHp);
}

TEST_CASE("balance: a maxed legendary party can clear the M38 town-7 content", "[balance]") {
    const content::ContentDatabase db = loadContent();
    // The new per-town bosses/enemies (towns 2-7) must be beatable, not unwinnable:
    // a level-50 party in full legendaries clears essentially every town-7 boss.
    int wins = 0;
    const int seeds = 6;
    for (int s = 0; s < seeds; ++s) {
        const dungeon::Dungeon d = dungeon::generate(1000 + s, 8, db, "hollow_forest", 7);
        const int bossTeam = d.rooms[static_cast<std::size_t>(d.bossRoom)].teamIndex;
        REQUIRE(bossTeam >= 0);
        Party p = makeParty(db, 50);
        legendaryUp(p, db);
        battle::Battle b = battle::buildBattle(p, d.teams[static_cast<std::size_t>(bossTeam)], db);
        if (battle::simulate(b, db).outcome == battle::Outcome::Victory) {
            ++wins;
        }
    }
    INFO("town-7 boss wins: " << wins << "/" << seeds);
    CHECK(wins >= seeds - 1);
}

TEST_CASE("balance: legendaries do not let level trivialize the top town",
          "[balance][blackmarket]") {
    const content::ContentDatabase db = loadContent();
    const dungeon::Dungeon d = dungeon::generate(4242, 6, db, "ruined_keep", 7);  // town 7
    const int bossTeam = d.rooms[static_cast<std::size_t>(d.bossRoom)].teamIndex;
    REQUIRE(bossTeam >= 0);

    Party strong = makeParty(db, 50);
    legendaryUp(strong, db);
    Party weak = makeParty(db, 12);
    legendaryUp(weak, db);

    battle::Battle bs = battle::buildBattle(strong, d.teams[static_cast<std::size_t>(bossTeam)], db);
    battle::Battle bw = battle::buildBattle(weak, d.teams[static_cast<std::size_t>(bossTeam)], db);
    const battle::SimResult rs = battle::simulate(bs, db);
    const battle::SimResult rw = battle::simulate(bw, db);
    INFO("strong hp%=" << rs.partyHpFraction() << " weak hp%=" << rw.partyHpFraction());
    // Even in full legendaries, a low-level party fares clearly worse against the
    // town-7 boss than an endgame one: level still dominates gear, so legendaries
    // help without trivializing the climb.
    CHECK(rs.partyHpFraction() > rw.partyHpFraction());
}

#endif  // CRYSTAL_TEST_DATA_DIR
