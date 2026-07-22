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
// M44 adds two TURN-CONTROL statuses applied by the Royal Relics: Terrified
// (the bearer is forced to Guard on its next turn) and Stunned (it does nothing
// on its next turn). Unlike the others these take the turn itself, so their
// authored duration is applied exactly (never scaled) — see addStatus.
enum class StatusType {
    None,
    Poison,
    AttackUp,
    AttackDown,
    DefenseUp,
    DefenseDown,
    Confusion,
    Silence,
    Blind,
    Terrified,
    Stunned
};

// Which side a battle item is aimed at (M44). Ally is every pre-M44 item.
enum class BattleTarget { Ally, Enemy };

enum class BossArchetype { Brute, Sorcerer, Commander, Rush };

// Passive-skill behaviour (M36). Keyed by id in data; the sim implements each
// hook and reads a single `magnitude` parameter. None is the inert error default
// (never a valid data value).
enum class PassiveHook {
    None,
    Counter,      // retaliate with a basic attack after surviving a physical hit
    Evasion,      // % chance a physical attack misses you (Blind attacker always misses)
    SpellWard,    // % chance hostile magic fizzles on you
    Thorns,       // physical attacker takes magnitude% of its dealt damage back
    Lifedrink,    // heal magnitude% of the physical damage you deal
    Clarity,      // +magnitude MP each round; immune to Silence
    IronWill,     // survive a lethal blow at 1 HP once per battle
    FirstStrike,  // act first in round 1; +magnitude% damage on your first damaging action
    Bodyguard,    // redirect magnitude% of damage aimed at the lowest-HP ally to yourself
    KeenSenses    // immune to Blind; +magnitude% damage vs a debuffed target
};

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
std::optional<BattleTarget> parseBattleTarget(std::string_view s);
std::optional<BossArchetype> parseBossArchetype(std::string_view s);
std::optional<PassiveHook> parsePassiveHook(std::string_view s);

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
const char* toString(BattleTarget v);
const char* toString(BossArchetype v);
const char* toString(PassiveHook v);

}  // namespace cd::content
