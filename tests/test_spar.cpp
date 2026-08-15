// M94 — the sparring mirror: battle::buildSparBattle builds the party against
// exact enemy-side echoes of itself, deterministically, with no path back to
// the real party (partyIndex -1). The zero-stakes rule itself is structural
// (SparState snapshots and restores the whole Party object), so what the
// tests referee is the mirror's fidelity and the battle's resolvability.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "game/Party.hpp"
#include "game/Spoils.hpp"

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

Party sparParty() {
    Party p;
    p.members.push_back(createCharacter(*db().findClass("knight"), "Rolan", 12));
    p.members.push_back(createCharacter(*db().findClass("mage"), "Mira", 12));
    p.members.push_back(createCharacter(*db().findClass("cleric"), "Ana", 12));
    p.members[0].weapon = "iron_sword";
    for (Character& c : p.members) {
        refreshCharacter(c, db());
    }
    return p;
}

}  // namespace

TEST_CASE("spar: the mirror is exact, enemy-side, and severed from the party",
          "[spar]") {
    const Party party = sparParty();
    battle::Battle b = battle::buildSparBattle(party, db());
    REQUIRE(b.units.size() == party.members.size() * 2);

    for (std::size_t i = 0; i < party.members.size(); ++i) {
        const battle::Combatant& real = b.units[i];
        const battle::Combatant& echo = b.units[party.members.size() + i];
        CHECK(real.side == battle::Side::Party);
        CHECK(echo.side == battle::Side::Enemy);
        CHECK(echo.partyIndex == -1);  // nothing ever writes back
        CHECK(echo.name == "Echo " + real.name);
        // The mirror is exact where it matters: stats, pools, kit, element.
        CHECK(echo.stats.attack == real.stats.attack);
        CHECK(echo.stats.speed == real.stats.speed);
        CHECK(echo.maxHp == real.maxHp);
        CHECK(echo.maxMp == real.maxMp);
        CHECK(echo.skillIds == real.skillIds);
        CHECK(echo.weaponElement == real.weaponElement);
        CHECK_FALSE(echo.uncontrolled);  // the driver decides for the echo
    }

    // Deterministic: the same party mirrors to the same battle seed.
    CHECK(battle::buildSparBattle(party, db()).rngSeed == b.rngSeed);
    // The threat table covers the full roster (the finalization lockstep).
    CHECK(b.threat.size() == b.units.size());
}

TEST_CASE("spar: both-sides AI resolves the mirror, and the mirror pays nothing",
          "[spar]") {
    const Party party = sparParty();
    battle::Battle b = battle::buildSparBattle(party, db());
    const battle::SimResult r = battle::simulateInPlace(b, db(), 400);
    // A mirror match ends one way or the other inside the cap — never hangs.
    CHECK(r.outcome != battle::Outcome::Ongoing);
    CHECK(r.rounds > 0);

    // The mirror "team" itself carries no rewards (the structural guarantee is
    // SparState's whole-party restore; this referees the belt).
    dungeon::EnemyTeam mirror;
    mirror.name = "The Echoes";
    const BattleSpoils s = teamSpoils(mirror, db());
    CHECK(s.xp == 0);
    CHECK(s.gold == 0);
}
