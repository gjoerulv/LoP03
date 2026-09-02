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
// Salt pairs — one per event, appearance and slot separate (the M35 salt
// discipline). All predate v23 and are UNCHANGED by it, so which seeds fire
// which encounters is stable across the retune; only the shared chance and
// the contention rule moved.
constexpr std::uint64_t kSaltDuckAppears = 0xD0CC1157E11E2500ull;      // M76
constexpr std::uint64_t kSaltDuckSlot = 0xD0CC1157E11E2501ull;
constexpr std::uint64_t kSaltDragonformAppears = 0xD12A60F0124D9300ull;  // M93
constexpr std::uint64_t kSaltDragonformSlot = 0xD12A60F0124D9301ull;
constexpr std::uint64_t kSaltSurveyorAppears = 0x50124E70212AB500ull;    // M93
constexpr std::uint64_t kSaltSurveyorSlot = 0x50124E70212AB501ull;
}  // namespace

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
// The remaining salt pairs (M103 events, M104 dens, the v22 rite), plus the
// v23 contention-shuffle salt. Values unchanged from their introductions.
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
constexpr std::uint64_t kSaltReelsAppears = 0x2EE150DA7A000700ull;
constexpr std::uint64_t kSaltReelsSlot = 0x2EE150DA7A000701ull;
constexpr std::uint64_t kSaltBlackjackAppears = 0xB1AC7AC4A2D50800ull;
constexpr std::uint64_t kSaltBlackjackSlot = 0xB1AC7AC4A2D50801ull;
constexpr std::uint64_t kSaltRiteAppears = 0x217E5E77E0090900ull;
constexpr std::uint64_t kSaltRiteSlot = 0x217E5E77E0090901ull;
constexpr std::uint64_t kSaltContend = 0xC047E11DED0900AAull;  // v23 shuffle

// The registry rows, in the historical draw order (a stable identity order —
// contention shuffles, so it carries no priority).
constexpr std::array<EncounterDef, 10> kEncounterRegistry{{
    {RoomEventKind::DuckPeddler, kSaltDuckAppears, kSaltDuckSlot},
    {RoomEventKind::Dragonform, kSaltDragonformAppears, kSaltDragonformSlot},
    {RoomEventKind::GoosePolymorph, kSaltGoosePolyAppears, kSaltGoosePolySlot},
    {RoomEventKind::Sacrifice, kSaltSacrificeAppears, kSaltSacrificeSlot},
    {RoomEventKind::LevelAltar, kSaltLevelAltarAppears, kSaltLevelAltarSlot},
    {RoomEventKind::StrangerStory, kSaltStoryAppears, kSaltStorySlot},
    {RoomEventKind::TokenExchange, kSaltExchangeAppears, kSaltExchangeSlot},
    {RoomEventKind::PatrolReset, kSaltPatrolAppears, kSaltPatrolSlot},
    {RoomEventKind::Reels, kSaltReelsAppears, kSaltReelsSlot},
    {RoomEventKind::Blackjack, kSaltBlackjackAppears, kSaltBlackjackSlot},
}};
}  // namespace

const std::array<EncounterDef, 10>& encounterRegistry() { return kEncounterRegistry; }

EncounterDef themeRiteEncounter(const std::string& themeId) {
    EncounterDef e;
    e.kind = themeEventKind(themeId);  // None for unknown/empty: fires nothing
    e.saltAppears = kSaltRiteAppears;
    e.saltSlot = kSaltRiteSlot;
    return e;
}

bool encounterFires(std::uint64_t seed, const EncounterDef& e) {
    if (e.kind == RoomEventKind::None) {
        return false;
    }
    return static_cast<int>(themeEventHash(seed, 0, e.saltAppears) % 100) < kEncounterChancePct;
}

int encounterPick(std::uint64_t seed, const EncounterDef& e, int eligibleCount) {
    if (eligibleCount <= 0) {
        return -1;
    }
    const std::uint64_t pick = themeEventHash(seed, 1, e.saltSlot);
    return static_cast<int>(pick % static_cast<std::uint64_t>(eligibleCount));
}

void encounterContentionShuffle(std::uint64_t seed, std::vector<EncounterDef>& fired) {
    // Fisher-Yates driven by the pure hash (index folds into the room-index
    // channel): deterministic per seed, uniform over orders, no rng draw.
    for (int i = static_cast<int>(fired.size()) - 1; i > 0; --i) {
        const int j = static_cast<int>(themeEventHash(seed, i, kSaltContend) %
                                       static_cast<std::uint64_t>(i + 1));
        std::swap(fired[static_cast<std::size_t>(i)], fired[static_cast<std::size_t>(j)]);
    }
}

}  // namespace cd::dungeon
