#pragma once

#include <map>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "content/Enums.hpp"
#include "content/Stats.hpp"
#include "dungeon/DungeonModel.hpp"

// M88: the pre-fight team inspection body — pure and raylib-free, rendered by
// the dungeon's Details overlay. It shows what buildBattle will actually
// field: stats go through the SAME content::scaledStats(team.statScalePct)
// multiply, and affinities/passives disclose exactly what the in-battle
// target panel already shows while aiming (M48/M36 — never bestiary-gated),
// so this panel reveals nothing the first round of combat would not. Its
// point is owner item 5: judge a team BEFORE engaging, and gear up for it.

namespace cd::dungeon {

namespace detail {

inline std::string joinElements(const std::vector<content::Element>& elements) {
    std::string out;
    for (content::Element e : elements) {
        if (!out.empty()) {
            out += ", ";
        }
        out += content::toString(e);
    }
    return out;
}

inline std::string joinPassiveNames(const std::vector<std::string>& ids,
                                    const content::ContentDatabase& db) {
    std::string out;
    for (const std::string& id : ids) {
        if (!out.empty()) {
            out += ", ";
        }
        const content::PassiveDef* p = db.findPassive(id);
        out += p != nullptr ? p->name : id;
    }
    return out;
}

// One combatant's block: name (xN when the team fields several), the scaled
// stats as battle will build them, then affinity and passive lines only when
// the foe has any — a plain foe stays a plain two lines.
inline std::string memberBlock(const std::string& name, int copies,
                               const content::StatBlock& scaled,
                               const content::ElementAffinity& affinity,
                               const std::vector<std::string>& passiveIds,
                               const content::ContentDatabase& db) {
    std::string out = name;
    if (copies > 1) {
        out += " x" + std::to_string(copies);
    }
    out += "\nHP " + std::to_string(scaled.maxHp) + "  ATK " + std::to_string(scaled.attack) +
           "  MAG " + std::to_string(scaled.magic) + "  DEF " + std::to_string(scaled.defense) +
           "  SPD " + std::to_string(scaled.speed);
    if (!affinity.weaknesses.empty()) {
        out += "\nWeak: " + joinElements(affinity.weaknesses);
    }
    if (!affinity.immunities.empty()) {
        out += "\nImmune: " + joinElements(affinity.immunities);
    }
    if (!passiveIds.empty()) {
        out += "\nPassive: " + joinPassiveNames(passiveIds, db);
    }
    return out;
}

}  // namespace detail

// The full inspection body for a faced team. `tierName` is the run's
// snapshotted danger tier (M68 — the label the map already shows).
inline std::string describeTeam(const EnemyTeam& team, const std::string& tierName,
                                const content::ContentDatabase& db) {
    std::string body = team.name + " - " + tierName + ", " + std::to_string(team.count()) +
                       (team.count() == 1 ? " enemy." : " enemies.");

    if (!team.bossId.empty()) {
        if (const content::BossDef* boss = db.findBoss(team.bossId)) {
            body += "\n\n" + detail::memberBlock(
                                 boss->name, 1,
                                 content::scaledStats(boss->stats, team.statScalePct),
                                 boss->affinity, boss->passives, db);
        }
    }

    // Collapse duplicate ids (a team may field the same foe several times)
    // while keeping first-appearance order.
    std::vector<std::string> order;
    std::map<std::string, int> copies;
    for (const std::string& id : team.enemyIds) {
        if (copies[id]++ == 0) {
            order.push_back(id);
        }
    }
    for (const std::string& id : order) {
        if (const content::EnemyDef* e = db.findEnemy(id)) {
            body += "\n\n" + detail::memberBlock(
                                 e->name, copies[id],
                                 content::scaledStats(e->stats, team.statScalePct),
                                 e->affinity, e->passives, db);
        }
    }
    return body;
}

}  // namespace cd::dungeon
