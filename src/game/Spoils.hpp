#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Guild.hpp"  // M84: town-perk EXP/gold bonuses
#include "game/Milestones.hpp"
#include "game/Party.hpp"
#include "game/Scrolls.hpp"

// Battle spoils (M68): what a defeated team pays, and the ONE rule that
// applies it to the party while reporting per-member level-up diffs for the
// victory panel. Pure and raylib-free; BattleState displays what this
// computed, so the panel can never disagree with the award.

namespace cd {

// The team's base payout: every enemy's reward plus the boss's. XP is granted
// party-wide (each member receives the full amount, the M2 rule).
struct BattleSpoils {
    int xp = 0;
    int gold = 0;
};

inline BattleSpoils teamSpoils(const dungeon::EnemyTeam& team,
                               const content::ContentDatabase& db) {
    BattleSpoils s;
    for (const std::string& id : team.enemyIds) {
        if (const content::EnemyDef* e = db.findEnemy(id)) {
            s.xp += e->xpReward;
            s.gold += e->goldReward;
        }
    }
    if (const content::BossDef* boss = db.findBoss(team.bossId)) {
        s.xp += boss->xpReward;
        s.gold += boss->goldReward;
    }
    // M93 (owner decision 7): a danger-counter patrol pays XP but never gold —
    // circling for step-triggered fights must not become a gold farm. One
    // shared rule here, so the victory panel and the award agree.
    if (team.patrol) {
        s.gold = 0;
    }
    return s;
}

// One member's level-up, as shown on the victory panel: the level motion, the
// derived-stat deltas (gear and milestones included — refreshCharacter runs
// inside grantXp), and any skills the new level unlocked.
struct LevelUpDiff {
    std::string name;
    int fromLevel = 0;
    int toLevel = 0;
    int hpDelta = 0;   // max HP
    int mpDelta = 0;   // max MP
    int atkDelta = 0;
    int magDelta = 0;
    int defDelta = 0;
    int spdDelta = 0;
    std::vector<std::string> newSkillNames;  // display names, learnset order
};

struct SpoilsResult {
    int xp = 0;    // per member, as granted
    int gold = 0;  // total credited, standing-member bonuses included
    std::vector<LevelUpDiff> levelUps;
};

// Applies the payout to the party and reports what changed. Call with the
// party's post-battle HP already written back: the M63 standing-member gold
// bonuses (Cutpurse, Golden Goose) only count members left on their feet.
inline SpoilsResult applySpoils(Party& party, const BattleSpoils& spoils,
                                const content::ContentDatabase& db) {
    SpoilsResult out;
    // M84 town perks: Guild Lessons/Mastery raise battle EXP, the Bounty
    // Clause raises battle gold — additive with the M63 standing-member
    // bonuses, and applied HERE so the victory panel shows what was granted.
    out.xp = spoils.xp + spoils.xp * guildExpBonusPct(party.guild) / 100;
    // M103 (Sacrifice event): the offered gear buys ONE doubled award — read
    // and cleared here so the victory panel shows exactly what was granted.
    if (party.doubleXpNext) {
        out.xp *= 2;
        party.doubleXpNext = false;
    }
    out.gold = spoils.gold +
               spoils.gold *
                   (partyGoldBonusPct(party.members, db) + guildEnemyGoldPct(party.guild)) / 100;
    party.gold += out.gold;

    struct Snapshot {
        int level, maxHp, maxMp, atk, mag, def, spd;
        std::vector<std::string> known;
    };
    std::vector<Snapshot> before;
    before.reserve(party.members.size());
    for (const Character& c : party.members) {
        before.push_back({c.level, c.maxHp, c.maxMp, c.stats.attack, c.stats.magic,
                          c.stats.defense, c.stats.speed, allKnownSkills(c, db)});
    }

    // M102 (owner decision): the fallen earn nothing at the end of a fight —
    // only members still standing receive the XP. This also removes the old
    // silent "auto-revive": a KO'd member can no longer level up mid-award and
    // come back on the level-up heal (Party.cpp's HP-delta rule). Sanctioned
    // revives remain the Phoenix Tear (field or battle), Renew (battle), and
    // the Inn. Elder Root / Training Hall XP is deliberately NOT filtered —
    // paid tuition is not a fight.
    for (Character& c : party.members) {
        if (c.hp > 0) {
            grantXp(c, out.xp, db);  // the perk-adjusted amount (M84)
        }
    }

    for (std::size_t i = 0; i < party.members.size(); ++i) {
        const Character& c = party.members[i];
        const Snapshot& b = before[i];
        if (c.level <= b.level) {
            continue;
        }
        LevelUpDiff d;
        d.name = c.name;
        d.fromLevel = b.level;
        d.toLevel = c.level;
        d.hpDelta = c.maxHp - b.maxHp;
        d.mpDelta = c.maxMp - b.maxMp;
        d.atkDelta = c.stats.attack - b.atk;
        d.magDelta = c.stats.magic - b.mag;
        d.defDelta = c.stats.defense - b.def;
        d.spdDelta = c.stats.speed - b.spd;
        for (const std::string& id : allKnownSkills(c, db)) {
            if (std::find(b.known.begin(), b.known.end(), id) == b.known.end()) {
                const content::SkillDef* s = db.findSkill(id);
                d.newSkillNames.push_back(s != nullptr ? s->name : id);
            }
        }
        out.levelUps.push_back(std::move(d));
    }
    return out;
}

}  // namespace cd
