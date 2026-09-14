#pragma once

#include <string>
#include <string_view>
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

    // M95 (rules v17): a summon — castable once per dungeon/challenge run
    // (`oncePerRun`; the shared battle gate refuses a second cast and the run
    // resets the ledger at entry), announced by its creature's name
    // (`summonName`, presentation-only). Both inert by default, so every
    // pre-M95 skill is untouched.
    bool oncePerRun = false;
    std::string summonName;

    // M75 (rules v15): a damaging skill may also drain MP — the target loses
    // this percent of the HP damage it just took as MP (the owner's rule:
    // MP damage is about a quarter of the HP damage, so the authored value is
    // 25). 0 for every pre-M75 skill; valid on physical/magic only.
    int mpDamagePct = 0;

    std::string description;
};

// One level-gated skill grant on a class learnset (M29). The skill is known
// once the character reaches `level`; `startingSkills` remain the level-1 set.
struct LearnEntry {
    std::string skill;  // skill id
    int level = 1;      // >= 1
};

// One status a basic attack applies on a connecting hit (M45, the Dragon).
// M75 reuses the same {type, magnitude, duration} triple for `initialStatuses`
// (statuses a foe starts the battle already carrying).
struct AttackStatus {
    StatusType type = StatusType::None;
    int magnitude = 0;
    int duration = 0;
};

// One deterministic boss/elite trigger (M75, rules v15): WHEN a condition
// holds, DO an action. Authored on enemies and bosses (`triggers[]`); resolved
// onto the Combatant at buildBattle and evaluated in shared battle code, so
// the Simulator and live play agree by construction. Only the fields the
// chosen action reads are meaningful (validated by the loader).
struct TriggerDef {
    TriggerWhen when = TriggerWhen::None;
    int threshold = 0;         // every Nth hit/turn, or the HP percent bound
    TriggerDo action = TriggerDo::None;
    StatusType status = StatusType::None;  // Status* actions
    int magnitude = 0;
    int duration = 0;
    int scaleAttackPct = 100;  // ScaleStatsSelf (100 = unchanged; 200 = doubled)
    int scaleMagicPct = 100;
    int scaleDefensePct = 100;
    int scaleSpeedPct = 100;
    int cloneHpPct = 0;        // SummonCloneSelf: clone max HP as % of the bearer's
    int mpDrainPct = 0;        // DrainFoeMp: % of every living foe's current MP
    std::string text;          // authored announcement line (optional)
};

// M111 (rules v19): one step of a foe's scripted own-turn sequence.
struct ScriptStep {
    ScriptDo action = ScriptDo::None;
    std::vector<AttackStatus> statuses;  // status_all_foes: every living foe, one action
    std::string text;                    // authored announcement (optional)
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
// A class level-milestone bonus (M63): one of the two permanent choices a
// character of `classId` makes on reaching `level` (10/20/30). `option` is
// "a" or "b" (exactly one of each per class+level, validated); the chosen
// entry's id persists on the Character. One `effect` + one `magnitude`
// (0 where the effect needs none) — compound behaviours are their own
// effect values, the PassiveHook precedent.
struct MilestoneDef {
    std::string id;
    std::string classId;
    int level = 0;            // 10, 20 or 30
    std::string option;       // "a" | "b"
    std::string name;
    std::string description;  // shown on the choice modal and the party panel
    MilestoneEffect effect = MilestoneEffect::None;
    int magnitude = 0;
};

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
    // M61 (the Evil Geese): a per-own-turn chance (0-100) this foe simply does
    // nothing, with the flavour line shown when it happens ("Quack."). Decided
    // by a pure seeded hash in shared battle code (rules v12); 0 = never, which
    // is every pre-M61 enemy.
    int doNothingPct = 0;
    std::string doNothingText;
    // M75 (rules v15), all optional and inert by default so every pre-M75
    // enemy is untouched: statuses the foe starts the battle carrying, its
    // deterministic triggers, statuses that can never land on it, and the
    // sleep-aware AI manners (single-target attacks spare sleeping targets
    // while another stands; stun-rider skills are shelved while every foe
    // sleeps).
    std::vector<AttackStatus> initialStatuses;
    std::vector<TriggerDef> triggers;
    std::vector<StatusType> statusImmunities;
    bool avoidSleepingTargets = false;
    bool noStunWhileAllFoesSleep = false;
    // M89 (rules v16): optional MP pool override. 0 = derive from magic (every
    // pre-M89 foe); a positive value replaces the base and scales like magic.
    int maxMp = 0;
    // M111 (rules v19): the foe's scripted own turns — step N of the list on
    // its Nth own turn (a turn a control status took still consumes the step,
    // the M89 lunge precedent), then the ordinary AI. Empty for every foe
    // that carries none.
    std::vector<ScriptStep> script;
    // M111: a foe that exists only for a special encounter (the Golden Goose)
    // — never generated in a theme pool, the fallback sweep, an endless wave
    // or a guild trial. A flag like bossOnly, because two code paths sweep
    // the whole database and a naming convention could be sidestepped.
    bool specialOnly = false;
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

// M81: the gear-icon vocabulary. Every equipment/relic row renders a
// "ui.icon.<category>" pixel icon; these are exactly the categories the
// shipped icon set draws (a plain string vocabulary, the M80 kEventFlavorIds
// idiom — code only ever concatenates the id into a texture key). Kept in
// lockstep with the generator's icon grids by the presentation lint.
inline constexpr const char* kIconCategoryIds[] = {
    "sword", "axe", "dagger", "bow", "staff", "mace",
    "spear", "shield", "armor", "accessory", "relic",
};
inline constexpr int kIconCategoryIdCount =
    static_cast<int>(sizeof(kIconCategoryIds) / sizeof(kIconCategoryIds[0]));

inline bool isIconCategory(const std::string& s) {
    for (const char* id : kIconCategoryIds) {
        if (s == id) {
            return true;
        }
    }
    return false;
}

// The editor id list for the field's enum picker (the *Ids() idiom).
inline std::vector<std::string_view> iconCategoryIds() {
    return {kIconCategoryIds, kIconCategoryIds + kIconCategoryIdCount};
}

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
    // M78: how many of this consumable the party may HOLD before shops refuse
    // to sell another (0 = the type default, kDefaultConsumableCap; see
    // game/ItemCaps.hpp). Enforced at PURCHASE time only — nothing ever clamps
    // an existing overage. Validated 1..9, consumables only.
    int maxHeld = 0;
    // M78 (owner decision 2026-08-05): a premium tonic town item shops never
    // stock — but the in-dungeon merchant may still offer it, at FULL value
    // instead of its usual street discount. Consumables only; chest and
    // merchant pools (availableAtTown) are untouched.
    bool notSoldInTown = false;

    // Consumable behavior.
    ConsumableEffect effect = ConsumableEffect::None;
    int effectAmount = 0;
    // M43: the item also lifts ATK-/DEF- (stat debuffs only - full affliction
    // cleansing remains the Cure effect's job).
    bool curesDebuffs = false;
    // M75: the item lifts Curse (Holy Taxes). Deliberately its own flag —
    // neither the Cure effect nor a cleanse ever touches a Curse, so the two
    // removers (this and an `uncurse` skill) are exactly the ones the owner
    // named. Inert (false) for every other item.
    bool curesCurse = false;
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

    // M75 (engine hook; content arrives in M81): worn equipment may halve (or
    // otherwise reduce) incoming damage of the listed elements. `resistPct`
    // applies to every element in `resistElements` (the all-element legendary
    // simply lists all six). Both-or-neither, equipment/relic only (validated);
    // empty for every pre-M81 item, so the hook is inert until authored.
    std::vector<Element> resistElements;
    int resistPct = 0;

    // M96 (rules v18) — Heirlooms: worn keepsakes whose effects are BATTLE
    // triggers, not stat bonuses. `triggers` reuses the M75 TriggerDef
    // verbatim (attached to the wearer's Combatant at buildBattle); the
    // lowHp* pair is a CONDITIONAL modifier on the Brute-enrage pattern —
    // while the wearer's HP is at or under `lowHpThresholdPct` percent, its
    // attacks hit `lowHpAttackPct` percent harder (announced once). All
    // inert by default; validated to heirlooms only.
    std::vector<TriggerDef> triggers;
    int lowHpThresholdPct = 0;
    int lowHpAttackPct = 0;

    // M81: which hand-authored gear icon this piece renders with in every
    // equipment list ("ui.icon.<category>"). Optional where the slot makes it
    // obvious (armor and accessory pieces and relics default to their slot's
    // category via iconCategoryFor); a WEAPON must author one — a sword and a
    // staff share a slot. Validated against kIconCategoryIds, gear only.
    std::string iconCategory;

    // M76: a one-liner shown on the quip channel when this item is used in
    // battle (the Evil Duckling's Hilarious Punchline). Presentation only —
    // nothing in the battle model reads it. Empty for every other item.
    std::string useLine;

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

// M81: the icon category a piece of gear actually renders with — the authored
// override, else the slot-derived default. Weapons have no default, so an
// empty result on a weapon is a content error (the loader and the presentation
// lint both catch it); render code treats empty as "draw no icon". Non-gear
// items never have an icon.
inline std::string iconCategoryFor(const ItemDef& d) {
    if (d.type != ItemType::Equipment && d.type != ItemType::Relic &&
        d.type != ItemType::Heirloom) {
        return "";
    }
    if (!d.iconCategory.empty()) {
        return d.iconCategory;
    }
    // M96: heirlooms lead with the relic keepsake icon (a dedicated 10x10
    // heirloom glyph is deferred — recorded in the M96 note).
    if (d.type == ItemType::Relic || d.type == ItemType::Heirloom) {
        return "relic";
    }
    if (d.slot == EquipSlot::Armor) {
        return "armor";
    }
    if (d.slot == EquipSlot::Accessory) {
        return "accessory";
    }
    return "";
}

// The manifest texture key that category renders as ("" = no icon). Kept next
// to iconCategoryFor so the render sites and the presentation lint share one
// convention instead of each concatenating its own.
inline std::string gearIconTextureId(const ItemDef& d) {
    const std::string cat = iconCategoryFor(d);
    return cat.empty() ? "" : "ui.icon." + cat;
}

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
    // M61 (the Deadly Duck), all inert by default so every pre-M61 boss is
    // untouched: the basic attack sweeps the whole party (the M45 class
    // machinery, boss-side), applies status riders per connecting hit, and
    // `immuneToAfflictions` shrugs off every affliction — poison, confusion,
    // silence, blind, terrified, stunned — while ATK-/DEF- debuffs still land.
    bool attackHitsAll = false;
    std::vector<AttackStatus> attackStatuses;
    bool immuneToAfflictions = false;
    // M75 (rules v15), all optional and inert by default (see EnemyDef): the
    // battle-start statuses, the deterministic triggers, the per-status
    // immunity list (the Dragon's bespoke matrix), the sleep-aware AI manners,
    // and `immuneToStatScale` — the boss shrugs off a battle-long stat-scale
    // relic (the Deadly Spoon) entirely.
    std::vector<AttackStatus> initialStatuses;
    std::vector<TriggerDef> triggers;
    std::vector<StatusType> statusImmunities;
    bool avoidSleepingTargets = false;
    bool noStunWhileAllFoesSleep = false;
    bool immuneToStatScale = false;
    // M89 (rules v16): optional MP pool override (0 = derive from magic;
    // scales like magic), and the every-Nth-own-turn basic attack — N > 0
    // makes the boss swing instead of casting on every Nth of its own turns
    // (the Dragon's lunge), with an optional authored flavour line shown by
    // the battle screen when it fires.
    int maxMp = 0;
    int basicAttackEveryNth = 0;
    std::string basicAttackText;
    // M84: 0 for every ordinary boss. 1..7 marks this boss as that town's
    // GUILD MASTER: fought only in the town's guild gauntlet, excluded from
    // the dungeon boss pools and the Boss Rush (the kKingBossId exclusion
    // rule, as data instead of an id constant), and eligible for the Endless
    // Rush's every-10th-wave boss draw. At most one Master per town
    // (validated).
    int guildTown = 0;
    // M111/M112: a boss fought only in a special encounter (the Mimic) —
    // excluded from the dungeon boss pools, the Boss Rush, the Endless draw
    // and the treasure guards the way guildTown excludes a Master.
    bool specialOnly = false;
    std::string telegraph;              // flavor line shown when the battle begins
    int xpReward = 0;
    int goldReward = 0;
    std::string description;
};

// M106: minTown gates WHERE a theme is offered (the Goosy Gauntlet belongs
// to town 7 alone); 1 = everywhere, every pre-M106 theme.
struct DungeonThemeDef {
    std::string id;
    std::string name;
    std::vector<std::string> normalEnemies;  // enemy ids
    std::vector<std::string> eliteEnemies;   // enemy ids
    std::vector<std::string> bosses;         // boss ids
    std::string description;
    int minTown = 1;  // M106: offered at the Guild only from this town on
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

// M80: authored flavor for one dungeon event kind (data/event_flavor.json) —
// the centered panel's title and body. Pure presentation: nothing in the
// battle, generation or scoring model reads it, and an event whose id is
// absent (or the whole file missing — it is the one OPTIONAL content file)
// falls back to the classic footer prompt.
struct EventFlavorDef {
    std::string id;     // one of kEventFlavorIds
    std::string title;  // panel heading (one line)
    std::string body;   // dry-humor flavor (wrapped in the panel)
};

// The event vocabulary the loader accepts — the content-layer mirror of
// dungeon::RoomEventKind (which lives a layer above and cannot be named
// here); a test holds the two in lockstep.
inline constexpr const char* kEventFlavorIds[] = {
    "shrine",       "healing_spring", "merchant",    "elite_challenge",
    "score_wager",  "rest_token",     "royal_relic", "armory_ghost",
    "miners_cache", "elder_root",     "duck_peddler",
    "surveyor",     "dragonform",  // M93 (generation v17)
    "goose_polymorph", "sacrifice",      "level_altar",  // M103 (generation v19)
    "stranger_story",  "token_exchange", "patrol_reset",
    "reels",           "blackjack",  // M104 (generation v20)
    "goosy_flock",  // M106 (generation v21; the Goosy Gauntlet's rite)
};
inline constexpr std::size_t kEventFlavorIdCount = 22;

// M99: authored tutorial-prompt text (data/tutorials.json, the third OPTIONAL
// content file) — a beat's title and body, CrystalForge-editable. Pure
// presentation: the trigger keys stay code-owned (tutorial::kBeats, a layer
// above, so known-ness and full coverage are test-enforced — the curio-lore
// precedent), and a beat absent from the file (or the whole file missing)
// falls back to the constexpr text it shipped with.
struct TutorialTextDef {
    std::string id;     // a tutorial::kBeats id
    std::string title;  // prompt heading (one line)
    std::string body;   // the teaching text (wrapped in the prompt panel)
};

// M85: authored inspect-lore for one dungeon curio (data/curio_lore.json,
// the second OPTIONAL content file) — the Maps screen's lore panel. Pure
// presentation: a curio without its entry simply shows its name and
// description as before. Ids mirror game/Curios.hpp's table (a layer above,
// so known-ness and full coverage are test-enforced, not loader-enforced).
struct CurioLoreDef {
    std::string id;    // a curio id
    std::string body;  // the dry Duck-mythology lore (wrapped in the panel)
};

// M112: one of the Jester's lore questions (data/lore_questions.json — an
// OPTIONAL file like the flavor files; an empty pool means the lore patrol
// falls back to an ordinary one). `minTown` is the earliest ladder tier at
// which the fact is known (no spoilers below it); `postKing` gates the
// handful that only make sense once the King has fallen.
struct LoreQuestionDef {
    std::string id;
    int minTown = 1;        // 1..7
    bool postKing = false;
    std::string question;   // the prompt above the field (two lines at most)
    std::string answer;     // the right answer (a field box)
    std::string wrongAnswer;
    std::string mockLine;   // the Jester's line when the pick is wrong
};

// M97: the Hooded Goose story cutscenes (data/cutscenes.json — REQUIRED, it
// grants heirlooms, so unlike the two optional flavor files its absence is a
// content error). One scene per kCutsceneIds entry: dialogue beats, then a
// mandatory pick-one-of-two choice whose options each carry an heirloom and
// one response line. Beat text may use the {member1}..{member4} name tokens
// (resolved at display time, game/Cutscenes.hpp).
struct CutsceneBeat {
    std::string speaker;         // display name for the dialogue panel title
    std::string text;            // the beat's prose (tokens allowed)
    std::string emote = "idle";  // one of kGooseEmotes — the goose's stage act
    bool kingOnStage = false;    // stage the Hollow King behind the goose
    bool dragonOnStage = false;  // stage the Last Dragon behind the goose
};

struct CutsceneOption {
    std::string label;            // the choice row (short, one line)
    std::string heirloomId;       // item granted on first pick (type heirloom)
    std::string responseSpeaker;  // who answers the pick
    std::string responseText;     // the one closing line (tokens allowed)
};

struct CutsceneDef {
    std::string id;        // one of kCutsceneIds, or a "joke_*" id (M100)
    std::string question;  // the choice prompt (required only with options)
    std::vector<CutsceneBeat> beats;      // at least one
    // Exactly two for the story scenes (the 8x2 heirloom promise), or NONE
    // for a "joke_*" scene (M100): a tale with nothing to grant simply ends
    // after its last beat.
    std::vector<CutsceneOption> options;
};

// The scene vocabulary: the new-game prologue, the first arrival at each of
// towns 2..7, and the post-King finale at town 7's would-be eastern road.
// M100: any id starting with "joke_" is also known — the post-finale dry
// jokes the stranger cycles through; forge users may author more.
inline constexpr const char* kCutsceneIds[] = {
    "new_game", "town_2", "town_3", "town_4", "town_5", "town_6", "town_7", "finale",
};
inline constexpr std::size_t kCutsceneIdCount = 8;
inline constexpr const char* kJokeCutscenePrefix = "joke_";    // M100
inline constexpr const char* kStoryCutscenePrefix = "story_";  // M103: dungeon tales
inline constexpr const char* kPatrolCutscenePrefix = "patrol_";  // M110: the Stranger's patrol scenes

// The goose's stage vocabulary (presentation only; unknown never loads).
inline constexpr const char* kGooseEmotes[] = {"idle", "waddle", "jump", "panic"};
inline constexpr std::size_t kGooseEmoteCount = 4;

}  // namespace cd::content
