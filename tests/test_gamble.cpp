// M104 — the pure gamble rules: seeded reels (deterministic, weighted, with
// the match bias in its authored band) and the blackjack card stream + hand
// values (soft aces). The STATES only render these decisions.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <set>

#include "game/Gamble.hpp"

using namespace cd;

TEST_CASE("gamble: reels are reload-honest and weighted", "[gamble][m104]") {
    // Same (seed, room, spin) -> same symbols, always.
    for (std::uint64_t seed = 1; seed <= 50; ++seed) {
        for (int spin = 0; spin < 3; ++spin) {
            CHECK(gamble::reelSpin(seed, 3, spin) == gamble::reelSpin(seed, 3, spin));
        }
    }
    // Every symbol shows up somewhere, and the per-spin match rate sits in
    // the authored band (roughly one in five; assert loosely 8%..35%).
    std::set<int> seen;
    int matches = 0;
    int spins = 0;
    for (std::uint64_t seed = 1; seed <= 700; ++seed) {
        const std::array<gamble::ReelSymbol, 3> s = gamble::reelSpin(seed, 5, 0);
        for (const gamble::ReelSymbol sym : s) {
            seen.insert(static_cast<int>(sym));
        }
        ++spins;
        if (gamble::reelMatch(s) >= 0) {
            ++matches;
        }
    }
    CHECK(seen.size() == static_cast<std::size_t>(gamble::kReelSymbolCount));
    CHECK(matches * 100 / spins >= 8);
    CHECK(matches * 100 / spins <= 35);
}

TEST_CASE("gamble: the reel weights sum to a clean hundred", "[gamble][m104]") {
    int sum = 0;
    for (int w : gamble::kReelWeights) {
        CHECK(w > 0);
        sum += w;
    }
    CHECK(sum == 100);
}

TEST_CASE("gamble: blackjack cards are ranked, seeded, and replayable", "[gamble][m104]") {
    std::set<int> ranks;
    for (int i = 0; i < 300; ++i) {
        const int r = gamble::blackjackCard(42, 7, i);
        CHECK(r >= 1);
        CHECK(r <= 13);
        ranks.insert(r);
        CHECK(r == gamble::blackjackCard(42, 7, i));  // reload-honest
    }
    CHECK(ranks.size() == 13);  // the whole rank space appears over a long shoe
}

TEST_CASE("gamble: hand values count aces softly", "[gamble][m104]") {
    CHECK(gamble::handValue({1, 13}) == 21);      // A + K
    CHECK(gamble::handValue({1, 1, 9}) == 21);    // A + A + 9 (one soft ace)
    CHECK(gamble::handValue({1, 1, 1}) == 13);    // three aces
    CHECK(gamble::handValue({10, 9, 1}) == 20);   // the ace goes hard
    CHECK(gamble::handValue({10, 12, 5}) == 25);  // a bust stays a bust
    CHECK(gamble::handValue({7, 8}) == 15);
}
