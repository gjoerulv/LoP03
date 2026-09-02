#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "dungeon/ThemeEvents.hpp"  // themeEventHash — the seeded-draw idiom

// M104 (owner events 2 and 3): the pure rules behind the two gambling events.
// Everything here is a hash of (dungeon seed, room, draw index) — reload-honest
// like every event, no rng stream consumed. The STATES render and apply;
// these functions decide, so tests can pin every outcome headlessly.

namespace cd::gamble {

// ---- The reels -------------------------------------------------------------
// Owner spec, verbatim prices: ONE spin for 10 gold or THREE for 70 (the
// rip-off bundle is the joke and stays). The event is one-shot: play once,
// however many spins were bought, and the machine is gone.

inline constexpr int kReelOneSpinGold = 10;
inline constexpr int kReelThreeSpinGold = 70;

enum class ReelSymbol { TaxPapers, GooseHead, Spoon, Crown, RedX, BaldHead, Seven };
inline constexpr int kReelSymbolCount = 7;

inline const char* reelSymbolName(ReelSymbol s) {
    switch (s) {
        case ReelSymbol::TaxPapers: return "Tax Papers";
        case ReelSymbol::GooseHead: return "Goose Head";
        case ReelSymbol::Spoon: return "P-Spoon";
        case ReelSymbol::Crown: return "Crown";
        case ReelSymbol::RedX: return "Red X";
        case ReelSymbol::BaldHead: return "Bald Head";
        case ReelSymbol::Seven: return "7";
    }
    return "?";
}

// Symbol weights (sum 100) — cheap prizes common, the scroll and the 1000g
// rare. Owner-tunable constants; the milestone note carries the table.
inline constexpr std::array<int, kReelSymbolCount> kReelWeights = {22, 18, 14, 14, 14, 10, 8};
// Reels 2 and 3 copy reel 1 this often instead of drawing fresh — it lifts
// the three-of-a-kind rate to roughly one spin in five without flattening
// the symbol weights (pure independence would land near 2%).
inline constexpr int kReelMatchBiasPct = 35;

inline ReelSymbol weightedSymbol(std::uint64_t roll) {
    int v = static_cast<int>(roll % 100);
    for (int i = 0; i < kReelSymbolCount; ++i) {
        v -= kReelWeights[static_cast<std::size_t>(i)];
        if (v < 0) {
            return static_cast<ReelSymbol>(i);
        }
    }
    return ReelSymbol::Seven;
}

// The three symbols of spin `spin` (0-based). Deterministic per (seed, room).
inline std::array<ReelSymbol, 3> reelSpin(std::uint64_t seed, int room, int spin) {
    const std::uint64_t base = 0x2EE1500DA7A00000ull + static_cast<std::uint64_t>(spin) * 16;
    std::array<ReelSymbol, 3> out{};
    out[0] = weightedSymbol(dungeon::themeEventHash(seed, room, base));
    for (int r = 1; r < 3; ++r) {
        const std::uint64_t h =
            dungeon::themeEventHash(seed, room, base + static_cast<std::uint64_t>(r) * 2);
        if (static_cast<int>(h % 100) < kReelMatchBiasPct) {
            out[static_cast<std::size_t>(r)] = out[0];
        } else {
            out[static_cast<std::size_t>(r)] = weightedSymbol(
                dungeon::themeEventHash(seed, room, base + static_cast<std::uint64_t>(r) * 2 + 1));
        }
    }
    return out;
}

// The matched symbol of a spin, or -1 when the three differ anywhere.
inline int reelMatch(const std::array<ReelSymbol, 3>& s) {
    return (s[0] == s[1] && s[1] == s[2]) ? static_cast<int>(s[0]) : -1;
}

// ---- Blackjack -------------------------------------------------------------
// Minimal fair rules (documented assumption, owner may veto): dealer stands
// on 17, push returns the bet, a win pays the bet back doubled, no splits or
// doubles. Cards are an endless seeded stream of ranks 1..13 (suits carry no
// value and the bitmap font carries no suit glyphs).

inline constexpr int kDealerStands = 17;
inline constexpr std::array<int, 4> kBlackjackBets = {10, 25, 50, 100};

inline int blackjackCard(std::uint64_t seed, int room, int drawIndex) {
    constexpr std::uint64_t kSaltCards = 0xB1AC7AC4A2D50000ull;
    const std::uint64_t h = dungeon::themeEventHash(
        seed, room, kSaltCards + static_cast<std::uint64_t>(drawIndex));
    return 1 + static_cast<int>(h % 13);  // 1 = ace, 11..13 = face cards
}

inline const char* cardLabel(int rank) {
    static const char* const kLabels[] = {"A", "2", "3", "4",  "5", "6", "7",
                                          "8", "9", "10", "J", "Q", "K"};
    return (rank >= 1 && rank <= 13) ? kLabels[rank - 1] : "?";
}

// Best blackjack value of a hand: aces count 11 while that stays <= 21.
inline int handValue(const std::vector<int>& ranks) {
    int total = 0;
    int aces = 0;
    for (int r : ranks) {
        if (r == 1) {
            ++aces;
            total += 1;
        } else {
            total += r > 10 ? 10 : r;
        }
    }
    while (aces > 0 && total + 10 <= 21) {
        total += 10;
        --aces;
    }
    return total;
}

}  // namespace cd::gamble
