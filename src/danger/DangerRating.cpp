#include "danger/DangerRating.hpp"

#include <algorithm>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "game/Character.hpp"

namespace cd::danger {

namespace {

// Stat weights (scaled by 10, divided out) and skill weighting. Tunable in the
// balance pass; kept explicit so the rating is fully traceable.
constexpr int kWHp = 5;
constexpr int kWAttack = 15;
constexpr int kWMagic = 15;
constexpr int kWDefense = 10;
constexpr int kWSpeed = 8;
constexpr int kStatDivisor = 10;

int statThreat(const content::StatBlock& s) {
    return (kWHp * s.maxHp + kWAttack * s.attack + kWMagic * s.magic + kWDefense * s.defense +
            kWSpeed * s.speed) /
           kStatDivisor;
}

int skillThreat(const content::EnemyDef& def, const content::ContentDatabase& db) {
    int threat = 0;
    for (const std::string& sid : def.skills) {
        const content::SkillDef* skill = db.findSkill(sid);
        if (skill == nullptr) {
            continue;
        }
        const bool damaging = skill->category == content::SkillCategory::Physical ||
                              skill->category == content::SkillCategory::Magic;
        threat += damaging ? skill->power : skill->power / 2;
    }
    return threat;
}

}  // namespace

const char* tierName(Tier t) {
    switch (t) {
        case Tier::Trivial: return "Trivial";
        case Tier::Easy: return "Easy";
        case Tier::Fair: return "Fair";
        case Tier::Dangerous: return "Dangerous";
        case Tier::Deadly: return "Deadly";
        case Tier::Boss: return "Boss";
    }
    return "?";
}

int tierWeight(Tier t) {
    switch (t) {
        case Tier::Trivial: return 1;
        case Tier::Easy: return 2;
        case Tier::Fair: return 3;
        case Tier::Dangerous: return 4;
        case Tier::Deadly: return 5;
        case Tier::Boss: return 8;
    }
    return 0;
}

int teamThreat(const dungeon::EnemyTeam& team, const content::ContentDatabase& db) {
    int sum = 0;
    bool hasHealer = false;
    int count = 0;
    for (const std::string& id : team.enemyIds) {
        const content::EnemyDef* def = db.findEnemy(id);
        if (def == nullptr) {
            continue;
        }
        ++count;
        sum += statThreat(def->stats) + skillThreat(*def, db);
        for (const std::string& sid : def->skills) {
            const content::SkillDef* skill = db.findSkill(sid);
            if (skill != nullptr && skill->category == content::SkillCategory::Heal) {
                hasHealer = true;
            }
        }
    }
    // Team synergy: action economy (more bodies) and sustain (a healer).
    const int synergyPercent = 100 + 10 * std::max(0, count - 1) + (hasHealer ? 10 : 0);
    // Depth stat scaling (M20): the same multiplier buildBattle applies, so
    // the displayed tier matches the fight.
    return sum * synergyPercent / 100 * team.statScalePct / 100;
}

int memberThreat(const Character& c) {
    // The member's DERIVED stats (gear and milestones already folded in by
    // refreshCharacter); max HP, not current — the tier rates the matchup.
    content::StatBlock s;
    s.maxHp = c.maxHp;
    s.attack = c.stats.attack;
    s.magic = c.stats.magic;
    s.defense = c.stats.defense;
    s.speed = c.stats.speed;
    return statThreat(s);
}

int partyThreat(const std::vector<Character>& members) {
    int sum = 0;
    for (const Character& c : members) {
        sum += memberThreat(c);
    }
    return sum;
}

// M68 party-relative bands, in percent of the party's own threat. Calibrated
// against the simulator's clearing levels with town-shelf gear (the
// [danger-report] battery prints the matrix): a party that BARELY clears its
// dungeon reads the typical team Dangerous-to-Deadly; ~8-16 levels of geared
// headroom bring the typical read down to Fair, with Dangerous/Deadly left
// for the genuinely nasty compositions.
namespace {
constexpr int kTrivialBelow = 20;
constexpr int kEasyBelow = 40;
constexpr int kFairBelow = 70;
constexpr int kDangerousBelow = 110;
}  // namespace

Tier tierFor(int threat, int partyThreatValue, bool isBoss) {
    if (isBoss) {
        return Tier::Boss;
    }
    const int ratio = threat * 100 / std::max(1, partyThreatValue);
    if (ratio < kTrivialBelow) return Tier::Trivial;
    if (ratio < kEasyBelow) return Tier::Easy;
    if (ratio < kFairBelow) return Tier::Fair;
    if (ratio < kDangerousBelow) return Tier::Dangerous;
    return Tier::Deadly;
}

Tier assess(const dungeon::EnemyTeam& team, const content::ContentDatabase& db,
            int partyThreatValue) {
    return tierFor(teamThreat(team, db), partyThreatValue, team.isBoss);
}

}  // namespace cd::danger
