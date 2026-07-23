#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "game/BlackMarket.hpp"  // blackMarketHash (the shared seeded one-shot stream)

// Royal Relics (M44): four unique consumable relics granted by a rare room event
// that REPLACES an ordinary rolled event during generation. Everything here is
// pure and seeded — the replacement chance is a table over (town, depth), and the
// grant is a hash of (dungeon seed, room index) plus what the party already owns,
// so reloading and re-entering the room cannot reroll the pick. Headless-tested.

namespace cd {

// --- The four relics --------------------------------------------------------
// Weights are percentages of the base roll and sum to 100. Ownership excludes a
// relic and the remainder is renormalized proportionally; owning ALL four falls
// back to the base roll and grants a duplicate (owner decision, 2026-07-22).
inline constexpr int kRelicCount = 4;
struct RelicEntry {
    const char* id;
    int weight;
};
inline constexpr std::array<RelicEntry, kRelicCount> kRelics{{
    {"evil_goose", 40},
    {"tax_sheets", 40},
    {"dragon_crown", 15},
    {"deadly_spoon", 5},
}};

// --- Event replacement chance ----------------------------------------------
inline constexpr int kRelicEventMinTown = 2;
inline constexpr int kRelicEventMinDepth = 2;
inline constexpr int kRelicEventDeepDepth = 20;  // the depth bonus applies from here up
inline constexpr int kRelicEventDeepBonusPct = 5;

// Percent chance that an eligible rolled event is replaced by the relic event.
// 0 outside the eligible band, so the caller never even draws there.
inline int relicEventChancePct(int town, int depth) {
    if (town < kRelicEventMinTown || depth < kRelicEventMinDepth) {
        return 0;
    }
    int pct = 5;  // towns 3..6
    if (town <= 2) {
        pct = 3;
    } else if (town >= 7) {
        pct = 7;
    }
    if (depth >= kRelicEventDeepDepth) {
        pct += kRelicEventDeepBonusPct;
    }
    return pct;
}

// --- The seeded grant -------------------------------------------------------
// A salt of its own, so a relic pick can never shadow a black-market roll for the
// same seed.
inline constexpr std::uint64_t kRelicPickSalt = 0x5E11C0ull;

// `owned[i]` is whether the party already holds kRelics[i] (inventory count >= 1).
// Returns an index into kRelics. Pure: the same (seed, room, ownership) always
// yields the same relic, so a reload reproduces the grant exactly.
inline int relicPickIndex(std::uint64_t dungeonSeed, int roomIndex,
                          const std::array<bool, kRelicCount>& owned) {
    int total = 0;
    for (int i = 0; i < kRelicCount; ++i) {
        if (!owned[static_cast<std::size_t>(i)]) {
            total += kRelics[static_cast<std::size_t>(i)].weight;
        }
    }
    const bool ownsAll = total == 0;  // every relic held: roll the base table anyway
    if (ownsAll) {
        for (const RelicEntry& r : kRelics) {
            total += r.weight;
        }
    }
    const std::uint64_t roll =
        blackMarketHash(dungeonSeed, kRelicPickSalt + static_cast<std::uint64_t>(roomIndex));
    int pick = static_cast<int>(roll % static_cast<std::uint64_t>(total));
    for (int i = 0; i < kRelicCount; ++i) {
        if (!ownsAll && owned[static_cast<std::size_t>(i)]) {
            continue;  // excluded; the remaining weights carry its share proportionally
        }
        pick -= kRelics[static_cast<std::size_t>(i)].weight;
        if (pick < 0) {
            return i;
        }
    }
    return kRelicCount - 1;  // unreachable: the weights are positive and sum to `total`
}

inline const char* relicIdAt(int index) {
    const int i = index < 0 ? 0 : (index >= kRelicCount ? kRelicCount - 1 : index);
    return kRelics[static_cast<std::size_t>(i)].id;
}

}  // namespace cd
