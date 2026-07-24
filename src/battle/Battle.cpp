#include "battle/Battle.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Castle.hpp"  // kKingBossId (M43: King-context items)
#include "game/Party.hpp"

namespace cd::battle {

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
    for (content::Element x : defender.weaknesses) {
        if (x == e) {
            return kElementWeakPct;
        }
    }
    return 100;
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

int effectiveAttack(const Combatant& a) {
    int value = a.stats.attack * attackPercent(a) / 100;
    if (a.enrages && a.hp * 2 < a.maxHp) {
        value = value * 3 / 2;  // Brute enrage
    }
    return value;
}

int effectiveDefense(const Combatant& d) { return d.stats.defense * defensePercent(d) / 100; }

// M48: `element` is applied LAST, after the max(1, ...) floor — an immune hit
// must land on 0, and the floor would otherwise turn it into 1. Guarding and
// affinity therefore compose multiplicatively, which is what a player reading
// "guarded" and "immune" separately would expect.
int physicalDamage(const Combatant& a, const Combatant& d, int power,
                   content::Element element) {
    int dmg = std::max(1, effectiveAttack(a) + power - effectiveDefense(d) / 2);
    if (d.guarding) {
        dmg = std::max(1, dmg / 2);
    }
    return dmg * elementModifier(d, element) / 100;
}

int magicDamage(const Combatant& a, const Combatant& d, int power, content::Element element) {
    int dmg = std::max(1, a.stats.magic + power - effectiveDefense(d) / 4);
    if (d.guarding) {
        dmg = std::max(1, dmg / 2);
    }
    return dmg * elementModifier(d, element) / 100;
}

int healValue(const Combatant& a, int power) { return power + a.stats.magic / 2; }

void addStatus(Combatant& c, content::StatusType type, int magnitude, int turns) {
    if (type == content::StatusType::None || turns <= 0) {
        return;
    }
    // M35: statuses last 2x their authored duration - EXCEPT the M44 turn-control
    // statuses, which take the turn itself. Doubling those would quietly turn one
    // skipped turn into two, so they are applied exactly as authored. (Statuses
    // tick at the START of the bearer's turn, before it acts, so a duration of 2
    // costs it exactly one turn.)
    const bool turnControl = type == content::StatusType::Terrified ||
                             type == content::StatusType::Stunned;
    const int scaledTurns = turnControl ? turns : turns * kStatusDurationMult;
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
    // Cure strips every negative status (poison, ATK-/DEF- debuffs, and the M35
    // Confusion/Silence/Blind), keeping only the beneficial buffs. This is the
    // ITEM rule (Remedy/Antidote) — M47 narrowed the SKILL cleanse below, and
    // deliberately left this one alone so cure items are unchanged.
    std::vector<StatusInstance> kept;
    for (const StatusInstance& s : c.statuses) {
        if (s.type == content::StatusType::AttackUp || s.type == content::StatusType::DefenseUp) {
            kept.push_back(s);
        }
    }
    c.statuses = std::move(kept);
}

// M47 (rules v7): the `cleanse` skill control lifts AFFLICTIONS only — Poison,
// Blind, Silence, Confusion. ATK-/DEF- now survive a Purify (a cure item or
// Royal Snacks is what lifts those), and so do the M44 turn-control statuses,
// which take the turn they were bought with. Returns whether anything went, so
// the log can stay honest.
bool clearAfflictions(Combatant& c) {
    const std::size_t before = c.statuses.size();
    c.statuses.erase(std::remove_if(c.statuses.begin(), c.statuses.end(),
                                    [](const StatusInstance& s) {
                                        return s.type == content::StatusType::Poison ||
                                               s.type == content::StatusType::Blind ||
                                               s.type == content::StatusType::Silence ||
                                               s.type == content::StatusType::Confusion;
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
        case content::StatusType::None: return "";
    }
    return "";
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
constexpr const char* kGooseClassId = "goose";

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

void Battle::applyDamage(Combatant& d, int dmg) {
#ifndef CRYSTAL_SHIPPING_BUILD
    // M53 debug god mode: a party unit never drops below 1 HP from a hit routed
    // through here. Same shape as Iron Will, but repeatable and party-wide; still
    // snaps the unit out of confusion like any real hit. Compiled out of shipping
    // builds and off by default, so the normal path below is untouched.
    if (debugPartyUnkillable && d.side == Side::Party && dmg > 0 && d.hp > 0 &&
        d.hp - dmg <= 0) {
        d.hp = 1;
        removeStatus(d, content::StatusType::Confusion);
        return;
    }
#endif
    // Iron Will (M36): a lethal blow leaves the holder at 1 HP, once per battle.
    if (dmg > 0 && d.hp > 0 && d.hp - dmg <= 0 && d.ironWill && !d.ironWillUsed) {
        d.ironWillUsed = true;
        d.hp = 1;
    } else {
        d.hp = std::max(0, d.hp - dmg);
    }
    // Confusion (M35): a hit snaps its bearer out of confusion. Single chokepoint
    // for all attack/skill damage (poison DoT does not route through here), so the
    // rule holds identically in live play and the Simulator.
    if (dmg > 0) {
        removeStatus(d, content::StatusType::Confusion);
    }
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
    int toTarget = dmg;
    const int guard = bodyguardFor(target);
    if (guard >= 0) {  // Bodyguard (M36): the weakest ally's guard soaks a share
        Combatant& g = units[static_cast<std::size_t>(guard)];
        const int share = dmg * g.bodyguardPct / 100;
        if (share > 0) {
            applyDamage(g, share);
            toTarget = dmg - share;
            extra += " " + g.name + " shields " + units[static_cast<std::size_t>(target)].name +
                     " (" + std::to_string(share) + ").";
            if (!g.alive()) {
                extra += " " + g.name + " is KO'd!";
            }
        }
    }
    applyDamage(units[static_cast<std::size_t>(target)], toTarget);
    if (a.side == Side::Party) {
        addThreat(actor, dmg);  // total damage draws enmity (M28)
    }
    Combatant& t = units[static_cast<std::size_t>(target)];
    if (actor != target && t.thornsPct > 0 && toTarget > 0) {  // Thorns (M36)
        const int reflect = toTarget * t.thornsPct / 100;
        if (reflect > 0) {
            applyDamage(a, reflect);
            extra += " " + a.name + " takes " + std::to_string(reflect) + " thorns damage.";
            if (!a.alive()) {
                extra += " " + a.name + " is KO'd!";
            }
        }
    }
    if (a.lifedrinkPct > 0 && dmg > 0 && a.alive()) {  // Lifedrink (M36)
        const int heal = dmg * a.lifedrinkPct / 100;
        if (heal > 0) {
            applyHeal(a, heal);
            extra += " " + a.name + " drains " + std::to_string(heal) + " HP.";
        }
    }
    if (actor != target && t.alive() && t.counterAttack && !t.counteredThisRound &&
        a.alive()) {  // Counter Attack (M36): one basic retaliation per round
        t.counteredThisRound = true;
        // M48: a counter IS a basic attack, so it swings the counter-attacker's
        // own weapon element.
        const int cdmg = physicalDamage(t, a, 0, t.weaponElement);
        applyDamage(a, cdmg);
        if (t.side == Side::Party) {
            addThreat(target, cdmg);
        }
        extra += " " + t.name + " counters " + a.name + " for " + std::to_string(cdmg) + ".";
        if (!a.alive()) {
            extra += " " + a.name + " is KO'd!";
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
    int toTarget = dmg;
    const int guard = bodyguardFor(target);
    if (guard >= 0) {  // Bodyguard soaks magic too (any damage aimed at the weakest ally)
        Combatant& g = units[static_cast<std::size_t>(guard)];
        const int share = dmg * g.bodyguardPct / 100;
        if (share > 0) {
            applyDamage(g, share);
            toTarget = dmg - share;
            extra += " " + g.name + " shields " + units[static_cast<std::size_t>(target)].name +
                     " (" + std::to_string(share) + ").";
            if (!g.alive()) {
                extra += " " + g.name + " is KO'd!";
            }
        }
    }
    applyDamage(units[static_cast<std::size_t>(target)], toTarget);
    if (a.side == Side::Party) {
        addThreat(actor, dmg);
    }
    return toTarget;
}

bool canCast(const Combatant& c, const content::SkillDef& skill) {
    return !(isSilenced(c) && skill.mpCost > 0);
}

std::string Battle::beginUnitTurn(int actor) {
    if (actor < 0 || actor >= static_cast<int>(units.size())) {
        return "";
    }
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
        c.hp = c.maxHp;
        c.statuses.clear();  // raised whole, not raised wounded
    }
    return a.name + " strikes the floor: \"RISE.\" The court stands again, unmarked.";
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
            c.hp = std::max(floor, c.hp - dmg);
            log += c.name + " takes " + std::to_string(dmg) + " poison damage.";
            if (!c.alive()) {
                log += " " + c.name + " is KO'd!";
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
    // M45: a class whose basic attack hits every foe sweeps instead of striking
    // one — except while confused, when it lashes at a single member of its own
    // side like anyone else.
    const Combatant& self = units[static_cast<std::size_t>(actor)];
    if (self.attackHitsAll && !isConfused(self)) {
        return attackAll(actor);
    }
    return attackOne(actor, target);
}

std::string Battle::attackOne(int actor, int target) {
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
    int base = physicalDamage(a, t, 0, element);
    if (opener) {
        base *= 2;  // Rush: opening fury
    }
    std::string extra;
    const int dealt = dealPhysical(actor, target, base, extra);  // M36 passive effects
    std::string log = a.name + verb + t.name + " for " + std::to_string(dealt) + ".";
    if (opener) {
        log += " (opening fury!)";
    }
    if (mod == kElementImmunePct) {
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
    if (mod != kElementImmunePct) {
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
        addStatus(t, s.type, s.magnitude, s.turns);
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
        log += " " + attackOne(actor, ti);
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
    Combatant& a = units[static_cast<std::size_t>(actor)];
    a.mp = std::max(0, a.mp - skill.mpCost);
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
        case content::SkillEffect::None:
            break;
    }

    for (int ti : resolveTargets(skill, actor, primaryTarget)) {
        if (ti < 0 || ti >= static_cast<int>(units.size())) {
            continue;
        }
        Combatant& t = units[static_cast<std::size_t>(ti)];
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
                break;
            }
            case content::SkillCategory::Magic: {
                int base = magicDamage(a, t, skill.power, skill.element);
                base = base * (100 + empowerPct) / 100;
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
                break;
            }
            case content::SkillCategory::Heal: {
                if (t.alive()) {
                    const int amt = healValue(a, skill.power);
                    applyHeal(t, amt);
                    if (a.side == Side::Party) {
                        addThreat(actor, amt);  // healing draws enmity too (M28)
                    }
                    log += " " + t.name + " recovers " + std::to_string(amt) + " HP.";
                } else if (skill.reviveHpPct > 0) {
                    // M43: a revive-capable heal (Renew) raises a KO'd ally at a
                    // fixed share of its max HP - the skill's own power never
                    // enters, so reviving is an emergency, not a heal.
                    const int amt = std::max(1, t.maxHp * skill.reviveHpPct / 100);
                    t.hp = amt;
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
        // answers poison/blind/silence/confusion, not a weakened sword arm.
        if (skill.controlEffect == content::SkillEffect::Cleanse && t.alive() &&
            t.side == a.side && clearAfflictions(t)) {
            log += " " + t.name + " is cleansed.";
        }

        // Apply the skill's status to living targets.
        if (skill.statusEffect != content::StatusType::None && t.alive()) {
            addStatus(t, skill.statusEffect, skill.statusMagnitude, skill.statusDuration);
            log += " " + t.name + ": " + statusLabel(skill.statusEffect) + ".";
        }
    }
    // M45 (the Goose's tradeoff): a skill authored `alsoBuffsEnemies` applies its
    // status to every living FOE as well — the price of a goose's kindness. Inert
    // for every other skill, so nothing else changes.
    if (skill.alsoBuffsEnemies && skill.statusEffect != content::StatusType::None) {
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
    const int healAmount =
        kingBattle && item.kingEffectAmount > 0 ? item.kingEffectAmount : item.effectAmount;
    const int mpAmount = kingBattle ? item.kingMpAmount : 0;
    switch (item.effect) {
        case content::ConsumableEffect::Heal:
            if (t.alive()) {
                applyHeal(t, healAmount);
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
            if (t.statDiminished) {
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
            u.skillIds = content::knownSkillsFor(*cls, c.level);
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
        // M36: own many, equip one - resolve the single equipped passive.
        if (!c.equippedPassive.empty()) {
            applyPassives(u, {c.equippedPassive}, db);
        }
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
            applyPassives(u, boss->passives, db);  // M36 (bosses may carry several)
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
        if (totals[id] > 1) {
            const int n = seen[id]++;
            u.name += ' ';
            u.name += static_cast<char>('A' + n);
        }
        applyPassives(u, def->passives, db);  // M36
        b.units.push_back(std::move(u));
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
        if (s != nullptr && s->mpCost <= self.mp && canCast(self, *s)) {
            castable.push_back(&sid);
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

ForcedAction forcedActionFor(const Combatant& c) {
    // Order matters only in that a unit can carry more than one: a skipped turn
    // beats a forced guard, which beats a confused swing, because each is stricter
    // than the next.
    if (hasStatus(c, content::StatusType::Stunned)) {
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
    if (item.requiresBossId.empty()) {
        return true;
    }
    if (target < 0 || target >= static_cast<int>(b.units.size())) {
        return false;
    }
    return b.units[static_cast<std::size_t>(target)].sourceId == item.requiresBossId;
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
    // Silence (M35): a silenced enemy cannot use MP-cost skills, so it falls back
    // to any 0-MP skill or a basic attack. canCast enforces exactly that in each
    // skill loop below.

    // Pick the party target that best matches this enemy's profile (M28):
    // threat, kill pressure, and backline weight, with a small seeded tie-break.
    // Replaces the old "lowest HP always" rule that turned an efficient mage
    // into the party's tank.
    const TargetProfile profile = profileFor(self, db);
    int targetParty = -1;
    long bestScore = 0;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        const Combatant& u = b.units[i];
        if (u.side != Side::Party || !u.alive()) {
            continue;
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

    for (const std::string& sid : self.skillIds) {
        const content::SkillDef* skill = db.findSkill(sid);
        if (skill == nullptr || skill->mpCost > self.mp || !canCast(self, *skill)) {
            continue;
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
        if (skill == nullptr || skill->mpCost > self.mp || !canCast(self, *skill) ||
            skill->category != content::SkillCategory::Support ||
            skill->statusEffect == content::StatusType::None) {
            continue;
        }
        const bool onEnemy = skill->target == content::SkillTarget::SingleEnemy;
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
        if (skill == nullptr || skill->mpCost > self.mp || !canCast(self, *skill)) {
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
