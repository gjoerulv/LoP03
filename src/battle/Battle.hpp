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
// targeting); 5 = M44 (the Royal Relics: enemy-targetable items, the Terrified /
// Stunned turn-control statuses, and a battle-long stat scale); 6 = M45 (the
// uncontrolled turn, the AoE basic attack with attack-applied statuses, the
// enemy-buffing heal — and the Simulator now tracks `turnsTaken`, which the
// enemy-targeting tie-break reads, so simulation and live play finally agree on
// it too); 7 = M47 (the `cleanse` skill control lifts afflictions only — the
// ATK-/DEF- debuffs now survive a Purify, while cure ITEMS are unchanged);
// 8 = M48 (element weakness/immunity: a tagged skill or weapon-elemental basic
// attack deals x1.5 to a weak foe and nothing at all — riders included — to an
// immune one); 9 = M49 (the revive clock: a boss carrying `reviveMinionTurns`
// raises its whole fallen court on the Nth of its own turns with all of them
// down, repeatably); 10 = M52 (an enemy-targeted item flagged
// `disablesMinionRevive` ends that revive clock on the boss it is used on — the
// Dragon Crown against the King — so identical inputs now resolve to a court
// that stays down; deliberately produces no log line, so the effect is hidden
// in play and recorded only in the design docs);
// 11 = M58 (two King-fight behaviour changes: a Deadly Spoon's stat halving now
// applies at most once per foe — a second spoon no longer re-halves — and the
// Hollow King has a 10%-per-living-Goose chance, each of his own turns, to be
// scared into doing nothing; the scare is a pure hash of the battle seed like the
// targeting jitter, so the Simulator and live play agree, but it changes how a
// King fight resolves for a given seed).
inline constexpr int kBattleRulesVersion = 11;

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
    // M58 (Deadly Spoon): set once a battle-long stat-scale relic has diminished
    // this unit, so a second such relic cannot halve its stats again. Battle-only
    // state, never persisted.
    bool statDiminished = false;
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

    // Class battle traits (M45), resolved from the ClassDef at buildBattle so the
    // pure model never looks a class up. All inert by default.
    bool attackHitsAll = false;                 // basic attack sweeps every foe
    // Applied per connecting basic hit. Stored as StatusInstances (the same
    // {type, magnitude, turns} triple) so the pure model needs no content type
    // here; buildBattle converts the class's authored list once.
    std::vector<StatusInstance> attackStatuses;
    bool uncontrolled = false;                  // acts on its own, seeded

    // Elements (M48), resolved at buildBattle so the pure model never reads
    // content. `weaponElement` is the element this unit's BASIC attacks carry —
    // from its equipped weapon, so enemies (which have no weapons) always swing
    // unelemented. The two lists are what this unit is weak/immune TO: empty for
    // every untagged foe and for every party member (the layer is
    // one-directional by design — see the M48 note). Stored as bare element
    // lists, like `attackStatuses`, so the pure model needs no content struct.
    content::Element weaponElement = content::Element::None;
    std::vector<content::Element> weaknesses;
    std::vector<content::Element> immunities;

    // The revive clock (M49), resolved from the BossDef at buildBattle. 0 = this
    // unit never revives its fallen court, which is every unit but the King.
    // The counter advances only on this unit's OWN turns, so it cannot drift
    // with speed or turn order the way a round counter would.
    int reviveMinionTurns = 0;
    int reviveMinionCounter = 0;

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
    // M48: unit indices the last action hit weakly / could not hurt at all, for
    // the "Weak!" / "Immune" floaters. Presentation only — nothing in the model
    // reads them — and cleared alongside lastMissed by clearActionMarks().
    std::vector<int> lastWeak;
    std::vector<int> lastImmune;

    // M43: this fight is the King's (set by buildBattle from the team's boss id).
    // Content-derived, never rolled, so the live screen and the Simulator read the
    // same flag from the same construction path. Items may carry King-specific
    // amounts (Royal Snacks).
    bool kingBattle = false;

#ifndef CRYSTAL_SHIPPING_BUILD
    // M53 debug god mode: while set, no PARTY unit can be reduced below 1 HP by
    // any damage source (the applyDamage chokepoint and the poison tick that
    // bypasses it both clamp it). Set once in the BattleState ctor from the debug
    // cheat flag; NEVER set by the Simulator or the tests, so the flag-off path
    // (default) is byte-identical and there is no kBattleRulesVersion bump. The
    // whole member is compiled out of shipping builds, so it cannot exist there.
    bool debugPartyUnkillable = false;
#endif

    bool sideAlive(Side s) const;
    Outcome outcome() const;  // Victory / Defeat / Ongoing (Escaped is set by the caller)
    std::vector<int> aliveIndices(Side s) const;

    long threatOf(int unit) const;
    void addThreat(int unit, long amount);
    // Decays threat toward zero; call once at the top of each round in both the
    // Simulator and BattleState so decay stays identical.
    void beginRound();

    void clearGuard(int unit);  // call at the start of a unit's turn (also clears intercept)
    // M49: the per-turn rules that run when a unit is about to act, after its
    // statuses have ticked and its guard has dropped. Today that is exactly one
    // rule — the King's revive clock — but the seam exists so a future per-turn
    // boss mechanic has one home that BOTH drivers already call. Returns a log
    // line, empty when nothing happened.
    std::string beginUnitTurn(int actor);
    // Drops the previous action's presentation marks (missed / weak / immune).
    // Called at the top of every action so the lists only ever describe the
    // latest one.
    void clearActionMarks();
    // Applies poison, decrements durations, removes expired statuses. Returns a
    // log line (empty if nothing happened). Call at the start of a unit's turn.
    std::string tickStatuses(int unit);

    // Each returns a human-readable log line. Skills deduct MP from the actor.
    // `attack` dispatches to one strike, or (M45) a sweep of every living foe.
    std::string attack(int actor, int target);
    std::string useSkill(int actor, int primaryTarget, const content::SkillDef& skill);
    std::string useItem(int actor, int target, const content::ItemDef& item);
    std::string guard(int actor);

private:
    // M53: promoted from a file-local free function to a member so it can honour
    // the debug god-mode clamp without threading a flag through its six callers
    // (all of which are already Battle methods). Applies `dmg` to `d`, honouring
    // Iron Will and (debug builds only) party god mode; snaps the bearer out of
    // confusion on any real hit.
    void applyDamage(Combatant& d, int dmg);

    std::string attackOne(int actor, int target);   // one strike (the pre-M45 attack)
    std::string attackAll(int actor);               // M45: one strike per living foe
    std::string applyAttackStatuses(int actor, int target);  // M45: the Dragon's bite
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
// M48 — the element rule, as one pure function. Returns a PERCENTAGE applied to
// already-computed damage: 150 when `defender` is weak to `e`, 0 when it is
// immune, 100 otherwise (including `Element::None`, every untagged foe, and every
// party member). It is a lookup, never a roll — `rollCursor` is untouched, so
// adding elements changed no seeded stream.
//
// Applied as the LAST step inside physicalDamage/magicDamage, after their
// max(1, ...) floor — otherwise an immune hit would deal the floor's 1 instead of
// 0. Both drivers reach those helpers through the same five call sites, so sim ==
// live by construction.
inline constexpr int kElementWeakPct = 150;
inline constexpr int kElementImmunePct = 0;
int elementModifier(const Combatant& defender, content::Element e);

// True when `e` deals nothing to `defender` — the same rule as above, named for
// the places that must ALSO skip the rider (an immune hit applies no
// attack-status and no skill status).
bool isImmuneToElement(const Combatant& defender, content::Element e);

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
// M44: an action a unit does not get to choose. `None` means it acts normally;
// the rest are imposed by a status and resolved identically wherever a turn is
// decided (see forcedActionFor).
enum class ForcedAction { None, BasicAttack, Guard, Skip };

struct EnemyChoice {
    bool useSkill = false;
    int target = -1;
    std::string skillId;
    ForcedAction forced = ForcedAction::None;  // M44: set when the turn was taken away
};
EnemyChoice chooseEnemyAction(const Battle& b, int actor, const content::ContentDatabase& db);

// M44: does a status take this unit's turn away, and how? The single source of
// truth for every imposed action — Confusion (M35/M43) forces a basic attack,
// Terrified forces a Guard, Stunned skips the turn entirely. Immunities are
// honoured through the isConfused-style queries. Pure.
ForcedAction forcedActionFor(const Combatant& c);

// M44: the EnemyChoice a forced action resolves to, ready to apply. Callers that
// decide turns (BattleState, the Simulator, chooseEnemyAction) ask this once at
// the top of a turn and obey it — the M43 lesson, generalized.
EnemyChoice forcedChoice(const Battle& b, int actor, ForcedAction forced);

// M45: the turn an UNCONTROLLED unit (the Jester) takes for itself — a seeded
// pick among its affordable, castable known skills plus the basic attack, aimed
// at a random living foe. Pure: it hashes (rngSeed, turnsTaken, actor) the way
// `targetJitter` does and never advances `rollCursor`, so BattleState, the
// Simulator, and chooseEnemyAction all derive the same turn without having to
// consume the roll stream in the same order.
EnemyChoice uncontrolledChoice(const Battle& b, int actor, const content::ContentDatabase& db);

// M45: does the Jester quip this turn, and which line? `index` is only meaningful
// when it returns true. Presentation only — a pure hash under its own salt that
// never touches `rollCursor`, so showing (or hiding) a jest cannot change how a
// battle resolves.
bool jestThisTurn(const Battle& b, int actor, int lineCount, int& index);

// M58: how many living Goose-class party members are present in a King fight
// (0 outside a King fight). Read by the scare rule below.
int geeseScaringKing(const Battle& b);

// M58: is the Hollow King scared into doing nothing on `actor`'s turn? True only
// when `actor` is the boss of a King fight and a per-turn roll lands under
// 10% × living Geese. Like `jestThisTurn`/`uncontrolledChoice` this is a PURE hash
// of (rngSeed, turnsTaken, actor) under its own salt, so it never advances
// `rollCursor` yet resolves identically in the Simulator and live play — but
// UNLIKE a quip it feeds `chooseEnemyAction` and so DOES change how a King fight
// resolves (hence the kBattleRulesVersion bump). BattleState also calls it to
// choose the "geese scare" flavour over the ordinary skip line.
bool kingScaredThisTurn(const Battle& b, int actor);

// M43: the forced action of a confused unit — a basic attack, never a skill.
// `attack()` then performs the seeded same-side redirect, so the returned target
// is only a nominal living opposing unit (the actor itself if its foes are all
// down). EVERY caller that decides a turn — BattleState, the Simulator, and
// chooseEnemyAction — routes confusion through this one function, so live play
// and simulation agree by construction rather than by careful duplication.
EnemyChoice confusedChoice(const Battle& b, int actor);

// M43: the units a battle item may be used on, from the actor's side. An ally
// item filters by effect (Heal / RestoreMp / Cure / None reach the living; Revive
// reaches only the fallen); an enemy item (M44's relics) reaches the living foes
// of `side`. Empty means the item has no legal target and must not be spent.
// Pure, so the battle screen and the tests share one rule.
std::vector<int> itemTargets(const Battle& b, Side side, const content::ItemDef& item);

// M44: would this item do anything at all to this target? False only for an item
// restricted to a boss (`requiresBossId`) used on anything else — the caller keeps
// such an item instead of spending it on nothing.
bool itemAffects(const Battle& b, int target, const content::ItemDef& item);

// M43: the ally targets a single-ally skill may be aimed at — the living, plus
// the KO'd when the skill can revive them (SkillDef::reviveHpPct > 0).
std::vector<int> skillAllyTargets(const Battle& b, Side side, const content::SkillDef& skill);

}  // namespace cd::battle
