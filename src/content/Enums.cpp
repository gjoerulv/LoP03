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

constexpr std::array<std::pair<std::string_view, SkillEffect>, 7> kSkillEffects{{
    {"none", SkillEffect::None},
    {"taunt", SkillEffect::Taunt},
    {"fade", SkillEffect::Fade},
    {"intercept", SkillEffect::Intercept},
    {"cleanse", SkillEffect::Cleanse},
    {"break_reflect", SkillEffect::BreakReflect},  // M75: strips Reflect from foes
    {"uncurse", SkillEffect::Uncurse},             // M75: lifts Curse from allies
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

constexpr std::array<std::pair<std::string_view, ItemType>, 5> kItemTypes{{
    {"consumable", ItemType::Consumable},
    {"equipment", ItemType::Equipment},
    {"relic", ItemType::Relic},
    {"scroll", ItemType::Scroll},
    {"heirloom", ItemType::Heirloom},  // M96
}};

constexpr std::array<std::pair<std::string_view, EquipSlot>, 5> kEquipSlots{{
    {"none", EquipSlot::None},
    {"weapon", EquipSlot::Weapon},
    {"armor", EquipSlot::Armor},
    {"accessory", EquipSlot::Accessory},
    {"heirloom", EquipSlot::Heirloom},  // M96: the fourth worn slot
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

constexpr std::array<std::pair<std::string_view, StatusType>, 14> kStatusTypes{{
    {"none", StatusType::None},
    {"poison", StatusType::Poison},
    {"attack_up", StatusType::AttackUp},
    {"attack_down", StatusType::AttackDown},
    {"defense_up", StatusType::DefenseUp},
    {"defense_down", StatusType::DefenseDown},
    {"confusion", StatusType::Confusion},
    {"silence", StatusType::Silence},
    {"blind", StatusType::Blind},
    {"terrified", StatusType::Terrified},  // M44: forced to Guard next turn
    {"stunned", StatusType::Stunned},      // M44: skips its next turn
    {"reflect", StatusType::Reflect},      // M75: bounces hostile magic back
    {"sleep", StatusType::Sleep},          // M75: skips turns; damage wakes
    {"curse", StatusType::Curse},          // M75: half damage out, double MP costs
}};

constexpr std::array<std::pair<std::string_view, BattleTarget>, 2> kBattleTargets{{
    {"ally", BattleTarget::Ally},
    {"enemy", BattleTarget::Enemy},
}};

constexpr std::array<std::pair<std::string_view, BossArchetype>, 4> kBossArchetypes{{
    {"brute", BossArchetype::Brute},
    {"sorcerer", BossArchetype::Sorcerer},
    {"commander", BossArchetype::Commander},
    {"rush", BossArchetype::Rush},
}};

// The 10 valid passive hooks (M36); "none" is intentionally absent so it is
// rejected in data and only ever the inert error fallback.
constexpr std::array<std::pair<std::string_view, PassiveHook>, 10> kPassiveHooks{{
    {"counter", PassiveHook::Counter},
    {"evasion", PassiveHook::Evasion},
    {"spell_ward", PassiveHook::SpellWard},
    {"thorns", PassiveHook::Thorns},
    {"lifedrink", PassiveHook::Lifedrink},
    {"clarity", PassiveHook::Clarity},
    {"iron_will", PassiveHook::IronWill},
    {"first_strike", PassiveHook::FirstStrike},
    {"bodyguard", PassiveHook::Bodyguard},
    {"keen_senses", PassiveHook::KeenSenses},
}};

// The valid milestone effects (M63); "none" is intentionally absent so it is
// rejected in data and only ever the inert error fallback.
constexpr std::array<std::pair<std::string_view, MilestoneEffect>, 37> kMilestoneEffects{{
    {"stat_max_hp_pct", MilestoneEffect::StatMaxHpPct},
    {"stat_speed_pct", MilestoneEffect::StatSpeedPct},
    {"stat_max_mp_pct", MilestoneEffect::StatMaxMpPct},
    {"stat_defense_pct", MilestoneEffect::StatDefensePct},
    {"basic_attack_pct", MilestoneEffect::BasicAttackPct},
    {"magic_skill_pct", MilestoneEffect::MagicSkillPct},
    {"aoe_spell_pct", MilestoneEffect::AoeSpellPct},
    {"heal_cast_pct", MilestoneEffect::HealCastPct},
    {"execute_pct", MilestoneEffect::ExecutePct},
    {"vs_afflicted_pct", MilestoneEffect::VsAfflictedPct},
    {"weakness_bonus_pct", MilestoneEffect::WeaknessBonusPct},
    {"status_turns_bonus", MilestoneEffect::StatusTurnsBonus},
    {"opening_guard_pct", MilestoneEffect::OpeningGuardPct},
    {"double_strike_pct", MilestoneEffect::DoubleStrikePct},
    {"sweep_all_pct", MilestoneEffect::SweepAllPct},
    {"sweep_debuff_pct", MilestoneEffect::SweepDebuffPct},
    {"taunt_debuff_pct", MilestoneEffect::TauntDebuffPct},
    {"guard_block_pct", MilestoneEffect::GuardBlockPct},
    {"first_hit_immune", MilestoneEffect::FirstHitImmune},
    {"iron_will_healing", MilestoneEffect::IronWillHealing},
    {"revive_at_pct", MilestoneEffect::ReviveAtPct},
    {"purify_heals", MilestoneEffect::PurifyHeals},
    {"holy_basic", MilestoneEffect::HolyBasic},
    {"fire_basic", MilestoneEffect::FireBasic},
    {"gold_bonus_pct", MilestoneEffect::GoldBonusPct},
    {"item_potency_pct", MilestoneEffect::ItemPotencyPct},
    {"no_enemy_buff", MilestoneEffect::NoEnemyBuff},
    {"on_kill_party_atk_up", MilestoneEffect::OnKillPartyAtkUp},
    {"on_death_foe_debuff", MilestoneEffect::OnDeathFoeDebuff},
    {"grant_counter", MilestoneEffect::GrantCounter},
    {"grant_evasion", MilestoneEffect::GrantEvasion},
    {"grant_spell_ward", MilestoneEffect::GrantSpellWard},
    {"grant_thorns", MilestoneEffect::GrantThorns},
    {"grant_iron_will", MilestoneEffect::GrantIronWill},
    {"grant_first_strike", MilestoneEffect::GrantFirstStrike},
    {"grant_clarity", MilestoneEffect::GrantClarity},
    {"grant_bodyguard", MilestoneEffect::GrantBodyguard},
}};

// The valid trigger conditions and actions (M75); "none" is intentionally
// absent from both so it is rejected in data and only ever the inert error
// fallback, the PassiveHook precedent.
constexpr std::array<std::pair<std::string_view, TriggerWhen>, 4> kTriggerWhens{{
    {"every_nth_hit_taken", TriggerWhen::EveryNthHitTaken},
    {"first_time_hp_below_pct", TriggerWhen::FirstTimeHpBelowPct},
    {"every_nth_own_turn", TriggerWhen::EveryNthOwnTurn},
    {"first_time_ally_felled", TriggerWhen::FirstTimeAllyFelled},
}};

constexpr std::array<std::pair<std::string_view, TriggerDo>, 8> kTriggerDos{{
    {"status_self", TriggerDo::StatusSelf},
    {"status_attacker", TriggerDo::StatusAttacker},
    {"status_all_foes", TriggerDo::StatusAllFoes},
    {"status_boss", TriggerDo::StatusBoss},
    {"scale_stats_self", TriggerDo::ScaleStatsSelf},
    {"summon_clone", TriggerDo::SummonCloneSelf},
    {"drain_foe_mp", TriggerDo::DrainFoeMp},
    {"heal_self_pct", TriggerDo::HealSelfPct},  // M96: heirlooms (rules v18)
}};

constexpr std::array<std::pair<std::string_view, ScriptDo>, 3> kScriptDos{{  // M111
    {"status_all_foes", ScriptDo::StatusAllFoes},
    {"guard", ScriptDo::Guard},
    {"flee", ScriptDo::Flee},
}};

}  // namespace

std::optional<Element> parseElement(std::string_view s) { return parseFrom(kElements, s); }
std::optional<SkillCategory> parseSkillCategory(std::string_view s) {
    return parseFrom(kSkillCategories, s);
}
std::optional<SkillEffect> parseSkillEffect(std::string_view s) {
    return parseFrom(kSkillEffects, s);
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
std::optional<BattleTarget> parseBattleTarget(std::string_view s) {
    return parseFrom(kBattleTargets, s);
}
std::optional<BossArchetype> parseBossArchetype(std::string_view s) {
    return parseFrom(kBossArchetypes, s);
}
std::optional<PassiveHook> parsePassiveHook(std::string_view s) {
    return parseFrom(kPassiveHooks, s);
}
std::optional<MilestoneEffect> parseMilestoneEffect(std::string_view s) {
    return parseFrom(kMilestoneEffects, s);
}
std::optional<TriggerWhen> parseTriggerWhen(std::string_view s) {
    return parseFrom(kTriggerWhens, s);
}
std::optional<TriggerDo> parseTriggerDo(std::string_view s) { return parseFrom(kTriggerDos, s); }
std::optional<ScriptDo> parseScriptDo(std::string_view s) { return parseFrom(kScriptDos, s); }

const char* toString(Element v) { return nameFrom(kElements, v); }

const char* elementDisplayName(Element v) {
    switch (v) {
        case Element::None: return "None";
        case Element::Fire: return "Fire";
        case Element::Ice: return "Ice";
        case Element::Lightning: return "Lightning";
        case Element::Earth: return "Earth";
        case Element::Holy: return "Holy";
        case Element::Dark: return "Dark";
    }
    return "None";
}

const char* toString(SkillCategory v) { return nameFrom(kSkillCategories, v); }
const char* toString(SkillEffect v) { return nameFrom(kSkillEffects, v); }
const char* toString(SkillTarget v) { return nameFrom(kSkillTargets, v); }
const char* toString(EnemyTag v) { return nameFrom(kEnemyTags, v); }
const char* toString(EnemyTier v) { return nameFrom(kEnemyTiers, v); }
const char* toString(EnemyRole v) { return nameFrom(kEnemyRoles, v); }
const char* toString(ItemType v) { return nameFrom(kItemTypes, v); }
const char* toString(EquipSlot v) { return nameFrom(kEquipSlots, v); }
const char* toString(Rarity v) { return nameFrom(kRarities, v); }
const char* toString(ConsumableEffect v) { return nameFrom(kConsumableEffects, v); }
const char* toString(StatusType v) { return nameFrom(kStatusTypes, v); }
const char* toString(BattleTarget v) { return nameFrom(kBattleTargets, v); }
const char* toString(BossArchetype v) { return nameFrom(kBossArchetypes, v); }
const char* toString(PassiveHook v) { return nameFrom(kPassiveHooks, v); }
const char* toString(MilestoneEffect v) { return nameFrom(kMilestoneEffects, v); }
const char* toString(TriggerWhen v) { return nameFrom(kTriggerWhens, v); }
const char* toString(TriggerDo v) { return nameFrom(kTriggerDos, v); }
const char* toString(ScriptDo v) { return nameFrom(kScriptDos, v); }

namespace {

// M59: the id column of a parse table, in declaration order.
template <typename E, std::size_t N>
std::vector<std::string_view> idsFrom(const std::array<std::pair<std::string_view, E>, N>& table) {
    std::vector<std::string_view> ids;
    ids.reserve(N);
    for (const auto& [key, value] : table) {
        ids.push_back(key);
    }
    return ids;
}

}  // namespace

std::vector<std::string_view> elementIds() { return idsFrom(kElements); }
std::vector<std::string_view> skillCategoryIds() { return idsFrom(kSkillCategories); }
std::vector<std::string_view> skillEffectIds() { return idsFrom(kSkillEffects); }
std::vector<std::string_view> skillTargetIds() { return idsFrom(kSkillTargets); }
std::vector<std::string_view> enemyTagIds() { return idsFrom(kEnemyTags); }
std::vector<std::string_view> enemyTierIds() { return idsFrom(kEnemyTiers); }
std::vector<std::string_view> enemyRoleIds() { return idsFrom(kEnemyRoles); }
std::vector<std::string_view> itemTypeIds() { return idsFrom(kItemTypes); }
std::vector<std::string_view> equipSlotIds() { return idsFrom(kEquipSlots); }
std::vector<std::string_view> rarityIds() { return idsFrom(kRarities); }
std::vector<std::string_view> consumableEffectIds() { return idsFrom(kConsumableEffects); }
std::vector<std::string_view> statusTypeIds() { return idsFrom(kStatusTypes); }
std::vector<std::string_view> battleTargetIds() { return idsFrom(kBattleTargets); }
std::vector<std::string_view> bossArchetypeIds() { return idsFrom(kBossArchetypes); }
std::vector<std::string_view> passiveHookIds() { return idsFrom(kPassiveHooks); }
std::vector<std::string_view> milestoneEffectIds() { return idsFrom(kMilestoneEffects); }
std::vector<std::string_view> triggerWhenIds() { return idsFrom(kTriggerWhens); }
std::vector<std::string_view> triggerDoIds() { return idsFrom(kTriggerDos); }
std::vector<std::string_view> scriptDoIds() { return idsFrom(kScriptDos); }

}  // namespace cd::content
