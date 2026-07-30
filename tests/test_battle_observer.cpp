// M60 — the battle observer's zero-effect (parity) guarantee and the
// recorder's reconciliation. The same seeded battle is resolved twice — once
// bare, once with a recorder attached — and must produce byte-identical
// outcomes, unit states, and rollCursor: the proof that telemetry is
// record-only and needs no kBattleRulesVersion bump. The recorder's effective
// amounts must then reconcile exactly against every unit's HP delta.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "dungeon/DungeonModel.hpp"
#include "editor/BattleRecorder.hpp"
#include "game/Party.hpp"

namespace {

using namespace cd;

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

// A team that exercises the whole event surface: poison DoT (mire_imp),
// Blind rolls (hex_wisp's smoke_screen), healing pressure, and enough
// muscle that party members get hurt and healed.
battle::Battle statusHeavyBattle(std::uint64_t seed) {
    dungeon::EnemyTeam team;
    team.name = "Observer gauntlet";
    team.enemyIds = {"mire_imp", "hex_wisp", "ogre_marauder", "grave_chanter"};
    team.statScalePct = 120;
    battle::Battle b = battle::buildBattle(makeParty(8), team, db());
    b.rngSeed = seed;
    return b;
}

}  // namespace

TEST_CASE("observer: attaching a recorder changes nothing at all", "[battle][editor]") {
    for (std::uint64_t seed : {0x0BEE5EEDull, 0xDEADBEA7ull, 0x60606060ull}) {
        battle::Battle bare = statusHeavyBattle(seed);
        battle::Battle watched = statusHeavyBattle(seed);
        cd::editor::BattleRecorder recorder;
        recorder.bind(watched);
        watched.observer = &recorder;

        const battle::SimResult r1 = battle::simulateInPlace(bare, db());
        const battle::SimResult r2 = battle::simulateInPlace(watched, db());

        REQUIRE(r1.outcome == r2.outcome);
        REQUIRE(r1.rounds == r2.rounds);
        REQUIRE(r1.partyHpRemaining == r2.partyHpRemaining);
        REQUIRE(r1.partyAlive == r2.partyAlive);
        REQUIRE(bare.rollCursor == watched.rollCursor);  // the seeded stream never moved
        REQUIRE(bare.turnsTaken == watched.turnsTaken);
        REQUIRE(bare.units.size() == watched.units.size());
        for (std::size_t i = 0; i < bare.units.size(); ++i) {
            REQUIRE(bare.units[i].hp == watched.units[i].hp);
            REQUIRE(bare.units[i].mp == watched.units[i].mp);
            REQUIRE(bare.units[i].statuses.size() == watched.units[i].statuses.size());
        }
        REQUIRE(recorder.eventCount() > 0);
    }
}

TEST_CASE("observer: recorded amounts reconcile against every HP delta", "[battle][editor]") {
    battle::Battle b = statusHeavyBattle(0x0BEE5EEDull);
    std::vector<int> initialHp;
    for (const battle::Combatant& u : b.units) {
        initialHp.push_back(u.hp);
    }
    cd::editor::BattleRecorder recorder;
    recorder.bind(b);
    b.observer = &recorder;
    battle::simulateInPlace(b, db());

    REQUIRE(recorder.units().size() == b.units.size());
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        const cd::editor::UnitTally& tally = recorder.units()[i];
        INFO(tally.name);
        const long delta = static_cast<long>(b.units[i].hp) - initialHp[i];
        REQUIRE(delta == tally.healingReceived - tally.taken);
    }
    // Action tallies exist and count real uses.
    REQUIRE_FALSE(recorder.actions().empty());
    long uses = 0;
    for (const auto& [id, tally] : recorder.actions()) {
        REQUIRE(tally.uses > 0);
        uses += tally.uses;
    }
    REQUIRE(uses > 0);
}

TEST_CASE("observer: a battle with no observer needs no recorder machinery", "[battle]") {
    // The default-null path is the one the game and Simulator always take;
    // running twice from the same seed is bit-repeatable (regression guard
    // around the emit-site edits).
    battle::Battle a = statusHeavyBattle(0x5EED0001ull);
    battle::Battle b = statusHeavyBattle(0x5EED0001ull);
    const battle::SimResult r1 = battle::simulateInPlace(a, db());
    const battle::SimResult r2 = battle::simulateInPlace(b, db());
    REQUIRE(r1.outcome == r2.outcome);
    REQUIRE(r1.rounds == r2.rounds);
    REQUIRE(a.rollCursor == b.rollCursor);
    REQUIRE(a.observer == nullptr);
}
