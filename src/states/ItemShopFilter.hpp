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

// M88: the shelf reads by purpose, not by id. HP restoratives first (Potion on
// top), then MP, then cures, then revives, then everything else — and inside a
// category the cheap entry leads, so each group reads weakest-to-strongest.
// An item whose real job is a special rider (Royal Snacks' debuff shrug and
// King feast, Holy Taxes' uncurse) is an oddity no matter what token `effect`
// it carries — its 10 HP is a joke, not a shelf category.
inline int itemShopCategoryRank(const content::ItemDef& def) {
    if (def.curesDebuffs || def.curesCurse || def.kingEffectAmount > 0) {
        return 4;
    }
    switch (def.effect) {
        case content::ConsumableEffect::Heal: return 0;
        case content::ConsumableEffect::RestoreMp: return 1;
        case content::ConsumableEffect::Cure: return 2;
        case content::ConsumableEffect::Revive: return 3;
        case content::ConsumableEffect::None: break;
    }
    return 4;  // oddities close the list
}

// Ids of every consumable the item shop stocks at `town`, in category order
// (M88; was alphabetical by id). The town window is ItemDef::availableAtTown —
// the same question the equip shop and the generator's pools ask — so an
// item's availability has one answer everywhere.
inline std::vector<std::string> itemShopBuyIds(const content::ContentDatabase& content, int town) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : content.items()) {
        // M44: an item with no gold value has no price and is never stocked — it
        // exists only through whatever grants it (the Royal Relics).
        // M78: a premium tonic (`notSoldInTown` — Elixir, Hi-Ether) left the
        // town shelves for good; the in-dungeon merchant is its only seller.
        if (def.type == content::ItemType::Consumable && def.value > 0 &&
            def.availableAtTown(town) && !def.notSoldInTown) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end(), [&content](const std::string& a, const std::string& b) {
        const content::ItemDef* da = content.findItem(a);
        const content::ItemDef* db = content.findItem(b);
        const int ra = da != nullptr ? itemShopCategoryRank(*da) : 4;
        const int rb = db != nullptr ? itemShopCategoryRank(*db) : 4;
        if (ra != rb) {
            return ra < rb;
        }
        const int va = da != nullptr ? da->value : 0;
        const int vb = db != nullptr ? db->value : 0;
        if (va != vb) {
            return va < vb;
        }
        return a < b;  // stable, deterministic tie-break
    });
    return ids;
}

}  // namespace cd
