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
    if (themeId == "goosy_gauntlet") return RoomEventKind::GoosyFlock;  // M106
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

namespace {
// M76: the peddler's own salt — one for the appearance roll, one for the slot
// pick, so the two draws stay independent (the M35 salt discipline).
constexpr std::uint64_t kSaltDuckAppears = 0xD0CC1157E11E2500ull;
constexpr std::uint64_t kSaltDuckSlot = 0xD0CC1157E11E2501ull;
}  // namespace

int duckPeddlerSlot(std::uint64_t seed, int eligibleCount) {
    if (eligibleCount <= 0) {
        return -1;
    }
    const std::uint64_t roll = themeEventHash(seed, 0, kSaltDuckAppears);
    if (static_cast<int>(roll % 100) >= kDuckPeddlerChancePct) {
        return -1;  // the common case: no peddler in this dungeon
    }
    const std::uint64_t pick = themeEventHash(seed, 1, kSaltDuckSlot);
    return static_cast<int>(pick % static_cast<std::uint64_t>(eligibleCount));
}

namespace {
// M93: fresh salts per event, same discipline (appearance and slot separate).
constexpr std::uint64_t kSaltDragonformAppears = 0xD12A60F0124D9300ull;
constexpr std::uint64_t kSaltDragonformSlot = 0xD12A60F0124D9301ull;
constexpr std::uint64_t kSaltSurveyorAppears = 0x50124E70212AB500ull;
constexpr std::uint64_t kSaltSurveyorSlot = 0x50124E70212AB501ull;
}  // namespace

int dragonformSlot(std::uint64_t seed, int eligibleCount) {
    if (eligibleCount <= 0) {
        return -1;
    }
    const std::uint64_t roll = themeEventHash(seed, 0, kSaltDragonformAppears);
    if (static_cast<int>(roll % 100) >= kDragonformChancePct) {
        return -1;
    }
    const std::uint64_t pick = themeEventHash(seed, 1, kSaltDragonformSlot);
    return static_cast<int>(pick % static_cast<std::uint64_t>(eligibleCount));
}

int surveyorSlot(std::uint64_t seed, int floorIndex, int eligibleCount) {
    if (eligibleCount <= 0) {
        return -1;
    }
    // Per-floor: the floor index folds into the room-index channel so every
    // floor of a run rolls its own independent chance from the RUN seed.
    const std::uint64_t roll = themeEventHash(seed, floorIndex, kSaltSurveyorAppears);
    if (static_cast<int>(roll % 100) >= kSurveyorChancePct) {
        return -1;
    }
    const std::uint64_t pick = themeEventHash(seed, floorIndex + 1000, kSaltSurveyorSlot);
    return static_cast<int>(pick % static_cast<std::uint64_t>(eligibleCount));
}

namespace {
// M103: fresh salt pairs per event (appearance and slot separate — the M35
// salt discipline), and one shared per-dungeon shape behind the six names.
constexpr std::uint64_t kSaltGoosePolyAppears = 0x6005EF0270CA0900ull;
constexpr std::uint64_t kSaltGoosePolySlot = 0x6005EF0270CA0901ull;
constexpr std::uint64_t kSaltSacrificeAppears = 0x5AC21F1CE0FFE200ull;
constexpr std::uint64_t kSaltSacrificeSlot = 0x5AC21F1CE0FFE201ull;
constexpr std::uint64_t kSaltLevelAltarAppears = 0x1E7E1A17A2000300ull;
constexpr std::uint64_t kSaltLevelAltarSlot = 0x1E7E1A17A2000301ull;
constexpr std::uint64_t kSaltStoryAppears = 0x57012A9E20050400ull;
constexpr std::uint64_t kSaltStorySlot = 0x57012A9E20050401ull;
constexpr std::uint64_t kSaltExchangeAppears = 0xE8C4A26E70000500ull;
constexpr std::uint64_t kSaltExchangeSlot = 0xE8C4A26E70000501ull;
constexpr std::uint64_t kSaltPatrolAppears = 0x9A7201F0E5E70600ull;
constexpr std::uint64_t kSaltPatrolSlot = 0x9A7201F0E5E70601ull;

int perDungeonSlot(std::uint64_t seed, int eligibleCount, int chancePct,
                   std::uint64_t saltAppears, std::uint64_t saltSlot) {
    if (eligibleCount <= 0) {
        return -1;
    }
    const std::uint64_t roll = themeEventHash(seed, 0, saltAppears);
    if (static_cast<int>(roll % 100) >= chancePct) {
        return -1;
    }
    const std::uint64_t pick = themeEventHash(seed, 1, saltSlot);
    return static_cast<int>(pick % static_cast<std::uint64_t>(eligibleCount));
}
}  // namespace

int goosePolymorphSlot(std::uint64_t seed, int eligibleCount) {
    return perDungeonSlot(seed, eligibleCount, kGoosePolymorphChancePct, kSaltGoosePolyAppears,
                          kSaltGoosePolySlot);
}
int sacrificeSlot(std::uint64_t seed, int eligibleCount) {
    return perDungeonSlot(seed, eligibleCount, kSacrificeChancePct, kSaltSacrificeAppears,
                          kSaltSacrificeSlot);
}
int levelAltarSlot(std::uint64_t seed, int eligibleCount) {
    return perDungeonSlot(seed, eligibleCount, kLevelAltarChancePct, kSaltLevelAltarAppears,
                          kSaltLevelAltarSlot);
}
int strangerStorySlot(std::uint64_t seed, int eligibleCount) {
    return perDungeonSlot(seed, eligibleCount, kStrangerStoryChancePct, kSaltStoryAppears,
                          kSaltStorySlot);
}
int tokenExchangeSlot(std::uint64_t seed, int eligibleCount) {
    return perDungeonSlot(seed, eligibleCount, kTokenExchangeChancePct, kSaltExchangeAppears,
                          kSaltExchangeSlot);
}
int patrolResetSlot(std::uint64_t seed, int eligibleCount) {
    return perDungeonSlot(seed, eligibleCount, kPatrolResetChancePct, kSaltPatrolAppears,
                          kSaltPatrolSlot);
}

namespace {
// M104: the gambling dens' salt pairs.
constexpr std::uint64_t kSaltReelsAppears = 0x2EE150DA7A000700ull;
constexpr std::uint64_t kSaltReelsSlot = 0x2EE150DA7A000701ull;
constexpr std::uint64_t kSaltBlackjackAppears = 0xB1AC7AC4A2D50800ull;
constexpr std::uint64_t kSaltBlackjackSlot = 0xB1AC7AC4A2D50801ull;
}  // namespace

int reelsSlot(std::uint64_t seed, int eligibleCount) {
    return perDungeonSlot(seed, eligibleCount, kReelsChancePct, kSaltReelsAppears,
                          kSaltReelsSlot);
}
int blackjackSlot(std::uint64_t seed, int eligibleCount) {
    return perDungeonSlot(seed, eligibleCount, kBlackjackChancePct, kSaltBlackjackAppears,
                          kSaltBlackjackSlot);
}

namespace {
// 2026-08-17 (generation v22): the leveled theme rite's salt pair.
constexpr std::uint64_t kSaltRiteAppears = 0x217E5E77E0090900ull;
constexpr std::uint64_t kSaltRiteSlot = 0x217E5E77E0090901ull;
}  // namespace

int themeRiteSlot(std::uint64_t seed, int eligibleCount) {
    return perDungeonSlot(seed, eligibleCount, kThemeRiteChancePct, kSaltRiteAppears,
                          kSaltRiteSlot);
}

}  // namespace cd::dungeon
