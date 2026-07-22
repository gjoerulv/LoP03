#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "content/Enums.hpp"

// Pure, raylib-free helper for the Item Shop's stock list (M43), mirroring
// EquipShopFilter for gear. Consumables had no town gating at all, so a shop in
// any town sold everything; Royal Snacks are sold in town 1 only, which needs a
// real gate rather than a special case inside the raylib-linked state. Keeping
// the filter here makes it unit-testable headlessly.

namespace cd {

// Ids of every consumable the item shop stocks at `town`, sorted for a stable
// menu order. The town window is ItemDef::availableAtTown — the same question
// the equip shop and the generator's pools ask — so an item's availability has
// one answer everywhere.
inline std::vector<std::string> itemShopBuyIds(const content::ContentDatabase& content, int town) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : content.items()) {
        // M44: an item with no gold value has no price and is never stocked — it
        // exists only through whatever grants it (the Royal Relics).
        if (def.type == content::ItemType::Consumable && def.value > 0 &&
            def.availableAtTown(town)) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

}  // namespace cd
