#include <catch2/catch_test_macros.hpp>

#include "content/Definitions.hpp"
#include "game/Party.hpp"

using namespace cd;

namespace {
content::ClassDef knightClass() {
    content::ClassDef c;
    c.id = "knight";
    c.name = "Knight";
    c.baseStats = {120, 18, 4, 16, 8};
    c.growth = {12.0f, 2.0f, 0.4f, 1.6f, 0.8f};
    return c;
}
}  // namespace

TEST_CASE("party: createCharacter derives level-1 stats from the class", "[game]") {
    const content::ClassDef knight = knightClass();
    const Character c = createCharacter(knight, "Rolan");

    REQUIRE(c.classId == "knight");
    REQUIRE(c.name == "Rolan");
    REQUIRE(c.level == 1);
    REQUIRE(c.stats.maxHp == 120);
    REQUIRE(c.stats.attack == 18);
    REQUIRE(c.maxHp == 120);
    REQUIRE(c.hp == 120);  // starts full
    REQUIRE(c.maxMp == deriveMaxMp(4));
    REQUIRE(c.mp == c.maxMp);
}

TEST_CASE("party: growth applies per level (truncated, deterministic)", "[game]") {
    const content::ClassDef knight = knightClass();
    const Character c3 = createCharacter(knight, "X", 3);
    REQUIRE(c3.level == 3);
    REQUIRE(c3.stats.maxHp == 120 + 24);  // +12 * 2
    REQUIRE(c3.stats.attack == 18 + 4);   // +2 * 2
    REQUIRE(c3.stats.magic == 4);         // +0.4 * 2 = 0.8 -> truncates to 0
}

TEST_CASE("party: healFull and highestLevel", "[game]") {
    const content::ClassDef knight = knightClass();
    Party p;
    p.members.push_back(createCharacter(knight, "A", 1));
    p.members.push_back(createCharacter(knight, "B", 4));
    p.members[0].hp = 1;
    p.members[1].mp = 0;

    healFull(p);
    REQUIRE(p.members[0].hp == p.members[0].maxHp);
    REQUIRE(p.members[1].mp == p.members[1].maxMp);
    REQUIRE(highestLevel(p) == 4);
}

TEST_CASE("party: a New Game resets the WHOLE object (nothing leaks a loaded save)",
          "[game][new-game]") {
    // Owner bug 2026-08-29: New Game after loading a save kept every field the
    // old hand-picked clear list missed - the town-1 Guild Boss stood unlocked
    // (with its perk bonuses ACTIVE) in a brand-new game. The contract is now a
    // whole-object reset (game/Party.hpp resetForNewGame); this dirties the
    // once-leaking fields and pins the slate clean.
    const content::ClassDef knight = knightClass();
    Party p;
    p.members.push_back(createCharacter(knight, "Old", 40));
    p.gold = 9999;
    p.restTokens = 3;
    p.doubleXpNext = true;
    p.usedSummons = {"summon_goose"};
    p.currentTown = 7;
    p.highestUnlockedTown = 7;
    p.stakes.prevTown = 7;
    p.stakes.prevDepth = 20;
    p.stakes.penaltySteps = 2;
    p.legendaryTokens = 5;
    p.castleUnlocked = true;
    p.gooseTownUnlocked = true;
    p.mapPieces = 2;
    p.mapPiecesOwed = 1;
    p.treasure.active = true;
    p.treasureScrollsAwarded = {"scroll_x"};
    p.eternalBestFloors = 9;
    p.ownedCurios = {"keep_banner"};
    guildRecord(p.guild, 1).unlocked = true;   // the reported symptom
    guildRecord(p.guild, 1).bestTurns = 5;
    guildRecord(p.guild, 1).perkId = "perk_exp";
    p.storyMet = 0x7f;
    p.seenCutscenes = {"new_game"};
    p.heirloomChoices = {"new_game:heirloom_lastlight"};
    p.strangerJokesTold = 4;
    p.encountered = {"bandit"};
    p.recordBiggestHit = 999;
    p.recordRunDamage = 12345;

    resetForNewGame(p);

    CHECK(p.members.empty());
    CHECK(p.gold == kNewGameGold);
    CHECK(p.restTokens == 0);
    CHECK_FALSE(p.doubleXpNext);
    CHECK(p.usedSummons.empty());
    CHECK(p.currentTown == 1);
    CHECK(p.highestUnlockedTown == 1);
    CHECK(p.stakes.prevTown == 0);
    CHECK(p.stakes.penaltySteps == 0);
    CHECK(p.legendaryTokens == 0);
    CHECK_FALSE(p.blackMarket.present);
    CHECK_FALSE(p.castleUnlocked);
    CHECK_FALSE(p.gooseTownUnlocked);
    CHECK(p.mapPieces == 0);
    CHECK(p.mapPiecesOwed == 0);
    CHECK_FALSE(p.treasure.active);
    CHECK(p.treasureScrollsAwarded.empty());
    CHECK(p.eternalBestFloors == 0);
    CHECK(p.ownedCurios.empty());
    CHECK_FALSE(guildRecord(p.guild, 1).unlocked);
    CHECK(guildRecord(p.guild, 1).bestTurns == 0);
    CHECK(guildRecord(p.guild, 1).perkId.empty());
    CHECK(p.storyMet == 0);
    CHECK(p.seenCutscenes.empty());
    CHECK(p.heirloomChoices.empty());
    CHECK(p.strangerJokesTold == 0);
    CHECK(p.encountered.empty());
    CHECK(p.recordBiggestHit == 0);
    CHECK(p.recordRunDamage == 0);
    CHECK(p.inventory.stacks.empty());
}
