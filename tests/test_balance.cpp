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

#endif  // CRYSTAL_TEST_DATA_DIR
