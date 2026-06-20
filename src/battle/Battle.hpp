#pragma once

#include <string>
#include <vector>

#include "content/Stats.hpp"

namespace cd {
struct Party;
namespace content {
class ContentDatabase;
struct SkillDef;
struct ItemDef;
}  // namespace content
namespace dungeon {
struct EnemyTeam;
}
}  // namespace cd

// Deterministic, raylib-free turn-based combat model. All resolution is pure
// (no randomness), so battles are fully reproducible and unit-tested.

namespace cd::battle {

enum class Side { Party, Enemy };
enum class Outcome { Ongoing, Victory, Defeat, Escaped };

// Reported back to the caller (the dungeon) when a battle ends.
struct BattleResult {
    Outcome outcome = Outcome::Ongoing;
    int rounds = 0;                  // battle turns (rounds) elapsed
    bool partyKoOccurred = false;    // any party member was KO'd during the fight
};

struct Combatant {
    Side side = Side::Party;
    std::string name;
    std::string sourceId;  // classId or enemyId
    int partyIndex = -1;   // index into Party.members for write-back (party only)
    content::StatBlock stats;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    std::vector<std::string> skillIds;
    bool guarding = false;
    bool isBoss = false;

    bool alive() const { return hp > 0; }
};

class Battle {
public:
    std::vector<Combatant> units;
    int turnsTaken = 0;

    bool sideAlive(Side s) const;
    Outcome outcome() const;  // Victory / Defeat / Ongoing (Escaped is set by the caller)
    std::vector<int> aliveIndices(Side s) const;

    void clearGuard(int unit);  // call at the start of a unit's turn

    // Each returns a human-readable log line. Skills deduct MP from the actor.
    std::string attack(int actor, int target);
    std::string useSkill(int actor, int primaryTarget, const content::SkillDef& skill);
    std::string useItem(int actor, int target, const content::ItemDef& item);
    std::string guard(int actor);

private:
    std::vector<int> resolveTargets(const content::SkillDef& skill, int actor,
                                    int primaryTarget) const;
};

// Builds combatants from the party and an enemy team.
Battle buildBattle(const Party& party, const dungeon::EnemyTeam& team,
                   const content::ContentDatabase& db);

// Alive units ordered by speed (desc), tie-broken Party-first then index.
std::vector<int> turnOrder(const Battle& b);

// Deterministic enemy action: heal a hurt ally if able, else use a damaging
// skill (or basic attack) on the lowest-HP living party member.
struct EnemyChoice {
    bool useSkill = false;
    int target = -1;
    std::string skillId;
};
EnemyChoice chooseEnemyAction(const Battle& b, int actor, const content::ContentDatabase& db);

}  // namespace cd::battle
