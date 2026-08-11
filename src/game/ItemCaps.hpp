#pragma once

#include <algorithm>

#include "content/Definitions.hpp"
#include "game/Inventory.hpp"

// M78 — held-quantity caps, as one pure header (the ItemShopFilter shape so
// every buy path and the tests ask the same question). The rules, all owner
// decisions (2026-08-05):
//   * every consumable caps at 2 held; authored exceptions via
//     `ItemDef::maxHeld` (Potion 9, Hi-Potion 6);
//   * a perk bonus (M84's town milestones) raises EVERY consumable cap by 1
//     per rank, against a hard ceiling of 9 — so Potion simply stays 9;
//   * equipment, relics and scrolls are uncapped;
//   * enforcement is `>=` at PURCHASE time only — chests, events, rewards and
//     relics still grant freely, and a party already over a cap (an older
//     save) is never clamped, merely treated as at-max.
// The Evil Duckling needs no cap row: its only source, the Duckling Peddler,
// enforces one-per-customer at the till (M76) — duck rules outrank shop rules.

namespace cd {

inline constexpr int kDefaultConsumableCap = 2;
inline constexpr int kConsumableCapCeiling = 9;

// How many of `def` the party may hold before shops refuse another sale.
// 0 = uncapped (every non-consumable).
inline int capFor(const content::ItemDef& def, int capBonus = 0) {
    if (def.type != content::ItemType::Consumable) {
        return 0;
    }
    const int base = def.maxHeld > 0 ? def.maxHeld : kDefaultConsumableCap;
    return std::min(base + std::max(0, capBonus), kConsumableCapCeiling);
}

// May a shop sell the party one more of `def`? `>=` semantics: at the cap
// (or anywhere above it) the answer is no; the overage itself is untouched.
inline bool canBuyMore(const Inventory& inventory, const content::ItemDef& def,
                       int capBonus = 0) {
    const int cap = capFor(def, capBonus);
    return cap == 0 || inventory.count(def.id) < cap;
}

// M78 (owner decision): the in-dungeon merchant's asking price. Its wares
// keep the generated M37 street discount — except a premium tonic the towns
// no longer stock, which costs exactly full value. Applied at INTERACTION
// time so the generated `goldCost` (and with it generation determinism and
// kGenerationVersion) is untouched — the M76 peddler precedent.
inline int merchantPriceFor(const content::ItemDef& def, int generatedGoldCost) {
    return def.notSoldInTown ? def.value : generatedGoldCost;
}

}  // namespace cd
