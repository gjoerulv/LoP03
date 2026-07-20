#include "battle/Battle.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "dungeon/DungeonModel.hpp"
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
    for (StatusInstance& s : c.statuses) {
        if (s.type == type) {
            s.magnitude = magnitude;
            s.turns = turns;
            return;
        }
    }
    c.statuses.push_back({type, magnitude, turns});
}

void clearNegativeStatuses(Combatant& c) {
    std::vector<StatusInstance> kept;
    for (const StatusInstance& s : c.statuses) {
        if (s.type != content::StatusType::Poison && s.type != content::StatusType::AttackDown &&
            s.type != content::StatusType::DefenseDown) {
            kept.push_back(s);
        }
    }
    c.statuses = std::move(kept);
}

const char* statusLabel(content::StatusType type) {
    switch (type) {
        case content::StatusType::Poison: return "Poison";
        case content::StatusType::AttackUp: return "ATK+";
        case content::StatusType::AttackDown: return "ATK-";
        case content::StatusType::DefenseUp: return "DEF+";
        case content::StatusType::DefenseDown: return "DEF-";
        case content::StatusType::None: return "";
    }
    return "";
}

void applyDamage(Combatant& d, int dmg) { d.hp = std::max(0, d.hp - dmg); }

void applyHeal(Combatant& d, int amount) {
    if (d.hp <= 0) {
        return;  // a KO'd unit needs a revive, not a heal
    }
    d.hp = std::min(d.maxHp, d.hp + amount);
}

// --- M28 enmity/targeting helpers ---

// SplitMix64 mix — the basis for the deterministic targeting tie-break.
std::uint64_t mix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

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
}

void Battle::clearGuard(int unit) {
    if (unit >= 0 && unit < static_cast<int>(units.size())) {
        units[static_cast<std::size_t>(unit)].guarding = false;
        units[static_cast<std::size_t>(unit)].intercepting = false;
    }
}

std::string Battle::tickStatuses(int unit) {
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
            c.hp = std::max(0, c.hp - s.magnitude);
            log += c.name + " takes " + std::to_string(s.magnitude) + " poison damage.";
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
    target = redirectTarget(units, actor, target);  // intercept redirects enemy hits (M28)
    Combatant& a = units[static_cast<std::size_t>(actor)];
    Combatant& t = units[static_cast<std::size_t>(target)];
    const bool opener = a.rushOpener && !a.actedOnce;
    a.actedOnce = true;
    int dmg = physicalDamage(a, t, 0);
    if (opener) {
        dmg *= 2;  // Rush: opening fury
    }
    applyDamage(t, dmg);
    if (a.side == Side::Party) {
        addThreat(actor, dmg);  // dealing damage draws enmity (M28)
    }
    std::string log = a.name + " attacks " + t.name + " for " + std::to_string(dmg) + ".";
    if (opener) {
        log += " (opening fury!)";
    }
    if (!t.alive()) {
        log += " " + t.name + " is KO'd!";
    }
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
        case content::SkillEffect::None:
            break;
    }

    for (int ti : resolveTargets(skill, actor, primaryTarget)) {
        if (ti < 0 || ti >= static_cast<int>(units.size())) {
            continue;
        }
        Combatant& t = units[static_cast<std::size_t>(ti)];
        switch (skill.category) {
            case content::SkillCategory::Physical: {
                int dmg = physicalDamage(a, t, skill.power);
                if (opener) {
                    dmg *= 2;  // Rush: opening fury
                }
                applyDamage(t, dmg);
                if (a.side == Side::Party) {
                    addThreat(actor, dmg);  // M28 enmity
                }
                log += " " + t.name + " takes " + std::to_string(dmg) + ".";
                if (!t.alive()) {
                    log += " " + t.name + " is KO'd!";
                }
                break;
            }
            case content::SkillCategory::Magic: {
                int dmg = magicDamage(a, t, skill.power);
                dmg = dmg * (100 + empowerPct) / 100;
                if (opener) {
                    dmg *= 2;  // Rush: opening fury
                }
                applyDamage(t, dmg);
                if (a.side == Side::Party) {
                    addThreat(actor, dmg);  // M28 enmity
                }
                log += " " + t.name + " takes " + std::to_string(dmg) + ".";
                if (!t.alive()) {
                    log += " " + t.name + " is KO'd!";
                }
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
                }
                break;
            }
            case content::SkillCategory::Support:
                break;  // effect comes from the applied status below
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
    Combatant& t = units[static_cast<std::size_t>(target)];
    const std::string& who = units[static_cast<std::size_t>(actor)].name;
    std::string log = who + " uses " + item.name + " on " + t.name + ".";
    switch (item.effect) {
        case content::ConsumableEffect::Heal:
            if (t.alive()) {
                applyHeal(t, item.effectAmount);
                log += " HP +" + std::to_string(item.effectAmount) + ".";
            } else {
                log += " No effect.";
            }
            break;
        case content::ConsumableEffect::RestoreMp:
            t.mp = std::min(t.maxMp, t.mp + item.effectAmount);
            log += " MP +" + std::to_string(item.effectAmount) + ".";
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
    return log;
}

std::string Battle::guard(int actor) {
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
            u.skillIds = cls->startingSkills;
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

EnemyChoice chooseEnemyAction(const Battle& b, int actor, const content::ContentDatabase& db) {
    EnemyChoice choice;

    const Combatant& self = b.units[static_cast<std::size_t>(actor)];

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
        if (skill == nullptr || skill->mpCost > self.mp) {
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
        if (skill == nullptr || skill->mpCost > self.mp ||
            skill->category != content::SkillCategory::Support ||
            skill->statusEffect == content::StatusType::None) {
            continue;
        }
        const bool onEnemy = skill->target == content::SkillTarget::SingleEnemy;
        const int ti = onEnemy ? targetParty : actor;
        if (ti < 0) {
            continue;
        }
        if (statusSum(b.units[static_cast<std::size_t>(ti)], skill->statusEffect) == 0) {
            choice.useSkill = true;
            choice.skillId = sid;
            choice.target = ti;
            return choice;
        }
    }

    for (const std::string& sid : self.skillIds) {
        const content::SkillDef* skill = db.findSkill(sid);
        if (skill == nullptr || skill->mpCost > self.mp) {
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
