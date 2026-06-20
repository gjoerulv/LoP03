#include "battle/Battle.hpp"

#include <algorithm>
#include <unordered_map>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"

namespace cd::battle {

namespace {

int physicalDamage(const Combatant& a, const Combatant& d, int power) {
    int dmg = std::max(1, a.stats.attack + power - d.stats.defense / 2);
    if (d.guarding) {
        dmg = std::max(1, dmg / 2);
    }
    return dmg;
}

int magicDamage(const Combatant& a, const Combatant& d, int power) {
    int dmg = std::max(1, a.stats.magic + power - d.stats.defense / 4);
    if (d.guarding) {
        dmg = std::max(1, dmg / 2);
    }
    return dmg;
}

int healValue(const Combatant& a, int power) { return power + a.stats.magic / 2; }

void applyDamage(Combatant& d, int dmg) { d.hp = std::max(0, d.hp - dmg); }

void applyHeal(Combatant& d, int amount) {
    if (d.hp <= 0) {
        return;  // a KO'd unit needs a revive, not a heal
    }
    d.hp = std::min(d.maxHp, d.hp + amount);
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

void Battle::clearGuard(int unit) {
    if (unit >= 0 && unit < static_cast<int>(units.size())) {
        units[static_cast<std::size_t>(unit)].guarding = false;
    }
}

std::string Battle::attack(int actor, int target) {
    Combatant& a = units[static_cast<std::size_t>(actor)];
    Combatant& t = units[static_cast<std::size_t>(target)];
    const int dmg = physicalDamage(a, t, 0);
    applyDamage(t, dmg);
    std::string log = a.name + " attacks " + t.name + " for " + std::to_string(dmg) + ".";
    if (!t.alive()) {
        log += " " + t.name + " is KO'd!";
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
    Combatant& a = units[static_cast<std::size_t>(actor)];
    a.mp = std::max(0, a.mp - skill.mpCost);
    std::string log = a.name + " uses " + skill.name + ".";

    for (int ti : resolveTargets(skill, actor, primaryTarget)) {
        if (ti < 0 || ti >= static_cast<int>(units.size())) {
            continue;
        }
        Combatant& t = units[static_cast<std::size_t>(ti)];
        switch (skill.category) {
            case content::SkillCategory::Physical: {
                const int dmg = physicalDamage(a, t, skill.power);
                applyDamage(t, dmg);
                log += " " + t.name + " takes " + std::to_string(dmg) + ".";
                if (!t.alive()) {
                    log += " " + t.name + " is KO'd!";
                }
                break;
            }
            case content::SkillCategory::Magic: {
                const int dmg = magicDamage(a, t, skill.power);
                applyDamage(t, dmg);
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
                    log += " " + t.name + " recovers " + std::to_string(amt) + " HP.";
                }
                break;
            }
            case content::SkillCategory::Support: {
                t.guarding = true;
                log += " " + t.name + " is shielded.";
                break;
            }
        }
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
        u.stats = def->stats;
        u.hp = u.maxHp = def->stats.maxHp;
        u.mp = u.maxMp = deriveMaxMp(def->stats.magic);
        u.skillIds = def->skills;
        u.isBoss = team.isBoss;
        u.name = def->name;
        if (totals[id] > 1) {
            const int n = seen[id]++;
            u.name += ' ';
            u.name += static_cast<char>('A' + n);
        }
        b.units.push_back(std::move(u));
    }

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

    // Lowest-HP living party member (deterministic tie-break by index).
    int targetParty = -1;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        const Combatant& u = b.units[i];
        if (u.side == Side::Party && u.alive()) {
            if (targetParty < 0 || u.hp < b.units[static_cast<std::size_t>(targetParty)].hp) {
                targetParty = static_cast<int>(i);
            }
        }
    }

    const Combatant& self = b.units[static_cast<std::size_t>(actor)];

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
