#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "content/Enums.hpp"
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

// Battle-resolution rules version (M28). Bumped when the outcome of a battle
// for identical inputs changes (targeting/enmity/control skills), so the
// scoreboard can flag runs played under different rules. 0 = pre-M28.
inline constexpr int kBattleRulesVersion = 1;

enum class Side { Party, Enemy };
enum class Outcome { Ongoing, Victory, Defeat, Escaped };

struct StatusInstance {
    content::StatusType type = content::StatusType::None;
    int magnitude = 0;  // poison damage, or buff/debuff percent
    int turns = 0;      // remaining turns
};

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
    std::vector<StatusInstance> statuses;
    bool guarding = false;
    // Redirect/intercept (M28): while set, this (party) unit takes single-target
    // enemy hits aimed at its allies. Cleared at the unit's next turn, like
    // guard.
    bool intercepting = false;
    bool isBoss = false;
    // Boss archetype mechanics (M20, owner-approved; all deterministic).
    bool enrages = false;             // Brute: deals more damage below half HP
    bool enrageAnnounced = false;     // Brute: the rage line is shown once
    bool empowersOnAllyFall = false;  // Sorcerer: magic +25% per fallen ally
    bool ralliesMinions = false;      // Commander: one rally below half HP
    bool rallied = false;
    bool rushOpener = false;          // Rush: its first action deals double damage
    bool actedOnce = false;
    std::string telegraph;       // boss intro line

    bool alive() const { return hp > 0; }
};

class Battle {
public:
    std::vector<Combatant> units;
    int turnsTaken = 0;

    // Enmity (M28): global threat per unit (only party units accrue it; enemies
    // read it to pick targets). Mutated only through the shared attack/useSkill
    // paths, so live play and the Simulator stay in exact agreement. `rngSeed`
    // seeds the small, deterministic targeting tie-break jitter (a pure hash of
    // seed+turn+actor+candidate — no evolving RNG state to keep in sync).
    std::vector<long> threat;
    std::uint64_t rngSeed = 0;

    bool sideAlive(Side s) const;
    Outcome outcome() const;  // Victory / Defeat / Ongoing (Escaped is set by the caller)
    std::vector<int> aliveIndices(Side s) const;

    long threatOf(int unit) const;
    void addThreat(int unit, long amount);
    // Decays threat toward zero; call once at the top of each round in both the
    // Simulator and BattleState so decay stays identical.
    void beginRound();

    void clearGuard(int unit);  // call at the start of a unit's turn (also clears intercept)
    // Applies poison, decrements durations, removes expired statuses. Returns a
    // log line (empty if nothing happened). Call at the start of a unit's turn.
    std::string tickStatuses(int unit);

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

// Deterministic enemy action (M28): heal a hurt ally or apply a buff/debuff if
// warranted, else pick a damaging skill (or basic attack) against the party
// member that best matches this enemy's targeting profile (derived from its
// role) — weighing accrued threat, kill pressure, and the backline, with a
// small seeded tie-break. Pure and reproducible, so live play and the Simulator
// agree exactly.
struct EnemyChoice {
    bool useSkill = false;
    int target = -1;
    std::string skillId;
};
EnemyChoice chooseEnemyAction(const Battle& b, int actor, const content::ContentDatabase& db);

}  // namespace cd::battle
