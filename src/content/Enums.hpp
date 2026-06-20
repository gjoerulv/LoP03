#pragma once

#include <optional>
#include <string_view>

// Content vocabulary enums and their string<->enum mappings. Raylib-free and
// JSON-free so they can be reused by simulation and unit-tested headlessly.

namespace cd::content {

enum class Element { None, Fire, Ice, Lightning, Earth, Holy, Dark };

enum class SkillCategory { Physical, Magic, Heal, Support };

enum class SkillTarget { SingleEnemy, AllEnemies, SingleAlly, AllAllies, Self };

enum class EnemyTag { Fast, Magic, Armored, Poison };

enum class EnemyTier { Normal, Elite };

enum class ItemType { Consumable, Equipment, Relic, Scroll };

enum class EquipSlot { None, Weapon, Armor, Accessory };

enum class Rarity { Common, Uncommon, Rare, Epic, Legendary };

enum class ConsumableEffect { None, Heal, Revive, RestoreMp, Cure };

enum class StatusType { None, Poison, AttackUp, AttackDown, DefenseUp, DefenseDown };

enum class BossArchetype { Brute, Sorcerer, Commander, Rush };

// parse* return std::nullopt for unrecognized strings (the caller reports the
// error with context). toString is the inverse and always returns a stable id.
std::optional<Element> parseElement(std::string_view s);
std::optional<SkillCategory> parseSkillCategory(std::string_view s);
std::optional<SkillTarget> parseSkillTarget(std::string_view s);
std::optional<EnemyTag> parseEnemyTag(std::string_view s);
std::optional<EnemyTier> parseEnemyTier(std::string_view s);
std::optional<ItemType> parseItemType(std::string_view s);
std::optional<EquipSlot> parseEquipSlot(std::string_view s);
std::optional<Rarity> parseRarity(std::string_view s);
std::optional<ConsumableEffect> parseConsumableEffect(std::string_view s);
std::optional<StatusType> parseStatusType(std::string_view s);
std::optional<BossArchetype> parseBossArchetype(std::string_view s);

const char* toString(Element v);
const char* toString(SkillCategory v);
const char* toString(SkillTarget v);
const char* toString(EnemyTag v);
const char* toString(EnemyTier v);
const char* toString(ItemType v);
const char* toString(EquipSlot v);
const char* toString(Rarity v);
const char* toString(ConsumableEffect v);
const char* toString(StatusType v);
const char* toString(BossArchetype v);

}  // namespace cd::content
