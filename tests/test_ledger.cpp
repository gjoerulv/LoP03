// M109 - the economy ledger seam (game/Ledger.hpp): every gold/token movement
// in play routes through these helpers, which move the money AND tally it
// once, with town attribution; the defeat halving is a loss, never a spend.

#include <catch2/catch_test_macros.hpp>

#include "game/Ledger.hpp"
#include "game/Lifetime.hpp"
#include "game/Party.hpp"

using namespace cd;

TEST_CASE("ledger: earning moves the gold and tallies it once, with the town", "[lifetime][ledger]") {
    Party p;
    p.gold = 100;
    earnGold(p, 250, EconomySource::Chest, 3);
    REQUIRE(p.gold == 350);
    REQUIRE(p.lifetime.economy.goldEarned == 250);
    REQUIRE(p.lifetime.economy.largestPurse == 350);
    REQUIRE(lifetimeTown(p.lifetime, 3).goldEarned == 250);
    REQUIRE(lifetimeTown(p.lifetime, 1).goldEarned == 0);
    // Town 0: the castle - no town's ledger moves.
    earnGold(p, 5, EconomySource::CastleReward, 0);
    REQUIRE(p.lifetime.economy.goldEarned == 255);
    for (const TownLifetime& t : p.lifetime.towns) {
        REQUIRE(t.goldEarned <= 250);
    }
    // Nothing (or less) earned is nothing recorded.
    earnGold(p, 0, EconomySource::Chest, 3);
    earnGold(p, -7, EconomySource::Chest, 3);
    REQUIRE(p.gold == 355);
    REQUIRE(p.lifetime.economy.goldEarned == 255);
}

TEST_CASE("ledger: the largest purse is the peak, never the current gold", "[lifetime][ledger]") {
    Party p;
    earnGold(p, 900, EconomySource::BattleSpoils, 1);
    spendGold(p, 850, EconomySource::EquipShop, 1);
    earnGold(p, 10, EconomySource::BattleSpoils, 1);
    REQUIRE(p.gold == 60);
    REQUIRE(p.lifetime.economy.largestPurse == 900);
}

TEST_CASE("ledger: spending tallies by town, and the Inn keeps its own line", "[lifetime][ledger]") {
    Party p;
    p.gold = 500;
    spendGold(p, 120, EconomySource::Inn, 2);
    spendGold(p, 30, EconomySource::ItemShop, 2);
    spendGold(p, 40, EconomySource::Training, 5);
    REQUIRE(p.gold == 310);
    REQUIRE(p.lifetime.economy.goldSpent == 190);
    REQUIRE(p.lifetime.economy.innGold == 120);
    REQUIRE(lifetimeTown(p.lifetime, 2).goldSpent == 150);
    REQUIRE(lifetimeTown(p.lifetime, 5).goldSpent == 40);
    // Defensive: a spend can never drive the purse negative.
    spendGold(p, 1000, EconomySource::BlackMarket, 2);
    REQUIRE(p.gold == 0);
    REQUIRE(p.lifetime.economy.goldSpent == 1190);
}

TEST_CASE("ledger: the defeat halving is a loss apart from spending", "[lifetime][ledger]") {
    Party p;
    p.gold = 301;
    loseGold(p, p.gold - p.gold / 2);
    REQUIRE(p.gold == 150);
    REQUIRE(p.lifetime.economy.goldLost == 151);
    REQUIRE(p.lifetime.economy.goldSpent == 0);
    REQUIRE(p.lifetime.economy.goldEarned == 0);
}

TEST_CASE("ledger: legendary tokens earn and spend on their own counters", "[lifetime][ledger]") {
    Party p;
    earnTokens(p, 2, EconomySource::BossDrop);
    earnTokens(p, 1, EconomySource::EliteChallenge);
    spendTokens(p, 1, EconomySource::TokenExchange);
    REQUIRE(p.legendaryTokens == 2);
    REQUIRE(p.lifetime.economy.tokensEarned == 3);
    REQUIRE(p.lifetime.economy.tokensSpent == 1);
    spendTokens(p, 10, EconomySource::BlackMarket);  // clamps, still tallies the ask
    REQUIRE(p.legendaryTokens == 0);
    REQUIRE(p.lifetime.economy.tokensSpent == 11);
}

TEST_CASE("ledger: purchases, finds, learns and level-ups tally as counts", "[lifetime][ledger]") {
    Party p;
    recordItemBought(p);
    recordItemBought(p);
    recordEquipmentBought(p);
    recordPassiveBought(p);
    recordTreasureFound(p);
    recordCurioFound(p);
    recordMapTreasureCompleted(p);
    recordScrollLearned(p, 2);
    recordScrollLearned(p, 9);  // an invalid slot still counts the save's total
    recordLevelUps(p, 3);
    recordLevelUps(p, 0);
    recordLevelUps(p, -2);
    const EconomyLifetime& e = p.lifetime.economy;
    REQUIRE(e.itemsBought == 2);
    REQUIRE(e.equipmentBought == 1);
    REQUIRE(e.passivesBought == 1);
    REQUIRE(e.treasureFound == 1);
    REQUIRE(e.curiosFound == 1);
    REQUIRE(e.mapTreasures == 1);
    REQUIRE(e.scrollsLearned == 2);
    REQUIRE(p.lifetime.members[2].scrollsLearned == 1);
    REQUIRE(e.levelUps == 3);
}
