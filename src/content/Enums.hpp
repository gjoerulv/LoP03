#pragma once

#include <optional>
#include <string_view>

// Content vocabulary enums and their string<->enum mappings. Raylib-free and
// JSON-free so they can be reused by simulation and unit-tested headlessly.

namespace cd::content {

enum class Element { None, Fire, Ice, Lightning, Earth, Holy, Dark };

enum class SkillCategory { Physical, Magic, Heal, Support };

// Enmity-control / utility effect a skill carries, independent of its category.
// None = ordinary skill. Taunt spikes the caster's threat, Fade sheds it,
// Intercept makes the caster take hits aimed at allies until its next turn (M28).
// Cleanse strips every negative status from the skill's ally targets (M35), so a
// heal skill can double as a cure.
enum class SkillEffect { None, Taunt, Fade, Intercept, Cleanse };

enum class SkillTarget { SingleEnemy, AllEnemies, SingleAlly, AllAllies, Self };

enum class EnemyTag { Fast, Magic, Armored, Poison };

enum class EnemyTier { Normal, Elite };

// Tactical role for composition constraints (M20): what an enemy contributes
// to a team, distinct from tier and tags. Healer/Buffer count as "support"
// (capped per team); Bruiser/Sniper count as "damage" (required per team).
enum class EnemyRole { Bruiser, Sniper, Healer, Buffer, Protector, Attrition, Disruptor };

enum class ItemType { Consumable, Equipment, Relic, Scroll };

enum class EquipSlot { None, Weapon, Armor, Accessory };

enum class Rarity { Common, Uncommon, Rare, Epic, Legendary };

enum class ConsumableEffect { None, Heal, Revive, RestoreMp, Cure };

// M35 adds Confusion (basic-attacks its own side), Silence (no MP-cost skills),
// and Blind (physical attacks usually miss). All three are duration-only
// (no magnitude) and are stripped by Cure like the other negative statuses.
enum class StatusType {
    None,
    Poison,
    AttackUp,
    AttackDown,
    DefenseUp,
    DefenseDown,
    Confusion,
    Silence,
    Blind
};

enum class BossArchetype { Brute, Sorcerer, Commander, Rush };

// parse* return std::nullopt for unrecognized strings (the caller reports the
// error with context). toString is the inverse and always returns a stable id.
std::optional<Element> parseElement(std::string_view s);
std::optional<SkillCategory> parseSkillCategory(std::string_view s);
std::optional<SkillEffect> parseSkillEffect(std::string_view s);
std::optional<SkillTarget> parseSkillTarget(std::string_view s);
std::optional<EnemyTag> parseEnemyTag(std::string_view s);
std::optional<EnemyTier> parseEnemyTier(std::string_view s);
std::optional<EnemyRole> parseEnemyRole(std::string_view s);
std::optional<ItemType> parseItemType(std::string_view s);
std::optional<EquipSlot> parseEquipSlot(std::string_view s);
std::optional<Rarity> parseRarity(std::string_view s);
std::optional<ConsumableEffect> parseConsumableEffect(std::string_view s);
std::optional<StatusType> parseStatusType(std::string_view s);
std::optional<BossArchetype> parseBossArchetype(std::string_view s);

const char* toString(Element v);
const char* toString(SkillCategory v);
const char* toString(SkillEffect v);
const char* toString(SkillTarget v);
const char* toString(EnemyTag v);
const char* toString(EnemyTier v);
const char* toString(EnemyRole v);

// Role groupings used by the composition constraints.
inline bool isSupportRole(EnemyRole r) {
    return r == EnemyRole::Healer || r == EnemyRole::Buffer;
}
inline bool isDamageRole(EnemyRole r) {
    return r == EnemyRole::Bruiser || r == EnemyRole::Sniper;
}
const char* toString(ItemType v);
const char* toString(EquipSlot v);
const char* toString(Rarity v);
const char* toString(ConsumableEffect v);
const char* toString(StatusType v);
const char* toString(BossArchetype v);

}  // namespace cd::content
