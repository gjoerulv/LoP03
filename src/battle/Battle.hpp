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

// Battle-resolution rules version. Bumped when the outcome of a battle for
// identical inputs can change, so the scoreboard can flag runs played under
// different rules. 0 = pre-M28; 1 = M28 (enmity/targeting/control skills);
// 2 = M35 (Confusion/Silence/Blind statuses + the seeded to-hit layer);
// 3 = M36 (passive skills); 4 = M43 (confusion forces a basic attack on BOTH
// sides, revive-capable heal skills, King-context items, effect-filtered item
// targeting).
inline constexpr int kBattleRulesVersion = 4;

// Blind (M35): a physical attack from a blinded unit misses this often.
inline constexpr int kBlindMissPct = 75;

// M35 status balance knobs (owner tune). Every applied status lasts this many
// times its authored duration, and poison deals this many times its authored
// per-tick damage. Confusion is additionally cleared the moment its bearer takes
// damage (see applyDamage).
inline constexpr int kStatusDurationMult = 2;
inline constexpr int kPoisonDamageMult = 2;

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

    // Passive-skill effects (M36), resolved from the equipped passive (party) or
    // the enemy/boss passive list at buildBattle, then read directly by the pure
    // methods so the sim stays db-free and deterministic. All default to
    // off/zero, so a unit with no passive resolves battles byte-identically to v2.
    std::vector<std::string> passiveIds;  // for target-info legibility
    int evasionPct = 0;                   // Evasion: physical miss chance vs this unit
    int spellWardPct = 0;                 // Spell Ward: hostile-magic fizzle chance
    int thornsPct = 0;                    // Thorns: reflected share of physical damage
    int lifedrinkPct = 0;                 // Lifedrink: healed share of physical damage dealt
    int clarityMp = 0;                    // Clarity: MP regained each round
    int keenSensesPct = 0;                // Keen Senses: bonus damage vs a debuffed target
    int bodyguardPct = 0;                 // Bodyguard: share of a hit on the weakest ally soaked
    int firstStrikeBonusPct = 0;          // First Strike: bonus on the first damaging action
    bool blindImmune = false;             // Keen Senses
    bool silenceImmune = false;           // Clarity
    bool confusionImmune = false;         // M40: the King (bespoke BossDef immunity)
    bool counterAttack = false;           // Counter Attack
    bool counteredThisRound = false;      // reset by beginRound
    bool ironWill = false;                // Iron Will
    bool ironWillUsed = false;            // once per battle
    bool firstStrike = false;             // First Strike
    bool firstStrikeUsed = false;         // once per battle

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

    // Status-gated to-hit / confusion randomness (M35). A roll cursor advanced by
    // nextRandom, drawn only when a status actually gates a roll (a blinded
    // attacker, a confused attacker). Both BattleState and the Simulator call the
    // shared attack/useSkill in the same order, so the stream evolves identically
    // for a given battle+action sequence and a status-free battle never advances
    // it (byte-identical to the pre-M35 rules). Never seeded off wall-clock time.
    std::uint64_t rollCursor = 0;
    // Unit indices that the last action missed (Blind), for the "Miss!" floaters.
    // Cleared at the start of every action so it only reflects the latest one.
    std::vector<int> lastMissed;

    // M43: this fight is the King's (set by buildBattle from the team's boss id).
    // Content-derived, never rolled, so the live screen and the Simulator read the
    // same flag from the same construction path. Items may carry King-specific
    // amounts (Royal Snacks).
    bool kingBattle = false;

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
    // M35: advance the roll cursor and return a fresh value mixed from rngSeed +
    // the cursor + a per-use salt (so Blind and Confusion draws stay independent).
    std::uint64_t nextRandom(std::uint64_t salt);
    // M35: a seeded uniform pick among the actor's own living side (incl. self),
    // or -1 if none. Advances the roll stream.
    int confusedTarget(int actor);
    // M36 passive helpers. dealPhysical/dealMagic apply a hit's final damage (with
    // the first-strike/keen-senses bonuses and the bodyguard split), plus threat,
    // thorns/lifedrink/counter (physical only), appending any passive side-effect
    // lines to `extra`; they return the damage the primary target took (for the
    // caller's log). bodyguardFor returns a living guard for the weakest ally of
    // `target`'s side, or -1. A unit with no passives makes these inert.
    int dealPhysical(int actor, int target, int baseDmg, std::string& extra);
    int dealMagic(int actor, int target, int baseDmg, std::string& extra);
    int bodyguardFor(int target) const;
};

// --- M35 status queries (pure, header-inline) ---
inline bool hasStatus(const Combatant& c, content::StatusType t) {
    for (const StatusInstance& s : c.statuses) {
        if (s.type == t) {
            return true;
        }
    }
    return false;
}
// Confusion/Silence/Blind respect immunity (M40 confusionImmune, M36 Clarity /
// Keen Senses), so an immune unit is never treated as afflicted anywhere the
// queries are used. No existing content sets confusionImmune, so every prior
// battle resolves byte-identically.
inline bool isConfused(const Combatant& c) {
    return hasStatus(c, content::StatusType::Confusion) && !c.confusionImmune;
}
inline bool isSilenced(const Combatant& c) {
    return hasStatus(c, content::StatusType::Silence) && !c.silenceImmune;
}
inline bool isBlinded(const Combatant& c) {
    return hasStatus(c, content::StatusType::Blind) && !c.blindImmune;
}
// M40: whether this unit is immune to a status type. A stored status the unit is
// immune to has no effect (the queries above ignore it), so it must never be shown
// as afflicted either — display sites skip statuses for which this is true.
inline bool isImmuneTo(const Combatant& c, content::StatusType t) {
    return (t == content::StatusType::Blind && c.blindImmune) ||
           (t == content::StatusType::Silence && c.silenceImmune) ||
           (t == content::StatusType::Confusion && c.confusionImmune);
}

// M35: may this combatant cast this skill? False only if silenced and the skill
// costs MP (silence blocks MP-cost skills; 0-MP skills, items, attacks are fine).
// MP affordability is a separate check the callers still apply.
bool canCast(const Combatant& c, const content::SkillDef& skill);

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

// M43: the forced action of a confused unit — a basic attack, never a skill.
// `attack()` then performs the seeded same-side redirect, so the returned target
// is only a nominal living opposing unit (the actor itself if its foes are all
// down). EVERY caller that decides a turn — BattleState, the Simulator, and
// chooseEnemyAction — routes confusion through this one function, so live play
// and simulation agree by construction rather than by careful duplication.
EnemyChoice confusedChoice(const Battle& b, int actor);

// M43: the party members a battle item may be used on, by effect (Heal /
// RestoreMp / Cure / None reach the living; Revive reaches only the fallen).
// Empty means the item has no legal target and must not be spent. Pure, so the
// battle screen and the tests share one rule.
std::vector<int> itemTargets(const Battle& b, Side side, const content::ItemDef& item);

// M43: the ally targets a single-ally skill may be aimed at — the living, plus
// the KO'd when the skill can revive them (SkillDef::reviveHpPct > 0).
std::vector<int> skillAllyTargets(const Battle& b, Side side, const content::SkillDef& skill);

}  // namespace cd::battle
