// M103 — the goose polymorph's transform: one member, the rest of the run,
// XP and levels carried back, vitals by percentage, the heirloom riding along
// (the M96 carve-out), KO preserved, and the refusal guards.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "game/Gooseform.hpp"
#include "game/Party.hpp"

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

Party knightAndGoose() {
    Party p;
    p.members.push_back(createCharacter(*db().findClass("knight"), "Rolan", 10));
    p.members.push_back(createCharacter(*db().findClass("goose"), "Pond", 10));
    return p;
}

}  // namespace

TEST_CASE("gooseform: enter swaps class, keeps the heirloom, maps vitals",
          "[gooseform][m103]") {
    Party p = knightAndGoose();
    p.members[0].equippedHeirloom = "heirloom_hearthstone";
    refreshCharacter(p.members[0], db());
    p.members[0].hp = p.members[0].maxHp / 2;

    const GooseformStash stash = enterGooseform(p, 0, db());
    REQUIRE(stash.memberIndex == 0);
    CHECK(isGoose(p.members[0]));
    CHECK(p.members[0].name == "Rolan");
    CHECK(p.members[0].equippedHeirloom == "heirloom_hearthstone");  // memories stay
    CHECK(p.members[0].weapon.empty());  // arms do not (a bare goose)
    // Roughly half health, mapped by percentage; never 0 for a survivor.
    CHECK(p.members[0].hp > 0);
    CHECK(p.members[0].hp <= p.members[0].maxHp);
}

TEST_CASE("gooseform: leave restores the real member with XP carried back",
          "[gooseform][m103]") {
    Party p = knightAndGoose();
    const int levelBefore = p.members[0].level;
    const GooseformStash stash = enterGooseform(p, 0, db());
    REQUIRE(stash.memberIndex == 0);

    // The goose fights on: levels and XP earned while waddling.
    grantXp(p.members[0], 400, db());
    REQUIRE(p.members[0].level > levelBefore);
    const int gooseLevel = p.members[0].level;
    const int gooseXp = p.members[0].xp;

    leaveGooseform(p, stash, db());
    CHECK_FALSE(isGoose(p.members[0]));
    CHECK(p.members[0].classId == "knight");
    CHECK(p.members[0].level == gooseLevel);  // waddled levels still count
    CHECK(p.members[0].xp == gooseXp);
    CHECK(p.members[0].hp > 0);
}

TEST_CASE("gooseform: KO maps to KO in both directions", "[gooseform][m103]") {
    Party p = knightAndGoose();
    const GooseformStash stash = enterGooseform(p, 0, db());
    REQUIRE(stash.memberIndex == 0);
    p.members[0].hp = 0;  // the goose falls
    leaveGooseform(p, stash, db());
    CHECK(p.members[0].hp == 0);  // the transformation never revives anyone
}

TEST_CASE("gooseform: refusals - born geese, bad indices, empty stashes",
          "[gooseform][m103]") {
    Party p = knightAndGoose();
    CHECK(anyNonGoose(p));
    CHECK(enterGooseform(p, 1, db()).memberIndex == -1);  // already a goose
    CHECK(enterGooseform(p, 7, db()).memberIndex == -1);  // out of range
    CHECK(p.members[1].classId == "goose");               // untouched either way

    // All geese: the event's gate.
    Party geese;
    geese.members.push_back(createCharacter(*db().findClass("goose"), "A", 5));
    geese.members.push_back(createCharacter(*db().findClass("goose"), "B", 5));
    CHECK_FALSE(anyNonGoose(geese));

    // An inactive stash restores nothing (and crashes nothing).
    leaveGooseform(p, GooseformStash{}, db());
    CHECK(p.members[0].classId == "knight");
}
