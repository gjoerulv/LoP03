#pragma once

#include <string>
#include <vector>

#include "content/Enums.hpp"
#include "content/Stats.hpp"

// Immutable content definitions loaded from JSON. Plain data; no behavior.
// Ids are lowercase string keys used for cross-references and lookups.

namespace cd::content {

struct SkillDef {
    std::string id;
    std::string name;
    SkillCategory category = SkillCategory::Physical;
    SkillTarget target = SkillTarget::SingleEnemy;
    Element element = Element::None;
    int power = 0;   // damage/heal magnitude (>= 0)
    int mpCost = 0;  // MP spent (>= 0)

    // Optional status applied to the skill's targets.
    StatusType statusEffect = StatusType::None;
    int statusMagnitude = 0;  // percent for buffs/debuffs, damage for poison
    int statusDuration = 0;   // turns

    // Enmity-control effect (M28): None for ordinary skills; Taunt/Fade/Intercept
    // manipulate the threat model instead of (or besides) dealing damage.
    SkillEffect controlEffect = SkillEffect::None;

    // Revive capability (M43): 0 = the skill cannot raise the fallen (every
    // pre-M43 skill); 1..100 = a KO'd ally target is revived at this percentage
    // of max HP instead of being skipped. Validated to heal/single_ally only.
    int reviveHpPct = 0;

    // M45 (the Goose's comedy tradeoff): the skill ALSO applies its status to
    // every living enemy. Authored on ally-facing skills whose `statusEffect` is
    // a buff, so healing the party cheers the enemy up too.
    bool alsoBuffsEnemies = false;

    std::string description;
};

// One level-gated skill grant on a class learnset (M29). The skill is known
// once the character reaches `level`; `startingSkills` remain the level-1 set.
struct LearnEntry {
    std::string skill;  // skill id
    int level = 1;      // >= 1
};

// One status a basic attack applies on a connecting hit (M45, the Dragon).
struct AttackStatus {
    StatusType type = StatusType::None;
    int magnitude = 0;
    int duration = 0;
};

struct ClassDef {
    std::string id;
    std::string name;
    std::string role;
    StatBlock baseStats;
    StatGrowth growth;
    std::vector<std::string> startingSkills;  // skill ids (level-1 set)
    std::vector<LearnEntry> learnset;         // level-gated grants (M29)

    // M45 (the King's reward classes). Every field is optional and inert by
    // default, so the six original classes are untouched. Class identity is data:
    // no class id is ever branched on in code.
    bool unlockedByKing = false;              // hidden until the King has fallen
    std::vector<EquipSlot> equipBans;         // slots this class may never equip
    bool attackHitsAll = false;               // the basic attack strikes every foe
    std::vector<AttackStatus> attackStatuses; // applied per connecting basic hit
    bool uncontrolled = false;                // takes no player input; acts on its own
    int scoreModPct = 0;                      // per-member additive score modifier

    bool canEquip(EquipSlot slot) const {
        for (EquipSlot banned : equipBans) {
            if (banned == slot) {
                return false;
            }
        }
        return true;
    }
};

// A passive skill (M36): an always-on trait keyed by `hook`, parameterized by a
// single `magnitude`, purchased per character for `price` gold at the Training
// Hall. Also carried by enemies/bosses (an optional list on their defs).
struct PassiveDef {
    std::string id;
    std::string name;
    PassiveHook hook = PassiveHook::None;
    int magnitude = 0;  // hook parameter (percent, MP, etc.)
    int price = 0;      // Training Hall gold cost
    std::string description;
};

// Element affinities (M48). Both lists are optional and empty for every pre-M48
// foe, which is what "no affinity" means: every element resolves at 100 %. The
// loader validates that the two sets are disjoint — a foe that is both weak and
// immune to an element is a content error, not a precedence puzzle.
struct ElementAffinity {
    std::vector<Element> weaknesses;  // x150 % damage
    std::vector<Element> immunities;  // 0 damage, and no status rider lands

    bool weakTo(Element e) const { return contains(weaknesses, e); }
    bool immuneTo(Element e) const { return contains(immunities, e); }
    bool any() const { return !weaknesses.empty() || !immunities.empty(); }

private:
    static bool contains(const std::vector<Element>& v, Element e) {
        for (Element x : v) {
            if (x == e) {
                return true;
            }
        }
        return false;
    }
};

struct EnemyDef {
    std::string id;
    std::string name;
    StatBlock stats;
    EnemyTier tier = EnemyTier::Normal;
    EnemyRole role = EnemyRole::Bruiser;  // required in data (M20 taxonomy)
    std::vector<EnemyTag> tags;
    std::vector<std::string> skills;    // skill ids
    std::vector<std::string> passives;  // passive ids (M36; optional)
    int minTown = 1;                    // per-town gating (M38): spawns only at town >= minTown
    ElementAffinity affinity;           // M48 (optional; empty = no affinity)
    // M49: this enemy exists ONLY as a boss's minion — never in a generated
    // dungeon team and never in an endless wave. A flag rather than a naming
    // convention because two code paths sweep the whole enemy database (the
    // generator's empty-pool fallback and endlessWaveTeam), so simply leaving it
    // out of every theme would not be enough. Default false = ordinary enemy.
    bool bossOnly = false;
    int xpReward = 0;
    int goldReward = 0;
};

// Externalized team-composition constraints (data/composition.json, M20).
// The generator derives every curve from these; it never hard-codes them.
struct CompositionDef {
    // Normal-team size: min (deepMinSize once depth >= deepMinDepth) up to
    // min(maxSizeCap, maxSizeBase + depth / maxSizePerDepths).
    int minSize = 2;
    int deepMinSize = 3;
    int deepMinDepth = 4;
    int maxSizeBase = 2;
    int maxSizePerDepths = 2;
    int maxSizeCap = 5;
    // Elite share: min(elitePctMax, elitePctPerDepth * depth) percent.
    int elitePctPerDepth = 9;
    int elitePctMax = 70;
    // Role rules per normal team.
    int maxSupport = 1;   // healers + buffers
    int minDamage = 1;    // bruisers + snipers
    // Boss minion count bounds (clamping the BossDef list).
    int minMinions = 0;
    int maxMinions = 3;
    // Enemy stat scaling: +pctPerDepth% per depth beyond startDepth, capped.
    int scaleStartDepth = 5;
    int scalePctPerDepth = 6;
    int scalePctMax = 90;

    int teamSizeMin(int depth) const { return depth >= deepMinDepth ? deepMinSize : minSize; }
    int teamSizeMax(int depth) const {
        const int grown = maxSizeBase + depth / (maxSizePerDepths < 1 ? 1 : maxSizePerDepths);
        return grown > maxSizeCap ? maxSizeCap : grown;
    }
    int eliteChancePct(int depth) const {
        const int pct = elitePctPerDepth * depth;
        return pct > elitePctMax ? elitePctMax : pct;
    }
    int statScalePct(int depth) const {
        if (depth <= scaleStartDepth) {
            return 0;
        }
        const int pct = (depth - scaleStartDepth) * scalePctPerDepth;
        return pct > scalePctMax ? scalePctMax : pct;
    }
};

// One status a battle item applies to its target (M44). Authored as a list so an
// item can carry more than one (the Dragon Crown applies ATK- and DEF-) without
// the schema growing a field per slot.
struct ItemStatus {
    StatusType type = StatusType::None;
    int magnitude = 0;  // percent for buffs/debuffs, damage for poison
    int duration = 0;   // turns
};

struct ItemDef {
    std::string id;
    std::string name;
    ItemType type = ItemType::Consumable;
    EquipSlot slot = EquipSlot::None;  // for equipment/relics
    Rarity rarity = Rarity::Common;
    // M48: a WEAPON's element — the basic attacks of whoever wields it carry it.
    // Validated to weapons only; None for everything else, which is every
    // pre-M48 item.
    Element element = Element::None;
    int value = 0;      // gold value (>= 0)
    int minTown = 1;    // per-town gating (M37): stocked/dropped only at town >= minTown
    int maxTown = 0;    // M43: upper end of the town window (0 = unbounded)

    // Consumable behavior.
    ConsumableEffect effect = ConsumableEffect::None;
    int effectAmount = 0;
    // M43: the item also lifts ATK-/DEF- (stat debuffs only - full affliction
    // cleansing remains the Cure effect's job).
    bool curesDebuffs = false;
    // M43 (Royal Snacks): amounts used INSTEAD of the normal ones when the fight
    // is the King's (battle::Battle::kingBattle). 0 = no King-specific behavior,
    // which is every other item. Bespoke King fields follow the M40 precedent
    // (BossDef::immuneToConfusion).
    int kingEffectAmount = 0;
    int kingMpAmount = 0;

    // M44 (Royal Relics). All inert by default, so every pre-M44 item behaves
    // exactly as before.
    BattleTarget battleTarget = BattleTarget::Ally;  // which side it may be used on
    std::vector<ItemStatus> statuses;                // statuses applied to the target
    std::string requiresBossId;   // non-empty: only this boss is affected at all
    int statScalePct = 0;         // non-zero: scales the target's ATK/MAG/DEF/SPD
                                  // for the rest of the battle (50 = halved)
    // M52 (the Dragon Crown's hidden effect): used on a boss carrying a revive
    // clock (the King), this ends it — his fallen court never returns. Schema-
    // driven so no item id is branched on; valid only with battleTarget: enemy.
    // Inert (false) for every other item, so pre-M52 battles are unchanged.
    bool disablesMinionRevive = false;

    // Equipment/relic flat stat bonus.
    StatBlock statBonus;

    // Scroll: the skill id it teaches (empty for non-scrolls).
    std::string grantsSkill;

    std::string description;

    // M43: the town window this item exists in at all - shop stock, chest
    // rewards, and dungeon merchant offers all ask this one question, so an
    // item can never be "sold only in town 1" in one place and everywhere in
    // another. Unbounded above unless maxTown says otherwise.
    bool availableAtTown(int town) const {
        return minTown <= town && (maxTown <= 0 || town <= maxTown);
    }
};

struct BossDef {
    std::string id;
    std::string name;
    BossArchetype archetype = BossArchetype::Brute;
    StatBlock stats;
    std::vector<std::string> skills;    // skill ids
    std::vector<std::string> minions;   // enemy ids fighting alongside the boss
    std::vector<std::string> passives;  // passive ids (M36; bosses may carry several)
    int minTown = 1;                    // per-town gating (M38): chosen only at town >= minTown
    ElementAffinity affinity;           // M48 (optional; empty = no affinity)
    // M49: the revive clock. 0 = the boss never revives its minions (every boss
    // but one). N > 0 = on the Nth of ITS OWN turns taken with every minion
    // down, it raises them all to full HP and starts counting again. Data, not a
    // boss id branch — any boss can carry it, following the M40
    // `immuneToConfusion` precedent.
    int reviveMinionTurns = 0;
    bool immuneToConfusion = false;     // M40: bespoke status immunity (the King)
    std::string telegraph;              // flavor line shown when the battle begins
    int xpReward = 0;
    int goldReward = 0;
    std::string description;
};

struct DungeonThemeDef {
    std::string id;
    std::string name;
    std::vector<std::string> normalEnemies;  // enemy ids
    std::vector<std::string> eliteEnemies;   // enemy ids
    std::vector<std::string> bosses;         // boss ids
    std::string description;
};

// Story serial (M41): one beat per town 1..7 (told by the wandering storyteller)
// plus the Jester's beat at the castle (town == kCastleTown). Pure flavor content;
// no battle/generation/scoring effect.
struct StoryBeat {
    int town = 1;             // 1..7 for the town installments, kCastleTown for the Jester
    std::string speaker;      // who tells it (display)
    std::string title;        // panel heading
    std::string body;         // the beat text (wrapped in the dialog panel)
};

}  // namespace cd::content
