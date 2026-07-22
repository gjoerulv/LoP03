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

int physicalDamage(const Combatant& a, const Combatant& d, int power) {
    int dmg = std::max(1, effectiveAttack(a) + power - effectiveDefense(d) / 2);
    if (d.guarding) {
        dmg = std::max(1, dmg / 2);
    }
    return dmg;
}

int magicDamage(const Combatant& a, const Combatant& d, int power) {
    int dmg = std::max(1, a.stats.magic + power - effectiveDefense(d) / 4);
    if (d.guarding) {
        dmg = std::max(1, dmg / 2);
    }
    return dmg;
}

int healValue(const Combatant& a, int power) { return power + a.stats.magic / 2; }

void addStatus(Combatant& c, content::StatusType type, int magnitude, int turns) {
    if (type == content::StatusType::None || turns <= 0) {
        return;
    }
    const int scaledTurns = turns * kStatusDurationMult;  // M35: statuses last 2x their authored duration
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
    // Confusion/Silence/Blind), keeping only the beneficial buffs.
    std::vector<StatusInstance> kept;
    for (const StatusInstance& s : c.statuses) {
        if (s.type == content::StatusType::AttackUp || s.type == content::StatusType::DefenseUp) {
            kept.push_back(s);
        }
    }
    c.statuses = std::move(kept);
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
        case content::StatusType::None: return "";
    }
    return "";
}

void applyDamage(Combatant& d, int dmg) {
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
        const int cdmg = physicalDamage(t, a, 0);
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

void Battle::clearGuard(int unit) {
    if (unit >= 0 && unit < static_cast<int>(units.size())) {
        units[static_cast<std::size_t>(unit)].guarding = false;
        units[static_cast<std::size_t>(unit)].intercepting = false;
    }
}

std::string Battle::tickStatuses(int unit) {
    lastMissed.clear();
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
            c.hp = std::max(0, c.hp - dmg);
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
    lastMissed.clear();
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
    int base = physicalDamage(a, t, 0);
    if (opener) {
        base *= 2;  // Rush: opening fury
    }
    std::string extra;
    const int dealt = dealPhysical(actor, target, base, extra);  // M36 passive effects
    std::string log = a.name + verb + t.name + " for " + std::to_string(dealt) + ".";
    if (opener) {
        log += " (opening fury!)";
    }
    if (!t.alive()) {
        log += " " + t.name + " is KO'd!";
    }
    log += extra;
    if (a.enrages && !a.enrageAnnounced && a.hp * 2 < a.maxHp) {
        a.enrageAnnounced = true;
        log = a.name + " flies into a rage! " + log;
    }
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
    lastMissed.clear();
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
        switch (skill.category) {
            case content::SkillCategory::Physical: {
                int base = physicalDamage(a, t, skill.power);
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
                int base = magicDamage(a, t, skill.power);
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
                    const int base = magicDamage(a, t, skill.power);
                    const int dealt = dealMagic(actor, ti, base, extra);
                    log += " " + t.name + " takes " + std::to_string(dealt) + ".";
                    if (!t.alive()) {
                        log += " " + t.name + " is KO'd!";
                    }
                    log += extra;
                }
                break;  // status still applied below
        }

        // Cleanse (M35): strip every negative status from an ally target, so a
        // heal/support skill can double as a cure (Cleric's Purify).
        if (skill.controlEffect == content::SkillEffect::Cleanse && t.alive() &&
            t.side == a.side && !t.statuses.empty()) {
            clearNegativeStatuses(t);
            log += " " + t.name + " is cleansed.";
        }

        // Apply the skill's status to living targets.
        if (skill.statusEffect != content::StatusType::None && t.alive()) {
            addStatus(t, skill.statusEffect, skill.statusMagnitude, skill.statusDuration);
            log += " " + t.name + ": " + statusLabel(skill.statusEffect) + ".";
        }
    }
    if (a.enrages && !a.enrageAnnounced && a.hp * 2 < a.maxHp) {
        a.enrageAnnounced = true;
        log = a.name + " flies into a rage! " + log;
    }
    return log;
}

std::string Battle::useItem(int actor, int target, const content::ItemDef& item) {
    lastMissed.clear();
    Combatant& t = units[static_cast<std::size_t>(target)];
    const std::string& who = units[static_cast<std::size_t>(actor)].name;
    std::string log = who + " uses " + item.name + " on " + t.name + ".";
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
    return log;
}

std::string Battle::guard(int actor) {
    lastMissed.clear();
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
        }
        // M36: own many, equip one - resolve the single equipped passive.
        if (!c.equippedPassive.empty()) {
            applyPassives(u, {c.equippedPassive}, db);
        }
        b.units.push_back(std::move(u));
    }

    // Depth stat scaling (M20 composition): applied identically here and in
    // the danger assessment, so labels never lie about what spawns.
    const auto scaled = [&team](const content::StatBlock& base) {
        content::StatBlock s = base;
        s.maxHp = s.maxHp * team.statScalePct / 100;
        s.attack = s.attack * team.statScalePct / 100;
        s.magic = s.magic * team.statScalePct / 100;
        s.defense = s.defense * team.statScalePct / 100;
        s.speed = s.speed * team.statScalePct / 100;
        return s;
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
    const Side foe =
        b.units[static_cast<std::size_t>(actor)].side == Side::Party ? Side::Enemy : Side::Party;
    const std::vector<int> foes = b.aliveIndices(foe);
    // attack() redirects to a seeded member of the actor's OWN side, so this is
    // only a valid nominal target; the actor itself serves when nothing opposes it.
    choice.target = foes.empty() ? actor : foes.front();
    return choice;
}

std::vector<int> itemTargets(const Battle& b, Side side, const content::ItemDef& item) {
    const bool wantsFallen = item.effect == content::ConsumableEffect::Revive;
    std::vector<int> targets;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].side == side && b.units[i].alive() != wantsFallen) {
            targets.push_back(static_cast<int>(i));
        }
    }
    return targets;
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
    // Confusion (M43): a confused unit cannot choose - it swings wildly. Enforced
    // here, in shared code, so an enemy caster can no longer heal or curse through
    // its confusion the way it could before this bump.
    if (isConfused(self)) {
        return confusedChoice(b, actor);
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
