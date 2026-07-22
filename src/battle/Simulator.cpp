#include "battle/Simulator.hpp"

#include <utility>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"

namespace cd::battle {

namespace {

int lowestHpOnSide(const Battle& b, Side side) {
    int best = -1;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        const Combatant& u = b.units[i];
        if (u.side == side && u.alive()) {
            if (best < 0 || u.hp < b.units[static_cast<std::size_t>(best)].hp) {
                best = static_cast<int>(i);
            }
        }
    }
    return best;
}

}  // namespace

// A simple deterministic party AI: heal a badly hurt ally if able, else use the
// strongest affordable damaging skill (or a basic attack) on the weakest enemy.
EnemyChoice choosePartyAction(const Battle& b, int actor, const content::ContentDatabase& db) {
    EnemyChoice choice;
    const Combatant& self = b.units[static_cast<std::size_t>(actor)];
    // Confusion (M43): the same shared rule the battle screen and the enemy AI
    // use - a confused party member is forced to a basic attack. Without this the
    // simulator let a confused caster act normally and silently disagreed with
    // live play about the outcome.
    if (isConfused(self)) {
        return confusedChoice(b, actor);
    }
    const int enemy = lowestHpOnSide(b, Side::Enemy);

    int hurtAlly = -1;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        const Combatant& u = b.units[i];
        if (u.side == Side::Party && u.alive() && u.hp * 5 < u.maxHp * 2) {  // below 40%
            if (hurtAlly < 0 || u.hp < b.units[static_cast<std::size_t>(hurtAlly)].hp) {
                hurtAlly = static_cast<int>(i);
            }
        }
    }

    if (hurtAlly >= 0) {
        for (const std::string& sid : self.skillIds) {
            const content::SkillDef* s = db.findSkill(sid);
            if (s != nullptr && s->category == content::SkillCategory::Heal && s->mpCost <= self.mp &&
                canCast(self, *s)) {  // M35: silence blocks MP-cost skills
                choice.useSkill = true;
                choice.skillId = sid;
                choice.target = hurtAlly;
                return choice;
            }
        }
    }

    int bestPower = -1;
    for (const std::string& sid : self.skillIds) {
        const content::SkillDef* s = db.findSkill(sid);
        if (s == nullptr || s->mpCost > self.mp || !canCast(self, *s)) {  // M35 silence
            continue;
        }
        const bool damaging = s->category == content::SkillCategory::Physical ||
                              s->category == content::SkillCategory::Magic;
        if (damaging && s->power > bestPower) {
            bestPower = s->power;
            choice.useSkill = true;
            choice.skillId = sid;
            choice.target = enemy;
        }
    }
    if (!choice.useSkill) {
        choice.target = enemy;
    }
    return choice;
}

namespace {

void applyChoice(Battle& b, int actor, const EnemyChoice& choice,
                 const content::ContentDatabase& db) {
    if (choice.target < 0) {
        return;
    }
    if (choice.useSkill) {
        if (const content::SkillDef* s = db.findSkill(choice.skillId)) {
            b.useSkill(actor, choice.target, *s);
            return;
        }
    }
    b.attack(actor, choice.target);
}

}  // namespace

SimResult simulate(Battle battle, const content::ContentDatabase& db, int maxRounds) {
    return simulateInPlace(battle, db, maxRounds);
}

SimResult simulateInPlace(Battle& battle, const content::ContentDatabase& db, int maxRounds) {
    int rounds = 0;
    while (battle.outcome() == Outcome::Ongoing && rounds < maxRounds) {
        ++rounds;
        battle.beginRound();  // enmity decay at round start (M28); mirrors BattleState
        const std::vector<int> order = turnOrder(battle);
        for (int actor : order) {
            if (!battle.units[static_cast<std::size_t>(actor)].alive()) {
                continue;
            }
            battle.tickStatuses(actor);
            if (!battle.units[static_cast<std::size_t>(actor)].alive()) {
                continue;
            }
            battle.clearGuard(actor);

            if (battle.units[static_cast<std::size_t>(actor)].side == Side::Party) {
                applyChoice(battle, actor, choosePartyAction(battle, actor, db), db);
            } else {
                applyChoice(battle, actor, chooseEnemyAction(battle, actor, db), db);
            }

            if (battle.outcome() != Outcome::Ongoing) {
                break;
            }
        }
    }

    SimResult result;
    result.outcome = battle.outcome();
    result.rounds = rounds;
    for (const Combatant& u : battle.units) {
        if (u.side == Side::Party) {
            result.partyMaxHp += u.maxHp;
            result.partyHpRemaining += (u.hp > 0 ? u.hp : 0);
            if (u.alive()) {
                ++result.partyAlive;
            }
        }
    }
    return result;
}

}  // namespace cd::battle
