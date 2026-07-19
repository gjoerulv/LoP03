#include "content/Enums.hpp"

#include <array>
#include <utility>

namespace cd::content {

namespace {

template <typename E, std::size_t N>
std::optional<E> parseFrom(const std::array<std::pair<std::string_view, E>, N>& table,
                           std::string_view s) {
    for (const auto& [key, value] : table) {
        if (key == s) {
            return value;
        }
    }
    return std::nullopt;
}

template <typename E, std::size_t N>
const char* nameFrom(const std::array<std::pair<std::string_view, E>, N>& table, E value) {
    for (const auto& [key, candidate] : table) {
        if (candidate == value) {
            return key.data();
        }
    }
    return "?";
}

constexpr std::array<std::pair<std::string_view, Element>, 7> kElements{{
    {"none", Element::None},
    {"fire", Element::Fire},
    {"ice", Element::Ice},
    {"lightning", Element::Lightning},
    {"earth", Element::Earth},
    {"holy", Element::Holy},
    {"dark", Element::Dark},
}};

constexpr std::array<std::pair<std::string_view, SkillCategory>, 4> kSkillCategories{{
    {"physical", SkillCategory::Physical},
    {"magic", SkillCategory::Magic},
    {"heal", SkillCategory::Heal},
    {"support", SkillCategory::Support},
}};

constexpr std::array<std::pair<std::string_view, SkillTarget>, 5> kSkillTargets{{
    {"single_enemy", SkillTarget::SingleEnemy},
    {"all_enemies", SkillTarget::AllEnemies},
    {"single_ally", SkillTarget::SingleAlly},
    {"all_allies", SkillTarget::AllAllies},
    {"self", SkillTarget::Self},
}};

constexpr std::array<std::pair<std::string_view, EnemyTag>, 4> kEnemyTags{{
    {"fast", EnemyTag::Fast},
    {"magic", EnemyTag::Magic},
    {"armored", EnemyTag::Armored},
    {"poison", EnemyTag::Poison},
}};

constexpr std::array<std::pair<std::string_view, EnemyTier>, 2> kEnemyTiers{{
    {"normal", EnemyTier::Normal},
    {"elite", EnemyTier::Elite},
}};

constexpr std::array<std::pair<std::string_view, EnemyRole>, 7> kEnemyRoles{{
    {"bruiser", EnemyRole::Bruiser},
    {"sniper", EnemyRole::Sniper},
    {"healer", EnemyRole::Healer},
    {"buffer", EnemyRole::Buffer},
    {"protector", EnemyRole::Protector},
    {"attrition", EnemyRole::Attrition},
    {"disruptor", EnemyRole::Disruptor},
}};

constexpr std::array<std::pair<std::string_view, ItemType>, 4> kItemTypes{{
    {"consumable", ItemType::Consumable},
    {"equipment", ItemType::Equipment},
    {"relic", ItemType::Relic},
    {"scroll", ItemType::Scroll},
}};

constexpr std::array<std::pair<std::string_view, EquipSlot>, 4> kEquipSlots{{
    {"none", EquipSlot::None},
    {"weapon", EquipSlot::Weapon},
    {"armor", EquipSlot::Armor},
    {"accessory", EquipSlot::Accessory},
}};

constexpr std::array<std::pair<std::string_view, Rarity>, 5> kRarities{{
    {"common", Rarity::Common},
    {"uncommon", Rarity::Uncommon},
    {"rare", Rarity::Rare},
    {"epic", Rarity::Epic},
    {"legendary", Rarity::Legendary},
}};

constexpr std::array<std::pair<std::string_view, ConsumableEffect>, 5> kConsumableEffects{{
    {"none", ConsumableEffect::None},
    {"heal", ConsumableEffect::Heal},
    {"revive", ConsumableEffect::Revive},
    {"restore_mp", ConsumableEffect::RestoreMp},
    {"cure", ConsumableEffect::Cure},
}};

constexpr std::array<std::pair<std::string_view, StatusType>, 6> kStatusTypes{{
    {"none", StatusType::None},
    {"poison", StatusType::Poison},
    {"attack_up", StatusType::AttackUp},
    {"attack_down", StatusType::AttackDown},
    {"defense_up", StatusType::DefenseUp},
    {"defense_down", StatusType::DefenseDown},
}};

constexpr std::array<std::pair<std::string_view, BossArchetype>, 4> kBossArchetypes{{
    {"brute", BossArchetype::Brute},
    {"sorcerer", BossArchetype::Sorcerer},
    {"commander", BossArchetype::Commander},
    {"rush", BossArchetype::Rush},
}};

}  // namespace

std::optional<Element> parseElement(std::string_view s) { return parseFrom(kElements, s); }
std::optional<SkillCategory> parseSkillCategory(std::string_view s) {
    return parseFrom(kSkillCategories, s);
}
std::optional<SkillTarget> parseSkillTarget(std::string_view s) {
    return parseFrom(kSkillTargets, s);
}
std::optional<EnemyTag> parseEnemyTag(std::string_view s) { return parseFrom(kEnemyTags, s); }
std::optional<EnemyTier> parseEnemyTier(std::string_view s) { return parseFrom(kEnemyTiers, s); }
std::optional<EnemyRole> parseEnemyRole(std::string_view s) { return parseFrom(kEnemyRoles, s); }
std::optional<ItemType> parseItemType(std::string_view s) { return parseFrom(kItemTypes, s); }
std::optional<EquipSlot> parseEquipSlot(std::string_view s) { return parseFrom(kEquipSlots, s); }
std::optional<Rarity> parseRarity(std::string_view s) { return parseFrom(kRarities, s); }
std::optional<ConsumableEffect> parseConsumableEffect(std::string_view s) {
    return parseFrom(kConsumableEffects, s);
}
std::optional<StatusType> parseStatusType(std::string_view s) { return parseFrom(kStatusTypes, s); }
std::optional<BossArchetype> parseBossArchetype(std::string_view s) {
    return parseFrom(kBossArchetypes, s);
}

const char* toString(Element v) { return nameFrom(kElements, v); }
const char* toString(SkillCategory v) { return nameFrom(kSkillCategories, v); }
const char* toString(SkillTarget v) { return nameFrom(kSkillTargets, v); }
const char* toString(EnemyTag v) { return nameFrom(kEnemyTags, v); }
const char* toString(EnemyTier v) { return nameFrom(kEnemyTiers, v); }
const char* toString(EnemyRole v) { return nameFrom(kEnemyRoles, v); }
const char* toString(ItemType v) { return nameFrom(kItemTypes, v); }
const char* toString(EquipSlot v) { return nameFrom(kEquipSlots, v); }
const char* toString(Rarity v) { return nameFrom(kRarities, v); }
const char* toString(ConsumableEffect v) { return nameFrom(kConsumableEffects, v); }
const char* toString(StatusType v) { return nameFrom(kStatusTypes, v); }
const char* toString(BossArchetype v) { return nameFrom(kBossArchetypes, v); }

}  // namespace cd::content
