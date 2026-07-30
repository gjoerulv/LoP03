#include "dungeon/ThemeEvents.hpp"

#include <algorithm>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"

namespace cd::dungeon {

RoomEventKind themeEventKind(const std::string& themeId) {
    if (themeId == "ruined_keep") return RoomEventKind::ArmoryGhost;
    if (themeId == "crystal_mine") return RoomEventKind::MinersCache;
    if (themeId == "hollow_forest") return RoomEventKind::ElderRoot;
    return RoomEventKind::None;
}

content::Rarity nextRarityUp(content::Rarity r) {
    switch (r) {
        case content::Rarity::Common: return content::Rarity::Uncommon;
        case content::Rarity::Uncommon: return content::Rarity::Rare;
        case content::Rarity::Rare: return content::Rarity::Epic;
        case content::Rarity::Epic: return content::Rarity::Legendary;
        case content::Rarity::Legendary: return content::Rarity::Legendary;  // ceiling
    }
    return r;
}

namespace {
// The equippable rule, inlined so this dungeon module needs no states/ header:
// an item is tradeable/equippable when it is Equipment or a Relic (relics carry a
// slot). Matches states/EquipShopFilter.hpp::isEquippableItem.
bool equippable(const content::ItemDef& item) {
    return item.type == content::ItemType::Equipment ||
           item.type == content::ItemType::Relic;
}
}  // namespace

std::string armoryGhostUpgrade(const content::ContentDatabase& db,
                               const std::string& tradedId, std::uint64_t hash) {
    const content::ItemDef* traded = db.findItem(tradedId);
    if (traded == nullptr || !equippable(*traded) ||
        traded->slot == content::EquipSlot::None) {
        return {};  // not a valid trade-in
    }
    if (traded->rarity == content::Rarity::Legendary) {
        return {};  // the ghost declines a legendary
    }
    const content::Rarity target = nextRarityUp(traded->rarity);

    // Every equippable item of the same slot and the target rarity, sorted for a
    // deterministic pick.
    std::vector<std::string> pool;
    for (const auto& [id, def] : db.items()) {
        if (equippable(def) && def.slot == traded->slot && def.rarity == target) {
            pool.push_back(id);
        }
    }
    if (pool.empty()) {
        return {};  // nothing of the next tier in this slot
    }
    std::sort(pool.begin(), pool.end());
    const std::size_t index = static_cast<std::size_t>(hash % pool.size());
    return pool[index];
}

std::uint64_t themeEventHash(std::uint64_t seed, int roomIndex, std::uint64_t salt) {
    // SplitMix64 finalizer over (seed, roomIndex, salt) — the same shape as the
    // black-market/relic hashes: pure, no evolving RNG state to keep in sync.
    std::uint64_t x = seed ^ (0x9E3779B97F4A7C15ull * (static_cast<std::uint64_t>(roomIndex) + 1)) ^
                      (0xD1B54A32D192ED03ull * (salt + 1));
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return x;
}

}  // namespace cd::dungeon
