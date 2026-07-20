#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "content/Enums.hpp"

// Pure, raylib-free helpers for the Equip Shop's buy list (M31). Splitting the
// single "Buy Gear" list into Weapons / Armor / Accessories is just a slot
// filter over the equippable roster; keeping the filter here — not inside the
// raylib-linked EquipShopState — makes it unit-testable headlessly and keeps the
// state and the equip flow using one definition of "equippable".

namespace cd {

// A shop item can be bought and equipped when it is Equipment or a Relic.
// Relics carry slot == Accessory, so the slot filter below files them under
// Accessories with no special case.
inline bool isEquippableItem(const content::ItemDef& item) {
    return item.type == content::ItemType::Equipment ||
           item.type == content::ItemType::Relic;
}

// Ids of every equippable item in `slot`, sorted for a stable menu order. The
// three category calls (Weapon/Armor/Accessory) partition the equippable roster
// exactly — no item is dropped or listed twice.
inline std::vector<std::string> equipShopBuyIds(const content::ContentDatabase& content,
                                                content::EquipSlot slot) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : content.items()) {
        if (isEquippableItem(def) && def.slot == slot) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

}  // namespace cd
