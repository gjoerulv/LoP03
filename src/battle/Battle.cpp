#include "battle/Battle.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>

#include "battle/BattleObserver.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Castle.hpp"  // kKingBossId (M43: King-context items)
#include "game/Milestones.hpp"  // M63: chosen level-milestone resolution
#include "game/Party.hpp"
#include "game/Scrolls.hpp"  // M64: allKnownSkills (learnset + scroll extras)

namespace cd::battle {

namespace {

// M60: the single shape of every telemetry emit — one skipped branch when no
// recorder is attached, so the null path (the game, the Simulator, every
// pre-M60 test) is byte-identical by construction.
inline void emitEvent(const Battle& b, const BattleEvent& event) {
    if (b.observer != nullptr) {
        b.observer->onEvent(event);
    }
}

}  // namespace

// M48 — the element rule. Deliberately the only place the x0 / x1 / x1.5
// decision is made, so a future affinity source (equipment, a status) has one
// obvious home. Pure: no rolls, no content lookups.
int elementModifier(const Combatant& defender, content::Element e) {
    if (e == content::Element::None) {
        return 100;
    }
    for (content::Element x : defender.immunities) {
        if (x == e) {
            return kElementImmunePct;  // checked first: immunity is absolute
        }
    }
    int mod = 100;
    for (content::Element x : defender.weaknesses) {
        if (x == e) {
            mod = kElementWeakPct;
            break;
        }
    }
    // M75: worn element resistance (resolved from equipment at buildBattle)
    // reduces whatever remains — a 50% resist halves a neutral hit and takes a
    // weak one to 75%. Zero for every unit without authored resist gear, so
    // every pre-M81 battle keeps the exact 0/100/150 outcomes.
    const int resist = defender.elementResist[static_cast<std::size_t>(e)];
    if (resist > 0) {
        mod = mod * (100 - std::min(resist, 100)) / 100;
    }
    return mod;
}

bool isImmuneToElement(const Combatant& defender, content::Element e) {
    return elementModifier(defender, e) == kElementImmunePct;
}

namespace {

int statusSum(const Combatant& c, content::StatusType type) {
    int sum = 0;
    for (const StatusInstance& s : c.statuses) {
        if (s.type == type) {
            sum += s.magnitude;
        }
    }
    return sum;
}

int attackPercent(const Combatant& c) {
    return std::max(10, 100 + statusSum(c, content::StatusType::AttackUp) -
                            statusSum(c, content::StatusType::AttackDown));
}

int defensePercent(const Combatant& c) {
    return std::max(10, 100 + statusSum(c, content::StatusType::DefenseUp) -
                            statusSum(c, content::StatusType::DefenseDown));
}

// M75 (rules v15): the ATK+/- percent no longer scales the attack stat alone —
// it scales the whole offensive (attack + power) term in physicalDamage, so a
// buffed skill hits like a buffed skill. Enrage stays a raw-attack surge.
int effectiveAttack(const Combatant& a) {
    int value = a.stats.attack;
    if (a.enrages && a.hp * 2 < a.maxHp) {
        value = value * 3 / 2;  // Brute enrage
    }
    return value;
}

// M48: `element` is applied LAST, after the max(1, ...) floor — an immune hit
// must land on 0, and the floor would otherwise turn it into 1. Guarding and
// affinity therefore compose multiplicatively, which is what a player reading
// "guarded" and "immune" separately would expect.
// M63 (Deep Guard): a guard blocks the defender's guardBlockPct when one is
// set; the default 50 reproduces the historical dmg/2 exactly (dmg*50/100).
int guardedDamage(const Combatant& d, int dmg) {
    const int block = d.guardBlockPct > 0 ? d.guardBlockPct : 50;
    return std::max(1, dmg * (100 - block) / 100);
}

// M63 (Elemental Attunement): a weak hit deals the attacker's override when it
// carries one. Immune (0) and neutral (100) are never overridden.
// M85: an INTRINSIC basic-attack element (a chosen class milestone, not a
// wielded weapon) is never nullified — the M81-narrowed M48 absolute, now an
// engine rule: against an immune foe it resolves at the neutral 100% (no
// "Immune" float, no weak bonus — plain damage). Wielded and skill elements
// still meet immunities as the informed trade they are.
int attackerElementMod(const Combatant& a, const Combatant& d, content::Element element) {
    const int mod = elementModifier(d, element);
    if (mod == kElementWeakPct && a.weaknessBonusPct > 0) {
        return a.weaknessBonusPct;
    }
    if (mod == 0 && a.elementIntrinsic && element == a.weaponElement) {
        return 100;
    }
    return mod;
}

// M75 (rules v15): DEF+/- moved from the tiny def-term nudge to a scale on the
// FINAL damage taken — DEF+30 now shaves ~23% off every hit and DEF-30 adds
// ~43%, which is a buff a player can feel. With no statuses both percents are
// 100 and every formula below reduces exactly to its v14 shape, so a
// status-free battle is byte-identical.
int physicalDamage(const Combatant& a, const Combatant& d, int power,
                   content::Element element) {
    int dmg = std::max(1, (effectiveAttack(a) + power) * attackPercent(a) / 100 -
                              d.stats.defense / 2);
    dmg = std::max(1, dmg * 100 / defensePercent(d));
    if (d.guarding) {
        dmg = guardedDamage(d, dmg);
    }
    return dmg * attackerElementMod(a, d, element) / 100;
}

int magicDamage(const Combatant& a, const Combatant& d, int power, content::Element element) {
    // ATK+/- deliberately does not touch magic (its historical shape); DEF+/-
    // scales the final hit exactly like the physical path.
    int dmg = std::max(1, a.stats.magic + power - d.stats.defense / 4);
    dmg = std::max(1, dmg * 100 / defensePercent(d));
    if (d.guarding) {
        dmg = guardedDamage(d, dmg);
    }
    return dmg * attackerElementMod(a, d, element) / 100;
}

int healValue(const Combatant& a, int power) { return power + a.stats.magic / 2; }

void addStatus(Combatant& c, content::StatusType type, int magnitude, int turns,
               int extraTurns = 0) {
    if (type == content::StatusType::None || turns <= 0) {
        return;
    }
    // M61: an affliction-immune unit (the Deadly Duck) shrugs off every
    // affliction at this single chokepoint — poison, confusion, silence, blind,
    // terrified, stunned — while stat debuffs (and buffs) still land. Display
    // sites skip these via isImmuneTo, so a blocked status is also never shown.
    if (c.afflictionImmune && isAffliction(type)) {
        return;
    }
    // M75: the bespoke per-status immunity list (the Dragon) blocks at the same
    // chokepoint. Only new content carries one, so earlier battles are
    // untouched. (The legacy immunity FLAGS deliberately keep their old
    // query-side behaviour — a stored-but-inert status still counts as a
    // debuff for Keen Senses, and changing that would alter v14 battles.)
    for (content::StatusType x : c.statusImmunities) {
        if (x == type) {
            return;
        }
    }
    // M35: statuses last 2x their authored duration - EXCEPT the M44 turn-control
    // statuses, which take the turn itself. Doubling those would quietly turn one
    // skipped turn into two, so they are applied exactly as authored. (Statuses
    // tick at the START of the bearer's turn, before it acts, so a duration of 2
    // costs it exactly one turn.)
    const bool turnControl = type == content::StatusType::Terrified ||
                             type == content::StatusType::Stunned;
    // M63 (Lingering Hex): a caster's bonus turns extend the EFFECTIVE (post-
    // scale) duration and never a turn-control status — extending a stolen
    // turn would be a different rule entirely.
    int scaledTurns =
        turnControl ? turns : turns * kStatusDurationMult + std::max(0, extraTurns);
    // M75: a Curse outstays everything else by half again (owner decision
    // 2026-08-05) — the price of its two-remover exclusivity being fair.
    if (type == content::StatusType::Curse) {
        scaledTurns = scaledTurns * kCurseDurationPct / 100;
    }
    for (StatusInstance& s : c.statuses) {
        if (s.type == type) {
            s.magnitude = magnitude;
            s.turns = scaledTurns;
            return;
        }
    }
    c.statuses.push_back({type, magnitude, scaledTurns});
}

void removeStatus(Combatant& c, content::StatusType type) {
    c.statuses.erase(std::remove_if(c.statuses.begin(), c.statuses.end(),
                                    [type](const StatusInstance& s) { return s.type == type; }),
                     c.statuses.end());
}

void clearNegativeStatuses(Combatant& c) {
    // Cure strips every negative status (poison, ATK-/DEF- debuffs, the M35
    // Confusion/Silence/Blind, and now Sleep), keeping only the beneficial
    // buffs. This is the ITEM rule (Remedy/Antidote) — M47 narrowed the SKILL
    // cleanse below, and deliberately left this one alone so cure items are
    // unchanged. M75: a Curse SURVIVES a Cure — the owner named its only two
    // removers (an `uncurse` skill, a `curesCurse` item) — and Reflect is a
    // beneficial status, so both are kept.
    std::vector<StatusInstance> kept;
    for (const StatusInstance& s : c.statuses) {
        if (s.type == content::StatusType::AttackUp || s.type == content::StatusType::DefenseUp ||
            s.type == content::StatusType::Reflect || s.type == content::StatusType::Curse) {
            kept.push_back(s);
        }
    }
    c.statuses = std::move(kept);
}

// M47 (rules v7): the `cleanse` skill control lifts AFFLICTIONS only — Poison,
// Blind, Silence, Confusion, and (M75) Sleep. ATK-/DEF- now survive a Purify
// (a cure item or Royal Snacks is what lifts those), and so do the M44
// turn-control statuses, which take the turn they were bought with. M75: a
// Curse survives a cleanse too — only its two named removers touch it.
// Returns whether anything went, so the log can stay honest.
bool clearAfflictions(Combatant& c) {
    const std::size_t before = c.statuses.size();
    c.statuses.erase(std::remove_if(c.statuses.begin(), c.statuses.end(),
                                    [](const StatusInstance& s) {
                                        return s.type == content::StatusType::Poison ||
                                               s.type == content::StatusType::Blind ||
                                               s.type == content::StatusType::Silence ||
                                               s.type == content::StatusType::Confusion ||
                                               s.type == content::StatusType::Sleep;
                                    }),
                     c.statuses.end());
    return c.statuses.size() != before;
}

// M43: lift only the stat debuffs (ATK-/DEF-), leaving poison and the M35
// afflictions in place. Royal Snacks pick you up; a Remedy is still what cures
// you.
bool clearStatDebuffs(Combatant& c) {
    const std::size_t before = c.statuses.size();
    c.statuses.erase(std::remove_if(c.statuses.begin(), c.statuses.end(),
                                    [](const StatusInstance& s) {
                                        return s.type == content::StatusType::AttackDown ||
                                               s.type == content::StatusType::DefenseDown;
                                    }),
                     c.statuses.end());
    return c.statuses.size() != before;
}

const char* statusLabel(content::StatusType type) {
    switch (type) {
        case content::StatusType::Poison: return "Poison";
        case content::StatusType::AttackUp: return "ATK+";
        case content::StatusType::AttackDown: return "ATK-";
        case content::StatusType::DefenseUp: return "DEF+";
        case content::StatusType::DefenseDown: return "DEF-";
        case content::StatusType::Confusion: return "Confusion";
        case content::StatusType::Silence: return "Silence";
        case content::StatusType::Blind: return "Blind";
        case content::StatusType::Terrified: return "Terrified";
        case content::StatusType::Stunned: return "Stunned";
        case content::StatusType::Reflect: return "Reflect";  // M75
        case content::StatusType::Sleep: return "Sleep";      // M75
        case content::StatusType::Curse: return "Curse";      // M75
        case content::StatusType::None: return "";
    }
    return "";
}

// M75: the magnitude a status actually lands with. Poison scales with the
// APPLIER's Magic (snapshotted here, at application) so a flat authored tick
// stays a threat against endgame HP pools; everything else is authored as-is.
// Item-applied statuses keep their authored numbers (an item has no caster
// craft behind it) — this helper is for skills, attack riders and triggers.
int statusMagnitudeFor(const Combatant& applier, content::StatusType type, int base) {
    if (type == content::StatusType::Poison) {
        return base + applier.stats.magic / kPoisonMagicDiv;
    }
    return base;
}

// applyDamage is a Battle member (M53) so it can honour the debug god-mode
// clamp; its definition lives at namespace scope just after this anonymous
// namespace closes (a member cannot be defined inside an anonymous namespace).

void applyHeal(Combatant& d, int amount) {
    if (d.hp <= 0) {
        return;  // a KO'd unit needs a revive, not a heal
    }
    d.hp = std::min(d.maxHp, d.hp + amount);
}

// --- M28 enmity/targeting helpers ---

// SplitMix64 mix — the basis for the deterministic targeting tie-break and the
// M35 to-hit / confusion roll stream.
std::uint64_t mix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

// Per-use salts for the M35/M36 roll stream, so independent chance draws taken in
// the same action stay independent.
constexpr std::uint64_t kSaltBlind = 0xB11D5EED0F0F0F0Full;   // physical miss (Blind + Evasion)
constexpr std::uint64_t kSaltConfuse = 0xC0FFED15C0117E00ull;
constexpr std::uint64_t kSaltWard = 0x5FADE0000ABCDEF0ull;    // Spell Ward magic fizzle
// M45 Jester salts. These mix into a PURE hash (targetJitter), never into the
// roll stream, so an uncontrolled turn and its quip are reproducible without any
// ordering contract between the battle screen and the Simulator.
constexpr std::uint64_t kSaltJesterAct = 0x1E57E7AC7100D1E5ull;   // which action
constexpr std::uint64_t kSaltJesterAim = 0x1E57E7A10A1D0000ull;   // which target
constexpr std::uint64_t kSaltJesterLine = 0x1E57E71114E00D1Eull;  // which quip, if any
// M45: how often an uncontrolled Jester quips (presentation only).
constexpr int kJestChancePct = 15;
// M58: the "geese scare the King" rule. Its own salt keeps its per-turn roll
// clear of every other pure-hash stream. 10% per living Goose, additive.
constexpr std::uint64_t kSaltGooseScare = 0x600D6005E5CA1E00ull;  // "goose scare"
constexpr int kGoosePerScarePct = 10;
// M61: the authored do-nothing roll (a Quacking goose). Its own salt, same
// pure-hash contract as the scare above.
constexpr std::uint64_t kSaltDoNothing = 0x0DAC0DAC0DAC0001ull;

// Small deterministic jitter in [0, range) from the battle seed + round + acting
// enemy + candidate. Pure, so a given encounter always resolves identically and
// live play and the Simulator agree. Only breaks near-ties (range is small).
long targetJitter(std::uint64_t seed, int turn, int actor, int candidate, int range) {
    if (range <= 1) {
        return 0;
    }
    const std::uint64_t h = mix64(seed ^ mix64(static_cast<std::uint64_t>(turn) * 0x1000193ull) ^
                                  mix64(static_cast<std::uint64_t>(actor) * 0x100000001B3ull) ^
                                  mix64(static_cast<std::uint64_t>(candidate) * 0x9E3779B1ull));
    return static_cast<long>(h % static_cast<std::uint64_t>(range));
}

// Redirect (M28): an enemy's single-target hit aimed at a party member is taken
// by an intercepting ally instead, if there is one.
int redirectTarget(const std::vector<Combatant>& units, int actor, int target) {
    if (actor < 0 || target < 0 || target >= static_cast<int>(units.size())) {
        return target;
    }
    if (units[static_cast<std::size_t>(actor)].side != Side::Enemy) {
        return target;
    }
    const Combatant& t = units[static_cast<std::size_t>(target)];
    if (t.side != Side::Party || t.intercepting) {
        return target;
    }
    for (std::size_t i = 0; i < units.size(); ++i) {
        const Combatant& u = units[i];
        if (u.side == Side::Party && u.alive() && u.intercepting) {
            return static_cast<int>(i);
        }
    }
    return target;
}

// An enemy's targeting profile, derived from its role (bosses from archetype).
enum class TargetProfile { Aggressive, Opportunist, Tactician, Protector, Spread };

TargetProfile profileFor(const Combatant& self, const content::ContentDatabase& db) {
    if (self.isBoss) {
        if (const content::BossDef* boss = db.findBoss(self.sourceId)) {
            switch (boss->archetype) {
                case content::BossArchetype::Brute: return TargetProfile::Aggressive;
                case content::BossArchetype::Sorcerer: return TargetProfile::Tactician;
                case content::BossArchetype::Commander: return TargetProfile::Protector;
                case content::BossArchetype::Rush: return TargetProfile::Aggressive;
            }
        }
        return TargetProfile::Aggressive;
    }
    if (const content::EnemyDef* def = db.findEnemy(self.sourceId)) {
        switch (def->role) {
            case content::EnemyRole::Bruiser: return TargetProfile::Aggressive;
            case content::EnemyRole::Sniper: return TargetProfile::Opportunist;
            case content::EnemyRole::Disruptor: return TargetProfile::Tactician;
            case content::EnemyRole::Healer: return TargetProfile::Tactician;
            case content::EnemyRole::Buffer: return TargetProfile::Tactician;
            case content::EnemyRole::Protector: return TargetProfile::Protector;
            case content::EnemyRole::Attrition: return TargetProfile::Spread;
        }
    }
    return TargetProfile::Opportunist;  // default keeps the old "go for the weak" feel
}

// How attractive party member `c` is to an enemy with profile `p` (higher wins).
long targetScore(TargetProfile p, const Combatant& c, long threat) {
    const long missing = static_cast<long>(c.maxHp - c.hp);            // kill pressure
    const long dps = std::max(c.stats.attack, c.stats.magic);         // how dangerous
    const long caster = c.stats.magic;                                // backline weight
    switch (p) {
        case TargetProfile::Aggressive: return 2 * threat + 4 * dps;
        case TargetProfile::Opportunist: return 3 * missing + threat + 2 * dps;
        case TargetProfile::Tactician: return 4 * caster + threat;
        case TargetProfile::Protector: return 3 * threat + dps;
        case TargetProfile::Spread: return 2 * static_cast<long>(c.hp) + threat;  // healthiest
    }
    return threat;
}

// --- M36 passive helpers ---

// Resolves a unit's passive ids into its Combatant effect fields (mirrors the
// boss-flag resolution). Unknown ids are skipped defensively.
void applyPassives(Combatant& c, const std::vector<std::string>& ids,
                   const content::ContentDatabase& db) {
    for (const std::string& id : ids) {
        const content::PassiveDef* p = db.findPassive(id);
        if (p == nullptr) {
            continue;
        }
        c.passiveIds.push_back(id);
        const int m = p->magnitude;
        switch (p->hook) {
            case content::PassiveHook::Counter: c.counterAttack = true; break;
            case content::PassiveHook::Evasion: c.evasionPct = m; break;
            case content::PassiveHook::SpellWard: c.spellWardPct = m; break;
            case content::PassiveHook::Thorns: c.thornsPct = m; break;
            case content::PassiveHook::Lifedrink: c.lifedrinkPct = m; break;
            case content::PassiveHook::Clarity: c.clarityMp = m; c.silenceImmune = true; break;
            case content::PassiveHook::IronWill: c.ironWill = true; break;
            case content::PassiveHook::FirstStrike:
                c.firstStrike = true;
                c.firstStrikeBonusPct = m;
                break;
            case content::PassiveHook::Bodyguard: c.bodyguardPct = m; break;
            case content::PassiveHook::KeenSenses: c.blindImmune = true; c.keenSensesPct = m; break;
            case content::PassiveHook::None: break;
        }
    }
}

// M63: resolve a party member's CHOSEN level milestones onto its Combatant —
// the applyPassives pattern. Grants of existing hooks reuse the M36 fields
// and never weaken an equipped passive (percents take the max, booleans OR).
// Stat* effects are already inside the character's derived stats
// (refreshCharacter) and resolve to nothing here.
void applyMilestones(Combatant& u, const Character& c, const content::ContentDatabase& db) {
    forEachChosenMilestone(c, db, [&](const content::MilestoneDef& m) {
        using E = content::MilestoneEffect;
        switch (m.effect) {
            case E::BasicAttackPct: u.basicAttackPct += m.magnitude; break;
            case E::MagicSkillPct: u.magicSkillPct += m.magnitude; break;
            case E::AoeSpellPct: u.aoeSpellPct += m.magnitude; break;
            case E::HealCastPct: u.healCastPct += m.magnitude; break;
            case E::ExecutePct: u.executePct += m.magnitude; break;
            case E::VsAfflictedPct: u.vsAfflictedPct += m.magnitude; break;
            case E::WeaknessBonusPct:
                u.weaknessBonusPct = std::max(u.weaknessBonusPct, m.magnitude);
                break;
            case E::StatusTurnsBonus: u.statusTurnsBonus += m.magnitude; break;
            case E::OpeningGuardPct:
                // Authored 1 turn -> the M35 doubling makes it the described
                // 2 effective ticks, exactly like a cast DEF+.
                addStatus(u, content::StatusType::DefenseUp, m.magnitude, 1);
                break;
            case E::DoubleStrikePct: u.doubleStrikePct = m.magnitude; break;
            case E::SweepAllPct:
                u.attackHitsAll = true;
                u.sweepScalePct = m.magnitude;
                break;
            case E::SweepDebuffPct:
                u.attackStatuses.push_back({content::StatusType::AttackDown, m.magnitude, 1});
                break;
            case E::TauntDebuffPct: u.tauntDebuffPct = m.magnitude; break;
            case E::GuardBlockPct: u.guardBlockPct = std::max(u.guardBlockPct, m.magnitude); break;
            case E::FirstHitImmune: u.firstHitImmune = true; break;
            case E::IronWillHealing:
                u.ironWill = true;
                u.ironWillHealPct = m.magnitude;
                break;
            case E::ReviveAtPct: u.reviveAtPct = std::max(u.reviveAtPct, m.magnitude); break;
            case E::PurifyHeals: u.purifyHeals = true; break;
            case E::HolyBasic:
                if (u.weaponElement == content::Element::None) {
                    u.weaponElement = content::Element::Holy;  // a real weapon element wins
                    u.elementIntrinsic = true;                 // M85: never nullified
                }
                break;
            case E::FireBasic:
                if (u.weaponElement == content::Element::None) {
                    u.weaponElement = content::Element::Fire;
                    u.elementIntrinsic = true;  // M85: never nullified
                }
                break;
            case E::GoldBonusPct: u.goldBonusPct += m.magnitude; break;
            case E::ItemPotencyPct: u.itemPotencyPct += m.magnitude; break;
            case E::NoEnemyBuff: u.noEnemyBuff = true; break;
            case E::OnKillPartyAtkUp: u.onKillPartyAtkUpPct = m.magnitude; break;
            case E::OnDeathFoeDebuff: u.onDeathFoeDebuffPct = m.magnitude; break;
            case E::GrantCounter: u.counterAttack = true; break;
            case E::GrantEvasion: u.evasionPct = std::max(u.evasionPct, m.magnitude); break;
            case E::GrantSpellWard: u.spellWardPct = std::max(u.spellWardPct, m.magnitude); break;
            case E::GrantThorns: u.thornsPct = std::max(u.thornsPct, m.magnitude); break;
            case E::GrantIronWill: u.ironWill = true; break;
            case E::GrantFirstStrike:
                u.firstStrike = true;
                u.firstStrikeBonusPct = std::max(u.firstStrikeBonusPct, m.magnitude);
                break;
            case E::GrantClarity:
                u.clarityMp = std::max(u.clarityMp, m.magnitude);
                u.silenceImmune = true;
                break;
            case E::GrantBodyguard:
                u.bodyguardPct = std::max(u.bodyguardPct, m.magnitude);
                break;
            case E::StatMaxHpPct:
            case E::StatSpeedPct:
            case E::StatMaxMpPct:
            case E::StatDefensePct:
            case E::None:
                break;
        }
    });
}

// M75: mirror an authored trigger list onto the Combatant (the StatusInstance
// precedent — the pure model never holds a content struct).
void resolveTriggers(Combatant& u, const std::vector<content::TriggerDef>& defs) {
    for (const content::TriggerDef& t : defs) {
        TriggerRule r;
        r.when = t.when;
        r.threshold = t.threshold;
        r.action = t.action;
        r.status = t.status;
        r.magnitude = t.magnitude;
        r.duration = t.duration;
        r.scaleAttackPct = t.scaleAttackPct;
        r.scaleMagicPct = t.scaleMagicPct;
        r.scaleDefensePct = t.scaleDefensePct;
        r.scaleSpeedPct = t.scaleSpeedPct;
        r.mpDrainPct = t.mpDrainPct;
        r.text = t.text;
        u.triggers.push_back(std::move(r));
    }
}

// M75: statuses a foe starts the battle already carrying (`initialStatuses`).
// Applied through the addStatus chokepoint, so durations scale and immunities
// hold exactly as they would for a cast status. Authored magnitudes are kept
// as-is (there is no caster to scale a battle-start poison by).
void applyInitialStatuses(Combatant& u, const std::vector<content::AttackStatus>& list) {
    for (const content::AttackStatus& s : list) {
        if (s.type == content::StatusType::None || isImmuneTo(u, s.type)) {
            continue;
        }
        addStatus(u, s.type, s.magnitude, s.duration);
    }
}

// True if the unit carries any negative status (for Keen Senses' bonus).
bool hasAnyDebuff(const Combatant& c) {
    for (const StatusInstance& s : c.statuses) {
        switch (s.type) {
            case content::StatusType::Poison:
            case content::StatusType::AttackDown:
            case content::StatusType::DefenseDown:
            case content::StatusType::Confusion:
            case content::StatusType::Silence:
            case content::StatusType::Blind:
            case content::StatusType::Sleep:  // M75
            case content::StatusType::Curse:  // M75
                return true;
            default:
                break;
        }
    }
    return false;
}

// Combined physical miss chance vs a defender: Blind on the attacker (75 %) or
// Evasion on the defender (its %), and 100 % when a blind attacker faces an
// evader (owner rule). 0 (the common case) means no roll is taken.
int physicalMissPct(const Combatant& a, const Combatant& d) {
    const bool blind = isBlinded(a);
    const int evade = d.evasionPct;
    if (blind && evade > 0) {
        return 100;
    }
    return std::max(blind ? kBlindMissPct : 0, evade);
}

}  // namespace

void Battle::applyDamage(Combatant& d, int dmg, std::string* extra) {
    // M60 telemetry: effective damage (and a KO) is known only after the
    // clamps below, so the emits bracket the whole body. `d` always refers
    // into `units`, so its index is recoverable for the event.
    const int hpBefore = d.hp;
    // M63 (Immovable): the first damaging hit simply glances off, once per
    // battle. Checked before god mode and Iron Will — nothing lands, so
    // neither fires, and an untouched bearer keeps its confusion.
    if (dmg > 0 && d.hp > 0 && d.firstHitImmune && !d.firstHitImmuneUsed) {
        d.firstHitImmuneUsed = true;
        if (extra != nullptr) {
            *extra += " The blow glances off " + d.name + "!";
        }
        return;
    }
#ifndef CRYSTAL_SHIPPING_BUILD
    // M53 debug god mode: a party unit never drops below 1 HP from a hit routed
    // through here. Same shape as Iron Will, but repeatable and party-wide; still
    // snaps the unit out of confusion like any real hit. Compiled out of shipping
    // builds and off by default, so the normal path below is untouched.
    if (debugPartyUnkillable && d.side == Side::Party && dmg > 0 && d.hp > 0 &&
        d.hp - dmg <= 0) {
        d.hp = 1;
        removeStatus(d, content::StatusType::Confusion);
        removeStatus(d, content::StatusType::Sleep);  // M75: a hit wakes a sleeper
        if (observer != nullptr && hpBefore > d.hp) {
            emitEvent(*this, {BattleEvent::Type::Damage, -1,
                              static_cast<int>(&d - units.data()), hpBefore - d.hp, false,
                              false, {}});
        }
        return;
    }
#endif
    // Iron Will (M36): a lethal blow leaves the holder at 1 HP, once per battle.
    bool ironWillFired = false;
    if (dmg > 0 && d.hp > 0 && d.hp - dmg <= 0 && d.ironWill && !d.ironWillUsed) {
        d.ironWillUsed = true;
        d.hp = 1;
        ironWillFired = true;
    } else {
        d.hp = std::max(0, d.hp - dmg);
    }
    // Confusion (M35): a hit snaps its bearer out of confusion. Single chokepoint
    // for all attack/skill damage (poison DoT does not route through here), so the
    // rule holds identically in live play and the Simulator. M75: the same hit
    // wakes a sleeper — and because the poison tick bypasses this chokepoint,
    // poison damage deliberately does NOT (the owner's rule).
    if (dmg > 0) {
        removeStatus(d, content::StatusType::Confusion);
        removeStatus(d, content::StatusType::Sleep);
    }
    // M63 (The Last Laugh): a unit felled here curses every living foe of the
    // fallen. At the chokepoint so every attack/skill/thorns/counter death
    // triggers it identically in both drivers (the poison tick has its own).
    if (hpBefore > 0 && d.hp == 0 && d.onDeathFoeDebuffPct > 0) {
        for (Combatant& u : units) {
            if (u.side != d.side && u.alive()) {
                addStatus(u, content::StatusType::AttackDown, d.onDeathFoeDebuffPct, 1);
                addStatus(u, content::StatusType::DefenseDown, d.onDeathFoeDebuffPct, 1);
            }
        }
        if (extra != nullptr) {
            *extra += " " + d.name + "'s last laugh saps every foe!";
        }
    }
    if (observer != nullptr && hpBefore > d.hp) {
        const int index = static_cast<int>(&d - units.data());
        emitEvent(*this, {BattleEvent::Type::Damage, -1, index, hpBefore - d.hp, false, false,
                          {}});
        if (d.hp == 0) {
            emitEvent(*this, {BattleEvent::Type::KO, -1, index, 0, false, false, {}});
        }
    }
    // M63 (Iron Constitution): the survival, then the surge — its own heal,
    // after the damage emit, so telemetry reconciles hit-then-heal exactly.
    if (ironWillFired && d.ironWillHealPct > 0) {
        const int before = d.hp;
        d.hp = std::min(d.maxHp, d.hp + std::max(1, d.maxHp * d.ironWillHealPct / 100));
        if (extra != nullptr && d.hp > before) {
            *extra += " " + d.name + " holds fast and surges back!";
        }
        if (observer != nullptr && d.hp > before) {
            emitEvent(*this, {BattleEvent::Type::Heal, -1,
                              static_cast<int>(&d - units.data()), d.hp - before, false, false,
                              {}});
        }
    }
}

std::string Battle::rallyOnKill(int killer) {
    // M63 (Standing Ovation): felling a foe rallies the killer's whole side.
    Combatant& k = units[static_cast<std::size_t>(killer)];
    if (k.onKillPartyAtkUpPct <= 0) {
        return "";
    }
    for (Combatant& u : units) {
        if (u.side == k.side && u.alive()) {
            addStatus(u, content::StatusType::AttackUp, k.onKillPartyAtkUpPct, 1);
        }
    }
    return " " + k.name + " takes a bow - the party rallies!";
}

bool Battle::sideAlive(Side s) const {
    for (const Combatant& c : units) {
        if (c.side == s && c.alive()) {
            return true;
        }
    }
    return false;
}

Outcome Battle::outcome() const {
    if (!sideAlive(Side::Enemy)) {
        return Outcome::Victory;
    }
    if (!sideAlive(Side::Party)) {
        return Outcome::Defeat;
    }
    return Outcome::Ongoing;
}

std::vector<int> Battle::aliveIndices(Side s) const {
    std::vector<int> out;
    for (std::size_t i = 0; i < units.size(); ++i) {
        if (units[i].side == s && units[i].alive()) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

long Battle::threatOf(int unit) const {
    if (unit < 0 || unit >= static_cast<int>(threat.size())) {
        return 0;
    }
    return threat[static_cast<std::size_t>(unit)];
}

void Battle::addThreat(int unit, long amount) {
    if (unit < 0 || unit >= static_cast<int>(threat.size())) {
        return;
    }
    threat[static_cast<std::size_t>(unit)] =
        std::max<long>(0, threat[static_cast<std::size_t>(unit)] + amount);
}

void Battle::beginRound() {
    for (long& t : threat) {
        t = t * 3 / 4;  // recency decay; keeps threat bounded and recent-weighted
    }
    // M36 passives at round start: Counter Attack rearms; Clarity regenerates MP.
    for (Combatant& u : units) {
        u.counteredThisRound = false;
        if (u.clarityMp > 0 && u.alive()) {
            u.mp = std::min(u.maxMp, u.mp + u.clarityMp);
        }
    }
}

std::uint64_t Battle::nextRandom(std::uint64_t salt) {
    ++rollCursor;
    return mix64(rngSeed ^ mix64(rollCursor) ^ mix64(salt));
}

int Battle::confusedTarget(int actor) {
    const Side s = units[static_cast<std::size_t>(actor)].side;
    std::vector<int> allies;  // own living side, including self (M35: uniform pick)
    for (std::size_t i = 0; i < units.size(); ++i) {
        if (units[i].side == s && units[i].alive()) {
            allies.push_back(static_cast<int>(i));
        }
    }
    if (allies.empty()) {
        return -1;
    }
    const std::uint64_t r = nextRandom(kSaltConfuse);
    return allies[static_cast<std::size_t>(r % allies.size())];
}

int Battle::bodyguardFor(int target) const {
    if (target < 0 || target >= static_cast<int>(units.size())) {
        return -1;
    }
    const Side side = units[static_cast<std::size_t>(target)].side;
    // Bodyguard only protects the lowest-HP living member of the side (lowest
    // index wins ties, so it is deterministic).
    int lowest = -1;
    for (std::size_t i = 0; i < units.size(); ++i) {
        if (units[i].side == side && units[i].alive() &&
            (lowest < 0 || units[i].hp < units[static_cast<std::size_t>(lowest)].hp)) {
            lowest = static_cast<int>(i);
        }
    }
    if (lowest != target) {
        return -1;
    }
    for (std::size_t i = 0; i < units.size(); ++i) {
        if (static_cast<int>(i) != target && units[i].side == side && units[i].alive() &&
            units[i].bodyguardPct > 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Battle::dealPhysical(int actor, int target, int baseDmg, std::string& extra) {
    Combatant& a = units[static_cast<std::size_t>(actor)];
    int dmg = baseDmg;
    if (a.firstStrike && !a.firstStrikeUsed) {  // First Strike (M36): first damaging action
        dmg = dmg * (100 + a.firstStrikeBonusPct) / 100;
        a.firstStrikeUsed = true;
    }
    if (a.keenSensesPct > 0 && hasAnyDebuff(units[static_cast<std::size_t>(target)])) {
        dmg = dmg * (100 + a.keenSensesPct) / 100;  // Keen Senses (M36)
    }
    // M63 (Executioner / Opportunist): composed after the M36 passives so the
    // multiplier order is fixed and documented.
    {
        const Combatant& tgt = units[static_cast<std::size_t>(target)];
        if (a.executePct > 0 && tgt.alive() && tgt.hp * 2 < tgt.maxHp) {
            dmg = dmg * (100 + a.executePct) / 100;
        }
        if (a.vsAfflictedPct > 0 && hasAnyDebuff(tgt)) {
            dmg = dmg * (100 + a.vsAfflictedPct) / 100;
        }
    }
    // M75 (Curse): everything the afflicted deals is halved — the LAST
    // attacker-side modifier, so no bonus escapes it.
    if (isCursed(a) && dmg > 0) {
        dmg = std::max(1, dmg / 2);
    }
    int toTarget = dmg;
    const int guard = bodyguardFor(target);
    if (guard >= 0) {  // Bodyguard (M36): the weakest ally's guard soaks a share
        Combatant& g = units[static_cast<std::size_t>(guard)];
        const int share = dmg * g.bodyguardPct / 100;
        if (share > 0) {
            applyDamage(g, share, &extra);
            toTarget = dmg - share;
            extra += " " + g.name + " shields " + units[static_cast<std::size_t>(target)].name +
                     " (" + std::to_string(share) + ").";
            if (!g.alive()) {
                extra += " " + g.name + " is KO'd!";
            }
        }
    }
    const bool targetStood = units[static_cast<std::size_t>(target)].alive();  // M63
    applyDamage(units[static_cast<std::size_t>(target)], toTarget, &extra);
    if (a.side == Side::Party) {
        addThreat(actor, dmg);  // total damage draws enmity (M28)
    }
    Combatant& t = units[static_cast<std::size_t>(target)];
    if (targetStood && !t.alive()) {
        extra += rallyOnKill(actor);  // M63 (Standing Ovation)
    }
    if (actor != target && toTarget > 0) {
        extra += fireHitTriggers(target, actor);  // M75 (every-Nth-hit triggers)
    }
    if (actor != target && t.thornsPct > 0 && toTarget > 0) {  // Thorns (M36)
        const int reflect = toTarget * t.thornsPct / 100;
        if (reflect > 0) {
            const bool actorStood = a.alive();  // M63
            applyDamage(a, reflect, &extra);
            extra += " " + a.name + " takes " + std::to_string(reflect) + " thorns damage.";
            if (!a.alive()) {
                extra += " " + a.name + " is KO'd!";
                if (actorStood) {
                    extra += rallyOnKill(target);  // M63: thorns fell the attacker
                }
            }
        }
    }
    if (a.lifedrinkPct > 0 && dmg > 0 && a.alive()) {  // Lifedrink (M36)
        const int heal = dmg * a.lifedrinkPct / 100;
        if (heal > 0) {
            const int hpBefore = a.hp;  // M60 telemetry (effective gain)
            applyHeal(a, heal);
            if (observer != nullptr && a.hp > hpBefore) {
                emitEvent(*this, {BattleEvent::Type::Heal, actor, actor, a.hp - hpBefore, false,
                                  false, {}});
            }
            extra += " " + a.name + " drains " + std::to_string(heal) + " HP.";
        }
    }
    if (actor != target && t.alive() && t.counterAttack && !t.counteredThisRound &&
        a.alive()) {  // Counter Attack (M36): one basic retaliation per round
        t.counteredThisRound = true;
        // M48: a counter IS a basic attack, so it swings the counter-attacker's
        // own weapon element. M75: a cursed counter-attacker's retaliation is
        // halved like everything else it deals.
        int cdmg = physicalDamage(t, a, 0, t.weaponElement);
        if (isCursed(t) && cdmg > 0) {
            cdmg = std::max(1, cdmg / 2);
        }
        const bool actorStood = a.alive();  // M63
        applyDamage(a, cdmg, &extra);
        if (t.side == Side::Party) {
            addThreat(target, cdmg);
        }
        extra += " " + t.name + " counters " + a.name + " for " + std::to_string(cdmg) + ".";
        if (!a.alive()) {
            extra += " " + a.name + " is KO'd!";
            if (actorStood) {
                extra += rallyOnKill(target);  // M63: the counter fells the attacker
            }
        }
    }
    return toTarget;
}

int Battle::dealMagic(int actor, int target, int baseDmg, std::string& extra) {
    Combatant& a = units[static_cast<std::size_t>(actor)];
    int dmg = baseDmg;
    if (a.firstStrike && !a.firstStrikeUsed) {
        dmg = dmg * (100 + a.firstStrikeBonusPct) / 100;
        a.firstStrikeUsed = true;
    }
    if (a.keenSensesPct > 0 && hasAnyDebuff(units[static_cast<std::size_t>(target)])) {
        dmg = dmg * (100 + a.keenSensesPct) / 100;
    }
    // M63 (Executioner / Opportunist): the same fixed multiplier order as the
    // physical path.
    {
        const Combatant& tgt = units[static_cast<std::size_t>(target)];
        if (a.executePct > 0 && tgt.alive() && tgt.hp * 2 < tgt.maxHp) {
            dmg = dmg * (100 + a.executePct) / 100;
        }
        if (a.vsAfflictedPct > 0 && hasAnyDebuff(tgt)) {
            dmg = dmg * (100 + a.vsAfflictedPct) / 100;
        }
    }
    // M75 (Curse): the same last-modifier halving as the physical path.
    if (isCursed(a) && dmg > 0) {
        dmg = std::max(1, dmg / 2);
    }
    int toTarget = dmg;
    const int guard = bodyguardFor(target);
    if (guard >= 0) {  // Bodyguard soaks magic too (any damage aimed at the weakest ally)
        Combatant& g = units[static_cast<std::size_t>(guard)];
        const int share = dmg * g.bodyguardPct / 100;
        if (share > 0) {
            applyDamage(g, share, &extra);
            toTarget = dmg - share;
            extra += " " + g.name + " shields " + units[static_cast<std::size_t>(target)].name +
                     " (" + std::to_string(share) + ").";
            if (!g.alive()) {
                extra += " " + g.name + " is KO'd!";
            }
        }
    }
    const bool targetStood = units[static_cast<std::size_t>(target)].alive();  // M63
    applyDamage(units[static_cast<std::size_t>(target)], toTarget, &extra);
    if (a.side == Side::Party) {
        addThreat(actor, dmg);
    }
    if (targetStood && !units[static_cast<std::size_t>(target)].alive()) {
        extra += rallyOnKill(actor);  // M63 (Standing Ovation)
    }
    if (actor != target && toTarget > 0) {
        extra += fireHitTriggers(target, actor);  // M75 (every-Nth-hit triggers)
    }
    return toTarget;
}

bool canCast(const Combatant& c, const content::SkillDef& skill) {
    return !(isSilenced(c) && skill.mpCost > 0);
}

int mpCostFor(const Combatant& c, const content::SkillDef& skill) {
    // M75 (Curse): every skill costs its bearer double. One rule for the
    // battle menu, both AIs and useSkill's deduction, so a cursed caster can
    // never pick a skill it cannot pay for in one driver but not the other.
    return isCursed(c) ? skill.mpCost * 2 : skill.mpCost;
}

std::string Battle::beginUnitTurn(int actor) {
    if (actor < 0 || actor >= static_cast<int>(units.size())) {
        return "";
    }
    // M49 revive clock + M75 turn-start triggers, composed in the one per-turn
    // seam both drivers already call. Triggers fire even on a turn a status
    // then takes away (a sleeping dragon still rages at half HP) — simple,
    // deterministic, and identical in the Simulator and live play.
    std::string log = reviveCourtRule(actor);
    log += fireTurnTriggers(actor);
    if (!log.empty() && log.front() == ' ') {
        log.erase(0, 1);  // trigger fragments carry a leading separator
    }
    return log;
}

std::string Battle::reviveCourtRule(int actor) {
    Combatant& a = units[static_cast<std::size_t>(actor)];
    if (a.reviveMinionTurns <= 0) {
        return "";  // every unit but the King
    }
    // "His court" is every non-boss unit on his own side — in a boss battle
    // those are exactly the boss's authored minions, so the rule needs no id
    // branching and works for any boss that ever carries the clock.
    std::vector<int> court;
    bool anyAlive = false;
    for (std::size_t i = 0; i < units.size(); ++i) {
        const Combatant& u = units[i];
        if (u.side != a.side || u.isBoss || static_cast<int>(i) == actor) {
            continue;
        }
        court.push_back(static_cast<int>(i));
        anyAlive = anyAlive || u.alive();
    }
    if (court.empty()) {
        return "";  // a king with no court has nothing to count toward
    }
    if (anyAlive) {
        a.reviveMinionCounter = 0;  // one still stands: the clock restarts
        return "";
    }
    ++a.reviveMinionCounter;
    if (a.reviveMinionCounter < a.reviveMinionTurns) {
        return "";
    }
    a.reviveMinionCounter = 0;  // and it will happen again, and again
    for (int ci : court) {
        Combatant& c = units[static_cast<std::size_t>(ci)];
        const bool wasDown = !c.alive();
        c.hp = c.maxHp;
        c.statuses.clear();  // raised whole, not raised wounded
        if (wasDown) {  // M60 telemetry
            emitEvent(*this, {BattleEvent::Type::Revive, actor, ci, c.maxHp, false, false, {}});
        }
    }
    return a.name + " strikes the floor: \"RISE.\" The court stands again, unmarked.";
}

std::string Battle::fireTurnTriggers(int actor) {
    Combatant& a = units[static_cast<std::size_t>(actor)];
    if (a.triggers.empty() || !a.alive()) {
        return "";
    }
    std::string log;
    for (TriggerRule& tr : a.triggers) {
        bool fire = false;
        switch (tr.when) {
            case content::TriggerWhen::EveryNthHitTaken:
                break;  // hit-reactive; fires inside dealPhysical/dealMagic
            case content::TriggerWhen::FirstTimeHpBelowPct:
                if (!tr.fired && tr.threshold > 0 && a.hp * 100 <= a.maxHp * tr.threshold) {
                    tr.fired = true;
                    fire = true;
                }
                break;
            case content::TriggerWhen::EveryNthOwnTurn:
                ++tr.counter;
                if (tr.threshold > 0 && tr.counter % tr.threshold == 0) {
                    fire = true;
                }
                break;
            case content::TriggerWhen::FirstTimeAllyFelled:
                if (!tr.fired) {
                    for (std::size_t i = 0; i < units.size(); ++i) {
                        const Combatant& u = units[i];
                        if (static_cast<int>(i) != actor && u.side == a.side && !u.alive()) {
                            tr.fired = true;
                            fire = true;
                            break;
                        }
                    }
                }
                break;
            case content::TriggerWhen::None:
                break;
        }
        if (fire) {
            log += applyTriggerAction(actor, tr, -1);
        }
    }
    return log;
}

std::string Battle::fireHitTriggers(int target, int attacker) {
    Combatant& t = units[static_cast<std::size_t>(target)];
    if (t.triggers.empty() || !t.alive()) {
        return "";  // a felled unit reacts to nothing
    }
    // The counter only ever advances for a unit that carries triggers, and only
    // for DELIBERATE connecting hits routed through dealPhysical/dealMagic —
    // thorns, counters and poison ticks are consequences, not attacks.
    ++t.hitsTaken;
    std::string log;
    for (TriggerRule& tr : t.triggers) {
        if (tr.when != content::TriggerWhen::EveryNthHitTaken || tr.threshold <= 0) {
            continue;
        }
        if (t.hitsTaken % tr.threshold == 0) {
            log += applyTriggerAction(target, tr, attacker);
        }
    }
    return log;
}

std::string Battle::applyTriggerAction(int owner, TriggerRule& tr, int attacker) {
    Combatant& o = units[static_cast<std::size_t>(owner)];
    // M77: the sleep manners cover TRIGGER-borne stuns too — a `noStunWhile-
    // AllFoesSleep` owner (the Deadly Duck) lets the moment pass while the
    // whole opposing side sleeps (an every-Nth counter simply misses that
    // beat). No pre-M77 content carries any trigger, so every earlier battle
    // resolves byte-identically (recorded in the M77 note).
    if (tr.status == content::StatusType::Stunned && o.noStunWhileAllFoesSleep) {
        const Side foe = o.side == Side::Party ? Side::Enemy : Side::Party;
        bool anyAwake = false;
        for (int fi : aliveIndices(foe)) {
            if (!isAsleep(units[static_cast<std::size_t>(fi)])) {
                anyAwake = true;
                break;
            }
        }
        if (!anyAwake) {
            return "";
        }
    }
    std::string log = tr.text.empty() ? std::string() : " " + tr.text;
    switch (tr.action) {
        case content::TriggerDo::StatusSelf:
            if (!isImmuneTo(o, tr.status)) {
                addStatus(o, tr.status, statusMagnitudeFor(o, tr.status, tr.magnitude),
                          tr.duration);
                log += " " + o.name + ": " + statusLabel(tr.status) + ".";
            }
            break;
        case content::TriggerDo::StatusAttacker:
            if (attacker >= 0 && attacker < static_cast<int>(units.size())) {
                Combatant& atk = units[static_cast<std::size_t>(attacker)];
                if (atk.alive() && !isImmuneTo(atk, tr.status)) {
                    addStatus(atk, tr.status, statusMagnitudeFor(o, tr.status, tr.magnitude),
                              tr.duration);
                    log += " " + atk.name + ": " + statusLabel(tr.status) + ".";
                }
            }
            break;
        case content::TriggerDo::StatusAllFoes: {
            const Side foe = o.side == Side::Party ? Side::Enemy : Side::Party;
            bool any = false;
            for (int fi : aliveIndices(foe)) {
                Combatant& f = units[static_cast<std::size_t>(fi)];
                if (isImmuneTo(f, tr.status)) {
                    continue;
                }
                addStatus(f, tr.status, statusMagnitudeFor(o, tr.status, tr.magnitude),
                          tr.duration);
                any = true;
            }
            if (any) {
                log += " Every foe: " + std::string(statusLabel(tr.status)) + ".";
            }
            break;
        }
        case content::TriggerDo::StatusBoss:
            for (std::size_t i = 0; i < units.size(); ++i) {
                Combatant& b = units[i];
                if (b.side == o.side && b.isBoss && b.alive()) {
                    if (!isImmuneTo(b, tr.status)) {
                        addStatus(b, tr.status,
                                  statusMagnitudeFor(o, tr.status, tr.magnitude), tr.duration);
                        log += " " + b.name + ": " + statusLabel(tr.status) + ".";
                    }
                    break;
                }
            }
            break;
        case content::TriggerDo::ScaleStatsSelf: {
            const auto scale = [](int v, int pct) {
                return (v > 0 && pct != 100) ? std::max(1, v * pct / 100) : v;
            };
            o.stats.attack = scale(o.stats.attack, tr.scaleAttackPct);
            o.stats.magic = scale(o.stats.magic, tr.scaleMagicPct);
            o.stats.defense = scale(o.stats.defense, tr.scaleDefensePct);
            o.stats.speed = scale(o.stats.speed, tr.scaleSpeedPct);
            if (tr.text.empty()) {
                log += " " + o.name + "'s might swells!";
            }
            break;
        }
        case content::TriggerDo::SummonCloneSelf:
            // The clone was prebuilt DEAD at buildBattle (the roster never
            // grows mid-battle); raising it is all a summon is.
            for (std::size_t i = 0; i < units.size(); ++i) {
                Combatant& c = units[i];
                if (c.side == o.side && c.summonSlot && !c.alive()) {
                    c.hp = c.maxHp;
                    c.statuses.clear();
                    emitEvent(*this, {BattleEvent::Type::Revive, owner, static_cast<int>(i),
                                      c.maxHp, false, false, {}});
                    log += " " + c.name + " rises!";
                    break;
                }
            }
            break;
        case content::TriggerDo::DrainFoeMp: {
            const Side foe = o.side == Side::Party ? Side::Enemy : Side::Party;
            bool any = false;
            for (int fi : aliveIndices(foe)) {
                Combatant& f = units[static_cast<std::size_t>(fi)];
                const int loss = f.mp * std::min(tr.mpDrainPct, 100) / 100;
                if (loss > 0) {
                    f.mp -= loss;
                    any = true;
                }
            }
            if (any && tr.text.empty()) {
                log += " " + o.name + " devours its foes' spirit - MP drains away!";
            } else if (any) {
                log += " MP drains away!";
            }
            break;
        }
        case content::TriggerDo::None:
            break;
    }
    return log;
}

void Battle::clearActionMarks() {
    lastMissed.clear();
    lastWeak.clear();
    lastImmune.clear();
}

void Battle::clearGuard(int unit) {
    if (unit >= 0 && unit < static_cast<int>(units.size())) {
        units[static_cast<std::size_t>(unit)].guarding = false;
        units[static_cast<std::size_t>(unit)].intercepting = false;
    }
}

std::string Battle::tickStatuses(int unit) {
    clearActionMarks();
    std::string log;
    Combatant& c = units[static_cast<std::size_t>(unit)];

    // Commander rally (M20): once, at the start of its turn below half HP,
    // fallen minions rise at half strength. Runs here because both the
    // battle screen and the simulator call tickStatuses at turn start, so
    // play and simulation stay identical.
    if (c.ralliesMinions && !c.rallied && c.alive() && c.hp * 2 < c.maxHp) {
        c.rallied = true;
        int raised = 0;
        for (Combatant& u : units) {
            if (&u != &c && u.side == c.side && !u.isBoss && !u.alive()) {
                u.hp = std::max(1, u.maxHp / 2);
                ++raised;
            }
        }
        if (raised > 0) {
            log += c.name + " rallies the fallen - " + std::to_string(raised) +
                   (raised == 1 ? " minion rises anew!" : " minions rise anew!");
        }
    }

    if (c.statuses.empty()) {
        return log;
    }
    for (StatusInstance& s : c.statuses) {
        if (s.type == content::StatusType::Poison && c.hp > 0) {
            const int dmg = s.magnitude * kPoisonDamageMult;  // M35: poison deals 2x
            // Poison bypasses applyDamage, so the M53 god-mode clamp is repeated
            // here (the second and last clamp site). floor stays 0 in every
            // shipping/sim/test path, so this is byte-identical when off.
            int floor = 0;
#ifndef CRYSTAL_SHIPPING_BUILD
            if (debugPartyUnkillable && c.side == Side::Party) {
                floor = 1;
            }
#endif
            const int hpBefore = c.hp;  // M60: the tick bypasses applyDamage's emit too
            c.hp = std::max(floor, c.hp - dmg);
            if (observer != nullptr && hpBefore > c.hp) {
                emitEvent(*this, {BattleEvent::Type::Damage, -1, unit, hpBefore - c.hp, true,
                                  false, {}});
                if (c.hp == 0) {
                    emitEvent(*this, {BattleEvent::Type::KO, -1, unit, 0, false, false, {}});
                }
            }
            log += c.name + " takes " + std::to_string(dmg) + " poison damage.";
            if (!c.alive()) {
                log += " " + c.name + " is KO'd!";
                // M63 (The Last Laugh): the poison tick bypasses applyDamage,
                // so its death repeats the on-death curse here — the second
                // and last site, the god-mode-clamp precedent.
                if (c.onDeathFoeDebuffPct > 0) {
                    for (Combatant& u : units) {
                        if (u.side != c.side && u.alive()) {
                            addStatus(u, content::StatusType::AttackDown,
                                      c.onDeathFoeDebuffPct, 1);
                            addStatus(u, content::StatusType::DefenseDown,
                                      c.onDeathFoeDebuffPct, 1);
                        }
                    }
                    log += " " + c.name + "'s last laugh saps every foe!";
                }
            }
        }
        --s.turns;
    }
    std::vector<StatusInstance> kept;
    for (const StatusInstance& s : c.statuses) {
        if (s.turns > 0) {
            kept.push_back(s);
        }
    }
    c.statuses = std::move(kept);
    return log;
}

std::string Battle::attack(int actor, int target) {
    clearActionMarks();
    // M60 telemetry: a basic attack is an Action with an empty id.
    emitEvent(*this, {BattleEvent::Type::Action, actor, target, 0, false, false, {}});
    // M45: a class whose basic attack hits every foe sweeps instead of striking
    // one — except while confused, when it lashes at a single member of its own
    // side like anyone else.
    const Combatant& self = units[static_cast<std::size_t>(actor)];
    if (self.attackHitsAll && !isConfused(self)) {
        return attackAll(actor);
    }
    // M63 (Double Nock): two independent strikes — each with its own to-hit
    // roll, passives, and riders — the second at the milestone's strength,
    // and only while both parties still stand. A confused archer lashes out
    // once like anyone else.
    if (self.doubleStrikePct > 0 && !isConfused(self)) {
        std::string log = attackOne(actor, target);
        if (units[static_cast<std::size_t>(actor)].alive() &&
            units[static_cast<std::size_t>(target)].alive()) {
            std::vector<int> missed = lastMissed;
            std::vector<int> weak = lastWeak;
            std::vector<int> immune = lastImmune;
            log += " " + attackOne(actor, target, self.doubleStrikePct);
            missed.insert(missed.end(), lastMissed.begin(), lastMissed.end());
            weak.insert(weak.end(), lastWeak.begin(), lastWeak.end());
            immune.insert(immune.end(), lastImmune.begin(), lastImmune.end());
            lastMissed = std::move(missed);
            lastWeak = std::move(weak);
            lastImmune = std::move(immune);
        }
        return log;
    }
    return attackOne(actor, target);
}

std::string Battle::attackOne(int actor, int target, int scalePct) {
    clearActionMarks();
    // Confusion (M35): a confused unit swings at a seeded random member of its own
    // side instead. (Draws from the shared roll stream, so live play and the
    // Simulator reproduce it identically.)
    const bool confused = isConfused(units[static_cast<std::size_t>(actor)]);
    if (confused) {
        const int ct = confusedTarget(actor);
        if (ct >= 0) {
            target = ct;
        }
    }
    target = redirectTarget(units, actor, target);  // intercept redirects enemy hits (M28)
    Combatant& a = units[static_cast<std::size_t>(actor)];
    Combatant& t = units[static_cast<std::size_t>(target)];
    const bool opener = a.rushOpener && !a.actedOnce;
    a.actedOnce = true;
    const std::string verb = confused ? " is confused and attacks " : " attacks ";
    // Blind/Evasion physical miss (M35/M36): one seeded roll, only when it applies.
    const int missPct = physicalMissPct(a, t);
    if (missPct > 0 && static_cast<int>(nextRandom(kSaltBlind) % 100) < missPct) {
        lastMissed.push_back(target);
        std::string miss = a.name + verb + t.name + " but misses!";
        if (a.enrages && !a.enrageAnnounced && a.hp * 2 < a.maxHp) {
            a.enrageAnnounced = true;
            miss = a.name + " flies into a rage! " + miss;
        }
        return miss;
    }
    // M48: a basic attack carries the attacker's weapon element (None for every
    // enemy, which has no weapon, and for anyone holding untagged steel).
    const content::Element element = a.weaponElement;
    const int mod = elementModifier(t, element);
    // M85: the EFFECTIVE modifier the damage path used — an intrinsic element
    // resolves an immunity at neutral, so the mark, the log line and the
    // status rider must all agree with the damage and treat it as a plain
    // hit. The weak mark stays on the raw modifier (the M63 override changes
    // the percent, not the fact of the weakness).
    const int effMod = attackerElementMod(a, t, element);
    int base = physicalDamage(a, t, 0, element);
    if (a.basicAttackPct > 0) {
        base = base * (100 + a.basicAttackPct) / 100;  // M63 (Heavy Swing family)
    }
    if (opener) {
        base *= 2;  // Rush: opening fury
    }
    if (scalePct != 100 && base > 0) {
        base = std::max(1, base * scalePct / 100);  // M63 (second arrow / sweep)
    }
    std::string extra;
    const int dealt = dealPhysical(actor, target, base, extra);  // M36 passive effects
    std::string log = a.name + verb + t.name + " for " + std::to_string(dealt) + ".";
    // M75: the battle log names an elemental swing (owner decision 2026-08-05).
    if (element != content::Element::None) {
        log += " (" + std::string(content::elementDisplayName(element)) + ")";
    }
    if (opener) {
        log += " (opening fury!)";
    }
    if (effMod == kElementImmunePct) {
        // Not a miss: the blow lands and does nothing. Recorded for the float
        // and said plainly in the log, so the two never disagree.
        lastImmune.push_back(target);
        log += " " + t.name + " is immune!";
    } else if (mod == kElementWeakPct) {
        lastWeak.push_back(target);
        log += " It is devastating!";
    }
    if (!t.alive()) {
        log += " " + t.name + " is KO'd!";
    }
    log += extra;
    if (effMod != kElementImmunePct) {
        log += applyAttackStatuses(actor, target);  // M45 (the Dragon's bite)
    }
    if (a.enrages && !a.enrageAnnounced && a.hp * 2 < a.maxHp) {
        a.enrageAnnounced = true;
        log = a.name + " flies into a rage! " + log;
    }
    return log;
}

std::string Battle::applyAttackStatuses(int actor, int target) {
    // M45: statuses a class's BASIC attack carries (the Dragon's poison + blind),
    // applied only on a connecting hit and only to a living target. Empty for
    // every unit that carries none, so pre-M45 attacks are byte-identical.
    Combatant& a = units[static_cast<std::size_t>(actor)];
    Combatant& t = units[static_cast<std::size_t>(target)];
    if (a.attackStatuses.empty() || !t.alive()) {
        return "";
    }
    std::string log;
    for (const StatusInstance& s : a.attackStatuses) {
        if (s.type == content::StatusType::None || isImmuneTo(t, s.type)) {
            continue;
        }
        // M63 rider bonus; M75 poison scaling (statusMagnitudeFor).
        addStatus(t, s.type, statusMagnitudeFor(a, s.type, s.magnitude), s.turns,
                  a.statusTurnsBonus);
        log += " " + t.name + ": " + statusLabel(s.type) + ".";
    }
    return log;
}

std::string Battle::attackAll(int actor) {
    // M45: an AoE basic attack (the Dragon) resolves as one independent strike per
    // living foe — its own to-hit roll, its own passive interactions, its own
    // statuses — so it reads and resolves exactly like the single-target attack it
    // is repeated from.
    const Side foe =
        units[static_cast<std::size_t>(actor)].side == Side::Party ? Side::Enemy : Side::Party;
    const std::vector<int> targets = aliveIndices(foe);
    if (targets.empty()) {
        return units[static_cast<std::size_t>(actor)].name + " finds nothing to strike.";
    }
    std::string log = units[static_cast<std::size_t>(actor)].name + " sweeps every foe!";
    std::vector<int> missed;
    std::vector<int> weak;
    std::vector<int> immune;
    for (int ti : targets) {
        if (!units[static_cast<std::size_t>(ti)].alive()) {
            continue;  // felled by an earlier strike in this same sweep
        }
        // M63 (Rain of Arrows): a milestone sweep strikes at reduced strength;
        // a class-authored sweep (the Dragon, the Duck) stays full.
        const int scale = units[static_cast<std::size_t>(actor)].sweepScalePct;
        log += " " + attackOne(actor, ti, scale > 0 ? scale : 100);
        // attackOne clears the marks per strike, so the sweep accumulates them.
        missed.insert(missed.end(), lastMissed.begin(), lastMissed.end());
        weak.insert(weak.end(), lastWeak.begin(), lastWeak.end());
        immune.insert(immune.end(), lastImmune.begin(), lastImmune.end());
    }
    lastMissed = std::move(missed);  // every mark in the sweep, for the floaters
    lastWeak = std::move(weak);
    lastImmune = std::move(immune);
    return log;
}

std::vector<int> Battle::resolveTargets(const content::SkillDef& skill, int actor,
                                        int primaryTarget) const {
    const Side me = units[static_cast<std::size_t>(actor)].side;
    const Side foe = me == Side::Party ? Side::Enemy : Side::Party;
    switch (skill.target) {
        case content::SkillTarget::SingleEnemy:
        case content::SkillTarget::SingleAlly:
            return {primaryTarget};
        case content::SkillTarget::AllEnemies:
            return aliveIndices(foe);
        case content::SkillTarget::AllAllies:
            return aliveIndices(me);
        case content::SkillTarget::Self:
            return {actor};
    }
    return {primaryTarget};
}

std::string Battle::useSkill(int actor, int primaryTarget, const content::SkillDef& skill) {
    clearActionMarks();
    // Silence (M35): MP-cost skills are blocked (0-MP skills, items, and basic
    // attacks are fine). Guarded here too, so no caller can slip a silenced cast
    // past the battle menu / AI filters and desync live play from the Simulator.
    if (isSilenced(units[static_cast<std::size_t>(actor)]) && skill.mpCost > 0) {
        return units[static_cast<std::size_t>(actor)].name + " is silenced and cannot use " +
               skill.name + "!";
    }
    primaryTarget = redirectTarget(units, actor, primaryTarget);  // M28 intercept
    // M60 telemetry: the cast is real from here on (silence already returned).
    emitEvent(*this, {BattleEvent::Type::Action, actor, primaryTarget, 0, false, false,
                      skill.id});
    Combatant& a = units[static_cast<std::size_t>(actor)];
    a.mp = std::max(0, a.mp - mpCostFor(a, skill));  // M75: a Curse doubles the cost
    const bool opener = a.rushOpener && !a.actedOnce;
    a.actedOnce = true;
    // Sorcerer empowerment: +25% magic damage per fallen same-side ally.
    int empowerPct = 0;
    if (a.empowersOnAllyFall) {
        for (const Combatant& u : units) {
            if (&u != &a && u.side == a.side && !u.alive()) {
                empowerPct += 25;
            }
        }
    }
    std::string log = a.name + " uses " + skill.name + ".";
    // M75: the battle log names an attack's element (owner decision 2026-08-05).
    if (skill.element != content::Element::None) {
        log += " (" + std::string(content::elementDisplayName(skill.element)) + ")";
    }
    if (empowerPct > 0) {
        log += " (empowered +" + std::to_string(empowerPct) + "%)";
    }

    // Enmity-control effects (M28): manipulate the threat model / intercept.
    switch (skill.controlEffect) {
        case content::SkillEffect::Taunt: {
            long maxParty = 0;
            for (std::size_t i = 0; i < units.size(); ++i) {
                if (units[i].side == Side::Party) {
                    maxParty = std::max(maxParty, threatOf(static_cast<int>(i)));
                }
            }
            if (actor >= 0 && actor < static_cast<int>(threat.size())) {
                threat[static_cast<std::size_t>(actor)] = maxParty + 150;
            }
            log += " Foes are goaded into targeting " + a.name + "!";
            // M63 (Intimidating Taunt): the goading also saps every foe.
            if (a.tauntDebuffPct > 0) {
                const Side foeSide = a.side == Side::Party ? Side::Enemy : Side::Party;
                bool any = false;
                for (Combatant& u : units) {
                    if (u.side == foeSide && u.alive()) {
                        addStatus(u, content::StatusType::AttackDown, a.tauntDebuffPct, 1,
                                  a.statusTurnsBonus);
                        any = true;
                    }
                }
                if (any) {
                    log += " The foes falter: ATK-.";
                }
            }
            break;
        }
        case content::SkillEffect::Fade:
            if (actor >= 0 && actor < static_cast<int>(threat.size())) {
                threat[static_cast<std::size_t>(actor)] = threatOf(actor) / 4;
            }
            log += " " + a.name + " slips out of the enemy's focus.";
            break;
        case content::SkillEffect::Intercept:
            a.intercepting = true;
            log += " " + a.name + " stands ready to shield the party.";
            break;
        case content::SkillEffect::Cleanse:
            // Applied per ally target in the loop below (works with single_ally
            // and all_allies), so a heal skill can double as a cure (M35).
            break;
        case content::SkillEffect::BreakReflect:
        case content::SkillEffect::Uncurse:
            // M75: both applied per target in the loop below.
            break;
        case content::SkillEffect::None:
            break;
    }

    for (int ti : resolveTargets(skill, actor, primaryTarget)) {
        if (ti < 0 || ti >= static_cast<int>(units.size())) {
            continue;
        }
        Combatant& t = units[static_cast<std::size_t>(ti)];
        // M75 (Reflect): a hostile magic-category skill aimed at a reflecting
        // foe bounces back onto its caster — damage, MP drain and status rider
        // alike. No roll is taken (the mirror is absolute, so `rollCursor`
        // never moves for it) and the ward never sees the spell. Deliberately
        // checked before the ward: a mirror sits in front of everything.
        if (skill.category == content::SkillCategory::Magic && t.side != a.side &&
            hasReflect(t) && t.alive()) {
            log += " " + t.name + " turns the spell back!";
            int base = magicDamage(a, a, skill.power, skill.element);
            base = base * (100 + empowerPct) / 100;
            if (a.magicSkillPct > 0) {
                base = base * (100 + a.magicSkillPct) / 100;  // M63 (Arcane Edge)
            }
            std::string extra;
            const int dealt = dealMagic(actor, actor, base, extra);
            log += " " + a.name + " takes " + std::to_string(dealt) + ".";
            if (!a.alive()) {
                log += " " + a.name + " is KO'd!";
            }
            log += extra;
            if (skill.mpDamagePct > 0 && dealt > 0 && a.mp > 0) {  // the drain bounces too
                const int mpLoss = std::min(a.mp, dealt * skill.mpDamagePct / 100);
                if (mpLoss > 0) {
                    a.mp -= mpLoss;
                    log += " " + a.name + " loses " + std::to_string(mpLoss) + " MP.";
                }
            }
            if (skill.statusEffect != content::StatusType::None && a.alive() &&
                !isImmuneTo(a, skill.statusEffect)) {
                addStatus(a, skill.statusEffect,
                          statusMagnitudeFor(a, skill.statusEffect, skill.statusMagnitude),
                          skill.statusDuration);
                log += " " + a.name + ": " + statusLabel(skill.statusEffect) + ".";
            }
            continue;  // nothing of this skill reached the mirror-bearer
        }
        // Blind/Evasion physical miss, or Spell Ward magic fizzle (M35/M36): one
        // seeded roll per target, only when a status/passive gates it.
        if (skill.category == content::SkillCategory::Physical) {
            const int missPct = physicalMissPct(a, t);
            if (missPct > 0 && static_cast<int>(nextRandom(kSaltBlind) % 100) < missPct) {
                lastMissed.push_back(ti);
                log += " " + t.name + ": miss!";
                continue;  // no damage and no status on a miss
            }
        } else if (skill.category == content::SkillCategory::Magic && t.spellWardPct > 0 &&
                   static_cast<int>(nextRandom(kSaltWard) % 100) < t.spellWardPct) {
            log += " " + t.name + " wards the spell!";
            continue;  // Spell Ward (M36): the spell fizzles - no damage or status
        }
        // M75 (BreakReflect): the mirror shatters before anything else of this
        // skill lands. Forbidden on magic skills by the loader — a magic
        // breaker would bounce off the very mirror it came to break.
        if (skill.controlEffect == content::SkillEffect::BreakReflect && t.alive() &&
            t.side != a.side && hasStatus(t, content::StatusType::Reflect)) {
            removeStatus(t, content::StatusType::Reflect);
            log += " " + t.name + "'s mirror shatters!";
        }
        // M48: the skill's own element decides the affinity, for every category —
        // a Heal or Support skill is element-neutral in practice because every
        // shipped one is authored `none`.
        const int mod = elementModifier(t, skill.element);
        switch (skill.category) {
            case content::SkillCategory::Physical: {
                int base = physicalDamage(a, t, skill.power, skill.element);
                if (opener) {
                    base *= 2;  // Rush: opening fury
                }
                std::string extra;
                const int dealt = dealPhysical(actor, ti, base, extra);  // M36 passives
                log += " " + t.name + " takes " + std::to_string(dealt) + ".";
                if (!t.alive()) {
                    log += " " + t.name + " is KO'd!";
                }
                log += extra;
                if (skill.mpDamagePct > 0 && dealt > 0 && t.mp > 0) {  // M75 MP damage
                    const int mpLoss = std::min(t.mp, dealt * skill.mpDamagePct / 100);
                    if (mpLoss > 0) {
                        t.mp -= mpLoss;
                        log += " " + t.name + " loses " + std::to_string(mpLoss) + " MP.";
                    }
                }
                break;
            }
            case content::SkillCategory::Magic: {
                int base = magicDamage(a, t, skill.power, skill.element);
                base = base * (100 + empowerPct) / 100;
                if (a.magicSkillPct > 0) {
                    base = base * (100 + a.magicSkillPct) / 100;  // M63 (Arcane Edge)
                }
                if (a.aoeSpellPct > 0 && skill.target == content::SkillTarget::AllEnemies) {
                    base = base * (100 + a.aoeSpellPct) / 100;  // M63 (Devastation)
                }
                if (opener) {
                    base *= 2;  // Rush: opening fury
                }
                std::string extra;
                const int dealt = dealMagic(actor, ti, base, extra);  // M36 passives
                log += " " + t.name + " takes " + std::to_string(dealt) + ".";
                if (!t.alive()) {
                    log += " " + t.name + " is KO'd!";
                }
                log += extra;
                if (skill.mpDamagePct > 0 && dealt > 0 && t.mp > 0) {  // M75 MP damage
                    const int mpLoss = std::min(t.mp, dealt * skill.mpDamagePct / 100);
                    if (mpLoss > 0) {
                        t.mp -= mpLoss;
                        log += " " + t.name + " loses " + std::to_string(mpLoss) + " MP.";
                    }
                }
                break;
            }
            case content::SkillCategory::Heal: {
                if (t.alive()) {
                    // M62 (rules v13): a pure cleanse authored as a heal —
                    // power 0 + the cleanse control (Purify) — heals nothing.
                    // The description always said so, but the heal formula's
                    // magic/2 term leaked through. A cleanse with real power
                    // (Generous Mending) still heals; the cleanse itself runs
                    // below either way.
                    // M63 (Purifying Light): the chosen milestone deliberately
                    // restores the pre-M62 behaviour for this caster alone.
                    const bool pureCleanse =
                        skill.power == 0 &&
                        skill.controlEffect == content::SkillEffect::Cleanse &&
                        !a.purifyHeals;
                    if (!pureCleanse) {
                        int amt = healValue(a, skill.power);
                        if (a.healCastPct > 0) {
                            amt = amt * (100 + a.healCastPct) / 100;  // M63 (Devotion)
                        }
                        const int hpBefore = t.hp;  // M60: record the EFFECTIVE gain
                        applyHeal(t, amt);
                        if (observer != nullptr && t.hp > hpBefore) {
                            emitEvent(*this, {BattleEvent::Type::Heal, actor, ti,
                                              t.hp - hpBefore, false, false, {}});
                        }
                        if (a.side == Side::Party) {
                            addThreat(actor, amt);  // healing draws enmity too (M28)
                        }
                        log += " " + t.name + " recovers " + std::to_string(amt) + " HP.";
                    }
                } else if (skill.reviveHpPct > 0) {
                    // M43: a revive-capable heal (Renew) raises a KO'd ally at a
                    // fixed share of its max HP - the skill's own power never
                    // enters, so reviving is an emergency, not a heal.
                    // M63 (Blessed Renew): the caster's milestone share wins when
                    // higher; it never turns a non-revive heal into one.
                    const int pct = std::max(skill.reviveHpPct, a.reviveAtPct);
                    const int amt = std::max(1, t.maxHp * pct / 100);
                    t.hp = amt;
                    emitEvent(*this,
                              {BattleEvent::Type::Revive, actor, ti, amt, false, false, {}});
                    if (a.side == Side::Party) {
                        addThreat(actor, amt);
                    }
                    log += " " + t.name + " is revived with " + std::to_string(amt) + " HP!";
                }
                break;
            }
            case content::SkillCategory::Support:
                // M40 (owner refinement): a Support skill with power also wounds an
                // enemy target (magic), so a debuff-curse deals damage, not only a
                // status. Every shipped Support skill is power 0, so this is inert
                // for all prior content (battles stay byte-identical).
                if (skill.power > 0 && t.side != a.side && t.alive()) {
                    std::string extra;
                    const int base = magicDamage(a, t, skill.power, skill.element);
                    const int dealt = dealMagic(actor, ti, base, extra);
                    log += " " + t.name + " takes " + std::to_string(dealt) + ".";
                    if (!t.alive()) {
                        log += " " + t.name + " is KO'd!";
                    }
                    log += extra;
                }
                break;  // status still applied below
        }

        // M48: an immune target took nothing and takes no rider either — the
        // spell washed over it. A weak one is called out so the number is
        // explained. Both are marks for the floats, never model state.
        if (mod == kElementImmunePct) {
            lastImmune.push_back(ti);
            log += " " + t.name + " is immune!";
            continue;  // no cleanse, no status: nothing of this skill reached it
        }
        if (mod == kElementWeakPct) {
            lastWeak.push_back(ti);
            log += " It is devastating!";
        }

        // Cleanse (M35, narrowed in M47): lift the afflictions from an ally
        // target, so a heal/support skill can double as a cure (the Cleric's
        // Purify). Since rules v7 the stat debuffs survive it — a cleanse
        // answers poison/blind/silence/confusion/sleep, not a weakened sword
        // arm and (M75) never a Curse.
        if (skill.controlEffect == content::SkillEffect::Cleanse && t.alive() &&
            t.side == a.side && clearAfflictions(t)) {
            log += " " + t.name + " is cleansed.";
        }

        // M75 (Uncurse): with the `curesCurse` item, one of exactly two things
        // that ever lift a Curse.
        if (skill.controlEffect == content::SkillEffect::Uncurse && t.alive() &&
            t.side == a.side && hasStatus(t, content::StatusType::Curse)) {
            removeStatus(t, content::StatusType::Curse);
            log += " The curse on " + t.name + " lifts.";
        }

        // Apply the skill's status to living targets. M63 (Lingering Hex): the
        // caster's bonus turns ride along. M75: poison magnitudes scale with
        // the caster's Magic (statusMagnitudeFor), snapshotted here.
        if (skill.statusEffect != content::StatusType::None && t.alive()) {
            addStatus(t, skill.statusEffect,
                      statusMagnitudeFor(a, skill.statusEffect, skill.statusMagnitude),
                      skill.statusDuration, a.statusTurnsBonus);
            log += " " + t.name + ": " + statusLabel(skill.statusEffect) + ".";
        }
    }
    // M45 (the Goose's tradeoff): a skill authored `alsoBuffsEnemies` applies its
    // status to every living FOE as well — the price of a goose's kindness. Inert
    // for every other skill. M63 (Selective Generosity): the caster's milestone
    // suppresses the tradeoff outright.
    if (skill.alsoBuffsEnemies && !a.noEnemyBuff &&
        skill.statusEffect != content::StatusType::None) {
        const Side foe = a.side == Side::Party ? Side::Enemy : Side::Party;
        for (int fi : aliveIndices(foe)) {
            Combatant& f = units[static_cast<std::size_t>(fi)];
            if (isImmuneTo(f, skill.statusEffect)) {
                continue;
            }
            addStatus(f, skill.statusEffect, skill.statusMagnitude, skill.statusDuration);
            log += " " + f.name + " is cheered up: " + statusLabel(skill.statusEffect) + "!";
        }
    }
    if (a.enrages && !a.enrageAnnounced && a.hp * 2 < a.maxHp) {
        a.enrageAnnounced = true;
        log = a.name + " flies into a rage! " + log;
    }
    return log;
}

std::string Battle::useItem(int actor, int target, const content::ItemDef& item) {
    clearActionMarks();
    Combatant& t = units[static_cast<std::size_t>(target)];
    const std::string& who = units[static_cast<std::size_t>(actor)].name;
    std::string log = who + " uses " + item.name + " on " + t.name + ".";
    // M44: an item restricted to one boss does nothing to anyone else - and the
    // caller keeps it rather than spending it on nothing (itemAffects).
    if (!item.requiresBossId.empty() && t.sourceId != item.requiresBossId) {
        return log + " Nothing happens.";
    }
    // M60 telemetry: the item takes effect from here (a kept relic is no use).
    emitEvent(*this, {BattleEvent::Type::Action, actor, target, 0, false, true, item.id});
    // M52 (the Dragon Crown's hidden effect): used on a boss carrying a revive
    // clock (the King), it ends the clock so his fallen court never returns.
    // Schema-driven (no item id is branched on), in this shared path so the
    // Simulator and live play agree by construction. Deliberately silent — no
    // line is appended, so the King simply stops calling his guards back. The
    // design docs record it; the game hides it.
    if (item.disablesMinionRevive) {
        t.reviveMinionTurns = 0;
        t.reviveMinionCounter = 0;
    }
    // M43: an item may carry King-specific amounts (Royal Snacks). kingBattle is
    // set at buildBattle from the team's boss id, so this branch is content-
    // derived and identical in live play and the Simulator.
    int healAmount =
        kingBattle && item.kingEffectAmount > 0 ? item.kingEffectAmount : item.effectAmount;
    int mpAmount = kingBattle ? item.kingMpAmount : 0;
    // M63 (Field Medic): items this USER wields restore more — the heal / MP
    // amounts only; a revive's share, cures, and relic effects are untouched.
    const int potency = units[static_cast<std::size_t>(actor)].itemPotencyPct;
    if (potency > 0) {
        healAmount += healAmount * potency / 100;
        mpAmount += mpAmount * potency / 100;
    }
    switch (item.effect) {
        case content::ConsumableEffect::Heal:
            if (t.alive()) {
                const int hpBefore = t.hp;  // M60 telemetry (effective gain)
                applyHeal(t, healAmount);
                if (observer != nullptr && t.hp > hpBefore) {
                    emitEvent(*this, {BattleEvent::Type::Heal, actor, target, t.hp - hpBefore,
                                      false, false, {}});
                }
                log += " HP +" + std::to_string(healAmount) + ".";
                if (mpAmount > 0) {
                    t.mp = std::min(t.maxMp, t.mp + mpAmount);
                    log += " MP +" + std::to_string(mpAmount) + ".";
                }
            } else {
                log += " No effect.";
            }
            break;
        case content::ConsumableEffect::RestoreMp:
            t.mp = std::min(t.maxMp, t.mp + healAmount);
            log += " MP +" + std::to_string(healAmount) + ".";
            break;
        case content::ConsumableEffect::Revive:
            if (!t.alive()) {
                const int amt = item.effectAmount;
                t.hp = amt <= 100 ? std::max(1, t.maxHp * amt / 100) : amt;
                emitEvent(*this,
                          {BattleEvent::Type::Revive, actor, target, t.hp, false, false, {}});
                log += " " + t.name + " is revived!";
            } else {
                log += " No effect.";
            }
            break;
        case content::ConsumableEffect::Cure:
            clearNegativeStatuses(t);
            log += " Cured.";
            break;
        case content::ConsumableEffect::None:
            log += " Nothing happens.";
            break;
    }
    // M43: an item may also lift the stat debuffs alongside its main effect
    // (Royal Snacks). Inert for every item that does not ask for it.
    if (item.curesDebuffs && t.alive() && clearStatDebuffs(t)) {
        log += " ATK-/DEF- lifted.";
    }
    // M75 (Holy Taxes): the item-side Curse remover — with the `uncurse` skill
    // effect, one of exactly two. Inert for every other item.
    if (item.curesCurse && t.alive() && hasStatus(t, content::StatusType::Curse)) {
        removeStatus(t, content::StatusType::Curse);
        log += " The curse on " + t.name + " lifts.";
    }
    // M44 (Royal Relics): applied statuses and a battle-long stat scale. Both are
    // data-driven, so no relic has hardcoded behavior in the battle model.
    if (t.alive()) {
        for (const content::ItemStatus& s : item.statuses) {
            if (s.type == content::StatusType::None || isImmuneTo(t, s.type)) {
                continue;
            }
            addStatus(t, s.type, s.magnitude, s.duration);
            log += " " + t.name + ": " + statusLabel(s.type) + ".";
        }
        if (item.statScalePct > 0 && item.statScalePct < 100) {
            // Enemy stats never persist past a battle, so "for the rest of the
            // fight" is simply a direct scale of the combatant's own stats. HP/MP
            // are untouched: this weakens a foe, it does not wound it.
            // M58: applied at most ONCE per foe — a second Deadly Spoon no longer
            // re-halves an already-diminished target (which used to stack to a
            // quarter, an eighth, ...).
            // M75: a boss authored `immuneToStatScale` shrugs the relic off
            // entirely (itemAffects keeps the item from being spent on it).
            if (t.statScaleImmune) {
                log += " " + t.name + " is utterly unmoved.";
            } else if (t.statDiminished) {
                log += " " + t.name + " is already diminished.";
            } else {
                const auto scale = [pct = item.statScalePct](int v) {
                    return v > 0 ? std::max(1, v * pct / 100) : v;
                };
                t.stats.attack = scale(t.stats.attack);
                t.stats.magic = scale(t.stats.magic);
                t.stats.defense = scale(t.stats.defense);
                t.stats.speed = scale(t.stats.speed);
                t.statDiminished = true;
                log += " " + t.name + " is diminished for the rest of the battle!";
            }
        }
    }
    return log;
}

std::string Battle::guard(int actor) {
    clearActionMarks();
    Combatant& a = units[static_cast<std::size_t>(actor)];
    a.guarding = true;
    return a.name + " guards.";
}

Battle buildBattle(const Party& party, const dungeon::EnemyTeam& team,
                   const content::ContentDatabase& db) {
    Battle b;

    for (std::size_t i = 0; i < party.members.size(); ++i) {
        const Character& c = party.members[i];
        Combatant u;
        u.side = Side::Party;
        u.name = c.name;
        u.sourceId = c.classId;
        u.partyIndex = static_cast<int>(i);
        u.stats = c.stats;
        u.hp = c.hp;
        u.maxHp = c.maxHp;
        u.mp = c.mp;
        u.maxMp = c.maxMp;
        if (const content::ClassDef* cls = db.findClass(c.classId)) {
            // M29: usable skills are the class learnset resolved at the
            // character's level (startingSkills + level-gated grants), derived
            // identically here for live play and the headless simulator.
            // M64: plus the character's scroll-learned extras (deduped) —
            // allKnownSkills is the one rule the party panel shows too.
            u.skillIds = allKnownSkills(c, db);
            // M45: class battle traits are resolved once, here, so the pure model
            // never needs the content database to know how a unit fights.
            u.attackHitsAll = cls->attackHitsAll;
            for (const content::AttackStatus& s : cls->attackStatuses) {
                u.attackStatuses.push_back({s.type, s.magnitude, s.duration});
            }
            u.uncontrolled = cls->uncontrolled;
        }
        // M48: the equipped weapon's element, resolved once here — so a basic
        // attack carries it without the pure model ever asking what is equipped.
        // Empty hands and untagged steel both mean None.
        if (!c.weapon.empty()) {
            if (const content::ItemDef* weapon = db.findItem(c.weapon)) {
                u.weaponElement = weapon->element;
            }
        }
        // M75: worn element resistance, resolved once across the three pieces.
        // Max per element rather than a sum — there is one accessory slot, so
        // stacking is not a thing today, and max keeps any future second
        // source from compounding past its strongest piece.
        const auto absorbResists = [&u, &db](const std::string& itemId) {
            if (itemId.empty()) {
                return;
            }
            const content::ItemDef* it = db.findItem(itemId);
            if (it == nullptr || it->resistPct <= 0) {
                return;
            }
            for (content::Element e : it->resistElements) {
                int& slot = u.elementResist[static_cast<std::size_t>(e)];
                slot = std::max(slot, it->resistPct);
            }
        };
        absorbResists(c.weapon);
        absorbResists(c.armor);
        absorbResists(c.accessory);
        // M36: own many, equip one - resolve the single equipped passive.
        if (!c.equippedPassive.empty()) {
            applyPassives(u, {c.equippedPassive}, db);
        }
        // M63: the chosen level milestones layer on AFTER the passive so a
        // grant can only ever match-or-raise what is equipped.
        applyMilestones(u, c, db);
        b.units.push_back(std::move(u));
    }

    // Depth stat scaling (M20 composition): applied identically here and in
    // the danger assessment, so labels never lie about what spawns. M52: the
    // per-field multiply lives in content::scaledStats so the bestiary's
    // max-stats readout uses the exact same rule.
    const auto scaled = [&team](const content::StatBlock& base) {
        return content::scaledStats(base, team.statScalePct);
    };

    // M43: is this the King's fight? Read once, from content, so item behavior
    // that keys on it is deterministic and sim-identical.
    b.kingBattle = team.bossId == kKingBossId;

    // M75 (summon_clone): the clone is prepared inside the boss block and
    // appended AFTER the minions, prebuilt dead, so the unit roster never has
    // to grow mid-battle.
    bool hasClone = false;
    Combatant cloneUnit;

    // Boss combatant (built from a BossDef; its minions follow below).
    if (!team.bossId.empty()) {
        if (const content::BossDef* boss = db.findBoss(team.bossId)) {
            Combatant u;
            u.side = Side::Enemy;
            u.sourceId = boss->id;
            u.name = boss->name;
            u.stats = scaled(boss->stats);
            u.hp = u.maxHp = u.stats.maxHp < 1 ? 1 : u.stats.maxHp;
            u.mp = u.maxMp = deriveMaxMp(u.stats.magic);
            u.skillIds = boss->skills;
            u.isBoss = true;
            u.enrages = boss->archetype == content::BossArchetype::Brute;
            u.empowersOnAllyFall = boss->archetype == content::BossArchetype::Sorcerer;
            u.ralliesMinions = boss->archetype == content::BossArchetype::Commander;
            u.rushOpener = boss->archetype == content::BossArchetype::Rush;
            u.telegraph = boss->telegraph;
            u.confusionImmune = boss->immuneToConfusion;  // M40 (the King)
            u.weaknesses = boss->affinity.weaknesses;     // M48
            u.immunities = boss->affinity.immunities;
            u.reviveMinionTurns = boss->reviveMinionTurns;  // M49 (0 = never)
            // M61 (the Deadly Duck): a boss may swing the M45 class machinery —
            // an all-party basic attack with status riders — and shrug off every
            // affliction (the three legacy flags follow so the status queries
            // and display chips agree with the blanket immunity).
            u.attackHitsAll = boss->attackHitsAll;
            for (const content::AttackStatus& s : boss->attackStatuses) {
                u.attackStatuses.push_back({s.type, s.magnitude, s.duration});
            }
            u.afflictionImmune = boss->immuneToAfflictions;
            if (boss->immuneToAfflictions) {
                u.confusionImmune = true;
                u.silenceImmune = true;
                u.blindImmune = true;
            }
            applyPassives(u, boss->passives, db);  // M36 (bosses may carry several)
            // M75: the boss-side new-field resolution — bespoke immunities,
            // sleep manners, the Deadly-Spoon shrug, triggers, battle-start
            // statuses, and the prebuilt clone slot.
            u.statusImmunities = boss->statusImmunities;
            u.avoidSleepingTargets = boss->avoidSleepingTargets;
            u.noStunWhileAllFoesSleep = boss->noStunWhileAllFoesSleep;
            u.statScaleImmune = boss->immuneToStatScale;
            resolveTriggers(u, boss->triggers);
            applyInitialStatuses(u, boss->initialStatuses);
            int clonePct = 0;
            for (const content::TriggerDef& t : boss->triggers) {
                if (t.action == content::TriggerDo::SummonCloneSelf && t.cloneHpPct > 0) {
                    clonePct = t.cloneHpPct;
                }
            }
            if (clonePct > 0) {
                cloneUnit = u;  // the bearer's combat identity...
                cloneUnit.isBoss = false;
                cloneUnit.summonSlot = true;
                cloneUnit.name = u.name + " (Clone)";
                cloneUnit.triggers.clear();  // ...but never its triggers,
                cloneUnit.reviveMinionTurns = 0;  // its revive clock,
                cloneUnit.reviveMinionCounter = 0;
                cloneUnit.statuses.clear();  // or its battle-start statuses.
                cloneUnit.telegraph.clear();
                cloneUnit.maxHp = std::max(1, u.maxHp * clonePct / 100);
                cloneUnit.hp = 0;  // lies dead until the trigger raises it
                hasClone = true;
            }
            b.units.push_back(std::move(u));
        }
    }

    // Count duplicate enemy ids so we can disambiguate names (Goblin A / B).
    std::unordered_map<std::string, int> totals;
    for (const std::string& id : team.enemyIds) {
        ++totals[id];
    }
    std::unordered_map<std::string, int> seen;
    for (const std::string& id : team.enemyIds) {
        const content::EnemyDef* def = db.findEnemy(id);
        if (def == nullptr) {
            continue;
        }
        Combatant u;
        u.side = Side::Enemy;
        u.sourceId = id;
        u.stats = scaled(def->stats);
        u.hp = u.maxHp = u.stats.maxHp < 1 ? 1 : u.stats.maxHp;
        u.mp = u.maxMp = deriveMaxMp(u.stats.magic);
        u.skillIds = def->skills;
        u.isBoss = false;  // minions are not the boss
        u.name = def->name;
        u.weaknesses = def->affinity.weaknesses;  // M48
        u.immunities = def->affinity.immunities;
        u.doNothingPct = def->doNothingPct;    // M61 (a Quacking goose)
        u.doNothingText = def->doNothingText;
        if (totals[id] > 1) {
            const int n = seen[id]++;
            u.name += ' ';
            u.name += static_cast<char>('A' + n);
        }
        applyPassives(u, def->passives, db);  // M36
        // M75: the enemy-side new-field resolution (see the boss block).
        u.statusImmunities = def->statusImmunities;
        u.avoidSleepingTargets = def->avoidSleepingTargets;
        u.noStunWhileAllFoesSleep = def->noStunWhileAllFoesSleep;
        resolveTriggers(u, def->triggers);
        applyInitialStatuses(u, def->initialStatuses);
        b.units.push_back(std::move(u));
    }

    // M75: the clone slot stands at the end of the roster, dead until summoned.
    if (hasClone) {
        b.units.push_back(std::move(cloneUnit));
    }

    // Enmity state (M28): zero threat, plus a per-encounter seed derived purely
    // from the roster so the targeting tie-break is reproducible and identical
    // in live play and the Simulator.
    b.threat.assign(b.units.size(), 0);
    std::uint64_t seed = 0xC0FFEE1234567890ull;
    for (const Combatant& u : b.units) {
        for (char ch : u.name) {
            seed = seed * 131 + static_cast<unsigned char>(ch);
        }
        seed = seed * 131 + static_cast<std::uint64_t>(u.maxHp);
        seed = seed * 131 + static_cast<std::uint64_t>(u.stats.speed);
    }
    b.rngSeed = seed;

    return b;
}

std::vector<int> turnOrder(const Battle& b) {
    std::vector<int> order;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].alive()) {
            order.push_back(static_cast<int>(i));
        }
    }
    std::sort(order.begin(), order.end(), [&](int a, int c) {
        const Combatant& ua = b.units[static_cast<std::size_t>(a)];
        const Combatant& uc = b.units[static_cast<std::size_t>(c)];
        // First Strike (M36): a holder that has not yet acted outranks everyone,
        // so it moves first at the start of the battle (round 1). Inert otherwise.
        const bool aFirst = ua.firstStrike && !ua.actedOnce;
        const bool cFirst = uc.firstStrike && !uc.actedOnce;
        if (aFirst != cFirst) {
            return aFirst;
        }
        if (ua.stats.speed != uc.stats.speed) {
            return ua.stats.speed > uc.stats.speed;
        }
        if (ua.side != uc.side) {
            return ua.side == Side::Party;  // party wins ties
        }
        return a < c;
    });
    return order;
}

EnemyChoice confusedChoice(const Battle& b, int actor) {
    EnemyChoice choice;  // useSkill stays false: confusion forbids deliberate acts
    choice.forced = ForcedAction::BasicAttack;
    const Side foe =
        b.units[static_cast<std::size_t>(actor)].side == Side::Party ? Side::Enemy : Side::Party;
    const std::vector<int> foes = b.aliveIndices(foe);
    // attack() redirects to a seeded member of the actor's OWN side, so this is
    // only a valid nominal target; the actor itself serves when nothing opposes it.
    choice.target = foes.empty() ? actor : foes.front();
    return choice;
}

EnemyChoice uncontrolledChoice(const Battle& b, int actor, const content::ContentDatabase& db) {
    EnemyChoice choice;
    const Combatant& self = b.units[static_cast<std::size_t>(actor)];
    const Side foe = self.side == Side::Party ? Side::Enemy : Side::Party;
    const std::vector<int> foes = b.aliveIndices(foe);
    if (foes.empty()) {
        choice.target = -1;
        return choice;
    }
    // Pure hashes of (seed, round, actor) under their own salts: no roll-stream
    // consumption, so every decider derives the identical turn.
    const std::uint64_t pickRoll = static_cast<std::uint64_t>(
        targetJitter(b.rngSeed ^ kSaltJesterAct, b.turnsTaken, actor, 0, 1 << 20));
    const std::uint64_t aimRoll = static_cast<std::uint64_t>(
        targetJitter(b.rngSeed ^ kSaltJesterAim, b.turnsTaken, actor, 1, 1 << 20));

    choice.target = foes[static_cast<std::size_t>(aimRoll % foes.size())];

    // Its options: every known skill it can actually cast and afford, plus the
    // basic attack (always available). An unusable skill is simply not an option,
    // so a silenced or broke Jester still swings instead of fizzling.
    std::vector<const std::string*> castable;
    for (const std::string& sid : self.skillIds) {
        const content::SkillDef* s = db.findSkill(sid);
        if (s != nullptr && mpCostFor(self, *s) <= self.mp && canCast(self, *s)) {
            castable.push_back(&sid);  // M75: cursed Jesters pay double too
        }
    }
    const std::size_t options = castable.size() + 1;  // +1 = the basic attack
    const std::size_t pick = pickRoll % options;
    if (pick < castable.size()) {
        choice.useSkill = true;
        choice.skillId = *castable[pick];
        // An ally-facing skill is aimed at a random ALLY instead (the Jester is
        // chaotic, not suicidal).
        if (const content::SkillDef* s = db.findSkill(choice.skillId);
            s != nullptr && (s->target == content::SkillTarget::SingleAlly ||
                             s->target == content::SkillTarget::AllAllies ||
                             s->target == content::SkillTarget::Self)) {
            const std::vector<int> allies = b.aliveIndices(self.side);
            choice.target = allies.empty() ? actor
                                           : allies[static_cast<std::size_t>(aimRoll %
                                                                             allies.size())];
        }
    }
    return choice;
}

bool jestThisTurn(const Battle& b, int actor, int lineCount, int& index) {
    index = 0;
    if (lineCount <= 0) {
        return false;
    }
    // Presentation only: its own salt, a pure hash, no rollCursor. Whether a jest
    // shows can never change how the battle resolves.
    const long roll = targetJitter(b.rngSeed ^ kSaltJesterLine, b.turnsTaken, actor, 2, 10000);
    if (roll % 100 >= kJestChancePct) {
        return false;
    }
    index = static_cast<int>((roll / 100) % lineCount);
    return true;
}

int geeseScaringKing(const Battle& b) {
    if (!b.kingBattle) {
        return 0;
    }
    int geese = 0;
    for (const Combatant& u : b.units) {
        if (u.side == Side::Party && u.alive() && u.sourceId == kGooseClassId) {
            ++geese;
        }
    }
    return geese;
}

bool kingScaredThisTurn(const Battle& b, int actor) {
    if (actor < 0 || actor >= static_cast<int>(b.units.size())) {
        return false;
    }
    const Combatant& self = b.units[static_cast<std::size_t>(actor)];
    if (!self.isBoss || !b.kingBattle) {
        return false;
    }
    const int geese = geeseScaringKing(b);
    if (geese <= 0) {
        return false;
    }
    const int chance = std::min(geese * kGoosePerScarePct, 100);
    // Pure hash under its own salt (never advances rollCursor), so the Simulator
    // and BattleState derive the same answer for the same (seed, turn, actor).
    const long roll = targetJitter(b.rngSeed ^ kSaltGooseScare, b.turnsTaken, actor, 3, 10000);
    return (roll % 100) < chance;
}

bool doesNothingThisTurn(const Battle& b, int actor) {
    if (actor < 0 || actor >= static_cast<int>(b.units.size())) {
        return false;
    }
    const Combatant& self = b.units[static_cast<std::size_t>(actor)];
    if (self.doNothingPct <= 0) {
        return false;  // every pre-M61 foe
    }
    // Pure hash under its own salt, exactly the King-scare shape: the Simulator
    // and BattleState derive the same answer, and rollCursor never moves.
    const long roll = targetJitter(b.rngSeed ^ kSaltDoNothing, b.turnsTaken, actor, 4, 10000);
    return (roll % 100) < std::min(self.doNothingPct, 100);
}

ForcedAction forcedActionFor(const Combatant& c) {
    // Order matters only in that a unit can carry more than one: a skipped turn
    // beats a forced guard, which beats a confused swing, because each is stricter
    // than the next.
    if (hasStatus(c, content::StatusType::Stunned)) {
        return ForcedAction::Skip;
    }
    // M75 (Sleep): the turn passes a sleeper by; damage will wake it (poison
    // will not). Between Stunned and Terrified — a sleeper cannot cower.
    if (isAsleep(c)) {
        return ForcedAction::Skip;
    }
    if (hasStatus(c, content::StatusType::Terrified)) {
        return ForcedAction::Guard;
    }
    if (isConfused(c)) {
        return ForcedAction::BasicAttack;
    }
    return ForcedAction::None;
}

EnemyChoice forcedChoice(const Battle& b, int actor, ForcedAction forced) {
    if (forced == ForcedAction::BasicAttack) {
        return confusedChoice(b, actor);
    }
    EnemyChoice choice;
    choice.forced = forced;  // Guard / Skip need no target
    return choice;
}

std::vector<int> itemTargets(const Battle& b, Side side, const content::ItemDef& item) {
    // M44: an enemy-targeting item (the relics) reaches the living foes of `side`.
    if (item.battleTarget == content::BattleTarget::Enemy) {
        return b.aliveIndices(side == Side::Party ? Side::Enemy : Side::Party);
    }
    const bool wantsFallen = item.effect == content::ConsumableEffect::Revive;
    std::vector<int> targets;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].side == side && b.units[i].alive() != wantsFallen) {
            targets.push_back(static_cast<int>(i));
        }
    }
    return targets;
}

bool itemAffects(const Battle& b, int target, const content::ItemDef& item) {
    if (target < 0 || target >= static_cast<int>(b.units.size())) {
        return item.requiresBossId.empty();
    }
    const Combatant& t = b.units[static_cast<std::size_t>(target)];
    if (!item.requiresBossId.empty() && t.sourceId != item.requiresBossId) {
        return false;
    }
    // M75: a stat-scale-only relic (the Deadly Spoon) does nothing at all to a
    // boss authored immune to it — the caller keeps the item, the M44 rule.
    if (item.statScalePct > 0 && item.statuses.empty() &&
        item.effect == content::ConsumableEffect::None && !item.disablesMinionRevive &&
        t.statScaleImmune) {
        return false;
    }
    return true;
}

std::vector<int> skillAllyTargets(const Battle& b, Side side, const content::SkillDef& skill) {
    std::vector<int> targets;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].side == side && (b.units[i].alive() || skill.reviveHpPct > 0)) {
            targets.push_back(static_cast<int>(i));
        }
    }
    return targets;
}

EnemyChoice chooseEnemyAction(const Battle& b, int actor, const content::ContentDatabase& db) {
    EnemyChoice choice;

    const Combatant& self = b.units[static_cast<std::size_t>(actor)];
    // Forced actions (M43 confusion, M44 Terrified/Stunned): a unit whose turn was
    // taken away never gets to choose. Enforced here, in shared code, so an enemy
    // caster can no longer heal or curse through it the way it could before v4.
    if (const ForcedAction forced = forcedActionFor(self); forced != ForcedAction::None) {
        return forcedChoice(b, actor, forced);
    }
    // M58: with no status already taking his turn, the Hollow King can be scared
    // into doing nothing by Geese in the party (10% per living Goose). Same shared
    // rule the Simulator obeys, so sim == live.
    if (kingScaredThisTurn(b, actor)) {
        EnemyChoice scared;
        scared.forced = ForcedAction::Skip;
        return scared;
    }
    // M61: an authored do-nothing foe (a Quacking goose) may spend the turn on
    // nothing at all. Same shared rule, same skip shape, rules v12.
    if (doesNothingThisTurn(b, actor)) {
        EnemyChoice idle;
        idle.forced = ForcedAction::Skip;
        return idle;
    }
    // Silence (M35): a silenced enemy cannot use MP-cost skills, so it falls back
    // to any 0-MP skill or a basic attack. canCast enforces exactly that in each
    // skill loop below.

    // Pick the party target that best matches this enemy's profile (M28):
    // threat, kill pressure, and backline weight, with a small seeded tie-break.
    // Replaces the old "lowest HP always" rule that turned an efficient mage
    // into the party's tank.
    // M75 (sleep manners): a foe authored `avoidSleepingTargets` leaves
    // sleepers alone while anything else stands — its single-target actions
    // aim elsewhere (multi-hit attacks sweep everyone regardless, which is the
    // owner's stated exemption). All-asleep and it attacks normally.
    bool anyAwake = false;
    bool allFoesAsleep = true;
    for (const Combatant& u : b.units) {
        if (u.side == Side::Party && u.alive()) {
            if (isAsleep(u)) {
                continue;
            }
            anyAwake = true;
            allFoesAsleep = false;
        }
    }
    const TargetProfile profile = profileFor(self, db);
    int targetParty = -1;
    long bestScore = 0;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        const Combatant& u = b.units[i];
        if (u.side != Side::Party || !u.alive()) {
            continue;
        }
        if (self.avoidSleepingTargets && anyAwake && isAsleep(u)) {
            continue;  // M75: spare the sleeper while another target stands
        }
        const long score = targetScore(profile, u, b.threatOf(static_cast<int>(i))) +
                           targetJitter(b.rngSeed, b.turnsTaken, actor, static_cast<int>(i), 24);
        if (targetParty < 0 || score > bestScore) {
            bestScore = score;
            targetParty = static_cast<int>(i);
        }
    }

    // Prefer healing a badly hurt ally if a heal skill is affordable.
    int hurtAlly = -1;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        const Combatant& u = b.units[i];
        if (u.side == Side::Enemy && u.alive() && u.hp * 2 < u.maxHp) {
            if (hurtAlly < 0 || u.hp < b.units[static_cast<std::size_t>(hurtAlly)].hp) {
                hurtAlly = static_cast<int>(i);
            }
        }
    }

    // M75: a foe authored `noStunWhileAllFoesSleep` (the Deadly Duck's manners)
    // shelves its Stun-rider skills while the whole opposing side sleeps.
    const auto stunShelved = [&](const content::SkillDef& skill) {
        return self.noStunWhileAllFoesSleep && allFoesAsleep &&
               skill.statusEffect == content::StatusType::Stunned;
    };

    for (const std::string& sid : self.skillIds) {
        const content::SkillDef* skill = db.findSkill(sid);
        if (skill == nullptr || mpCostFor(self, *skill) > self.mp || !canCast(self, *skill)) {
            continue;  // M75: a cursed caster pays double, so it must afford double
        }
        if (skill->category == content::SkillCategory::Heal && hurtAlly >= 0) {
            choice.useSkill = true;
            choice.skillId = sid;
            choice.target = hurtAlly;
            return choice;
        }
    }
    // Support skills (M20: makes the buffer/debuffer role real): cast a
    // buff/debuff whose status the target does not already carry. Statuses
    // expire, so this recurs but never spams while one is active.
    for (const std::string& sid : self.skillIds) {
        const content::SkillDef* skill = db.findSkill(sid);
        if (skill == nullptr || mpCostFor(self, *skill) > self.mp || !canCast(self, *skill) ||
            skill->category != content::SkillCategory::Support ||
            skill->statusEffect == content::StatusType::None || stunShelved(*skill)) {
            continue;
        }
        // M77: an ALL-enemy support skill (the King's lullaby, the Duck's
        // notice, the Hexwing's grudge) is gated on the PROFILED party target,
        // not the actor — an actor-side check would re-cast a party-wide
        // status every single turn (the caster never carries it). No pre-M77
        // foe carries a support skill targeting all_enemies, so every earlier
        // battle resolves byte-identically (recorded in the M77 note).
        const bool onEnemy = skill->target == content::SkillTarget::SingleEnemy ||
                             skill->target == content::SkillTarget::AllEnemies;
        const int ti = onEnemy ? targetParty : actor;
        if (ti < 0) {
            continue;
        }
        // Use presence, not magnitude: the M35 statuses (silence/blind/confusion)
        // carry magnitude 0, so statusSum would read 0 and the AI would re-cast
        // every turn. hasStatus is correct for magnitude-based and duration-only.
        if (!hasStatus(b.units[static_cast<std::size_t>(ti)], skill->statusEffect)) {
            choice.useSkill = true;
            choice.skillId = sid;
            choice.target = ti;
            return choice;
        }
    }

    for (const std::string& sid : self.skillIds) {
        const content::SkillDef* skill = db.findSkill(sid);
        if (skill == nullptr || mpCostFor(self, *skill) > self.mp || !canCast(self, *skill) ||
            stunShelved(*skill)) {
            continue;
        }
        const bool damaging = skill->category == content::SkillCategory::Physical ||
                              skill->category == content::SkillCategory::Magic;
        if (damaging && targetParty >= 0) {
            choice.useSkill = true;
            choice.skillId = sid;
            choice.target = targetParty;
            return choice;
        }
    }

    choice.useSkill = false;
    choice.target = targetParty;
    return choice;
}

}  // namespace cd::battle
