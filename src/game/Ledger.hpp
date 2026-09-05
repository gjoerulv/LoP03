#pragma once

#include <algorithm>

#include "game/Lifetime.hpp"
#include "game/Party.hpp"

// M109: the economy's recording seam. Every gold or legendary-token movement
// that happens in PLAY routes through these helpers so the lifetime ledger
// counts it exactly once, at the site that decided it. Debug cheats, capture
// scenes and tests write `party.gold` directly on purpose - they are not play
// - which is why the helpers are called explicitly per site and never hidden
// inside a shared primitive. Gross flow, not net: a blackjack win routes the
// whole payout through earnGold exactly as the table pays it.

namespace cd {

// Where the money came from or went (self-documenting call sites; the Inn is
// the one source the ledger also tallies on its own).
enum class EconomySource {
    BattleSpoils,
    Chest,
    Shrine,
    Merchant,
    ElderRoot,
    MinersCache,
    DuckPeddler,
    Surveyor,
    Reels,
    Blackjack,
    TokenExchange,
    EliteChallenge,
    BossDrop,
    TreasureDig,
    CastleReward,
    Inn,
    Training,
    Passive,
    ItemShop,
    EquipShop,
    BlackMarket,
    Patrol,
    Other,
};

// `town` attributes the flow to a town's own ledger (1..kTownCount); 0 = no
// town (the castle, the pond, a guild hall).
inline void earnGold(Party& party, int amount, EconomySource source, int town) {
    (void)source;
    if (amount <= 0) {
        return;
    }
    party.gold += amount;
    EconomyLifetime& e = party.lifetime.economy;
    e.goldEarned += amount;
    e.largestPurse = std::max<LifetimeCount>(e.largestPurse, party.gold);
    if (town > 0) {
        lifetimeTown(party.lifetime, town).goldEarned += amount;
    }
}

inline void spendGold(Party& party, int amount, EconomySource source, int town) {
    if (amount <= 0) {
        return;
    }
    party.gold = std::max(0, party.gold - amount);
    EconomyLifetime& e = party.lifetime.economy;
    e.goldSpent += amount;
    if (source == EconomySource::Inn) {
        e.innGold += amount;
    }
    if (town > 0) {
        lifetimeTown(party.lifetime, town).goldSpent += amount;
    }
}

// The dungeon defeat's halving: taken, never "spent".
inline void loseGold(Party& party, int amount) {
    if (amount <= 0) {
        return;
    }
    party.gold = std::max(0, party.gold - amount);
    party.lifetime.economy.goldLost += amount;
}

inline void earnTokens(Party& party, int amount, EconomySource source) {
    (void)source;
    if (amount <= 0) {
        return;
    }
    party.legendaryTokens += amount;
    party.lifetime.economy.tokensEarned += amount;
}

inline void spendTokens(Party& party, int amount, EconomySource source) {
    (void)source;
    if (amount <= 0) {
        return;
    }
    party.legendaryTokens = std::max(0, party.legendaryTokens - amount);
    party.lifetime.economy.tokensSpent += amount;
}

inline void recordItemBought(Party& party) { ++party.lifetime.economy.itemsBought; }
inline void recordEquipmentBought(Party& party) { ++party.lifetime.economy.equipmentBought; }
inline void recordPassiveBought(Party& party) { ++party.lifetime.economy.passivesBought; }
inline void recordTreasureFound(Party& party) { ++party.lifetime.economy.treasureFound; }
inline void recordCurioFound(Party& party) { ++party.lifetime.economy.curiosFound; }
inline void recordMapTreasureCompleted(Party& party) { ++party.lifetime.economy.mapTreasures; }

// A scroll actually taught to the member in `slot` (the M64 learn), never a
// scroll merely found or bought.
inline void recordScrollLearned(Party& party, int slot) {
    ++party.lifetime.economy.scrollsLearned;
    if (validLifetimeSlot(slot)) {
        ++party.lifetime.members[static_cast<std::size_t>(slot)].scrollsLearned;
    }
}

// Levels gained by a member from ANY source (battle XP, tuition, the altar).
inline void recordLevelUps(Party& party, int levelsGained) {
    if (levelsGained > 0) {
        party.lifetime.economy.levelUps += levelsGained;
    }
}

}  // namespace cd
