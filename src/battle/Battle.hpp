#pragma once

#include <array>
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

struct BattleObserver;  // M60 record-only telemetry hook (battle/BattleObserver.hpp)

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
// King fight resolves for a given seed);
// 12 = M61 (the Goose Town rules, all schema-driven and inert for every prior
// foe: an enemy authored `doNothingPct` may simply do nothing on its own turn
// — a pure seeded hash like the King's scare, its flavour line authored as
// `doNothingText` ("Quack.") — a boss authored `attackHitsAll` /
// `attackStatuses` swings the M45 class machinery from the enemy side, and a
// boss authored `immuneToAfflictions` shrugs off every affliction — poison,
// confusion, silence, blind, terrified, stunned — while ATK-/DEF- debuffs
// still land. No shipped pre-M61 content carries any of the fields, so every
// earlier battle resolves byte-identically);
// 13 = M62 (Purify heals nothing, at last: a pure cleanse — a heal-category
// skill with power 0 and the cleanse control — no longer applies the heal
// formula's magic/2 term. The skill's description promised this since M43;
// the code now keeps the promise. Cleanses with real power — Generous
// Mending — still heal. Changes how any battle containing a Purify cast
// resolves, hence the bump);
// 14 = M63 (class level milestones: at levels 10/20/30 a party member's
// chosen data/milestones.json bonus resolves onto its Combatant at
// buildBattle — damage/heal/status modifiers, guard-block and weakness
// overrides, double strikes and reduced-strength sweeps, first-hit
// immunity, Iron Will healing, on-kill/on-death triggers, and grants of
// the existing passive hooks. A party with no chosen milestones resolves
// byte-identically; any chosen battle-side milestone changes outcomes,
// hence the bump);
// 15 = M75 (the program's one engine revision, batched deliberately so the
// scoreboard tags a single rules change: THREE new statuses — Reflect
// (hostile magic bounces back onto its caster), Sleep (the bearer skips its
// turns; any damage except a poison tick wakes it), Curse (outgoing damage
// halved, skill MP costs doubled, duration x1.5) — plus their removers
// (`break_reflect` / `uncurse` skill effects, a `curesCurse` item flag, and
// Sleep joining the cleanse/cure sets); poison ticks now scale with the
// applier's Magic (snapshotted at application); ATK+/- now scales the whole
// (attack + power) term and DEF+/- scales the FINAL damage taken (both were
// stat-only nudges too small to matter); damaging skills may drain
// `mpDamagePct` of the HP damage as MP; foes may start the battle with
// `initialStatuses`; the deterministic trigger framework (every-Nth-hit,
// first-time-below-HP%, every-Nth-own-turn, first-ally-felled conditions;
// status/stat-scale/clone/MP-drain actions) evaluated in shared code with no
// new rolls; sleep-aware enemy targeting; `immuneToStatScale` (a boss the
// Deadly Spoon cannot diminish); per-status immunity lists; and worn
// equipment may resist elements (`resistElements`/`resistPct`, content in
// M81). A battle whose content carries none of the new fields AND no
// poison/ATK+-/DEF+- status changes nothing; any battle where those statuses
// appear resolves differently — that rebalance is the point, hence the bump).
// M77 amended v15 in two places its shipped content is the first to reach —
// the enemy AI's support-loop gate for all_enemies support skills reads the
// profiled party target instead of the caster, and trigger-borne stuns honour
// `noStunWhileAllFoesSleep` — both provably unreachable by pre-M77 content
// (no such skill in any earlier kit; no earlier foe carries a trigger), so no
// recorded battle changes and the version holds at 15 (see the M77 note §E).
// 16 = M89 (an authored `maxMp` override replaces the derived from-magic MP
// pool on any enemy/boss that carries it, scaled like magic; a boss authored
// `basicAttackEveryNth` swings instead of casting on every Nth of its OWN
// turns — deterministic, counter-based like the M49 revive clock. Shipped
// content: the Last Dragon, whose 500%-scaled ~130 MP dried after six of its
// 20-MP breaths; it now sustains breaths for the whole fight and lunges every
// 4th turn); 17 = M95 (summons: a skill authored `oncePerRun` is castable
// once per dungeon/challenge run — the ledger rides the Battle, copied from
// the party at build, appended by useSkill's shared refusal rule, written
// back with HP/MP — so the menu, both AIs, and the Simulator refuse a spent
// summon identically); 18 = M96 (heirlooms: the worn fourth slot attaches
// its M75 triggers to the wearer — the first PARTY-side trigger source —
// TriggerDo gains heal_self_pct, and the item's lowHp* pair is a conditional
// attack edge on the Brute-enrage pattern, announced once. A party wearing
// no heirloom resolves byte-identically to v17).
inline constexpr int kBattleRulesVersion = 18;

// Blind (M35): a physical attack from a blinded unit misses this often.
inline constexpr int kBlindMissPct = 75;

// M35 status balance knobs (owner tune). Every applied status lasts this many
// times its authored duration, and poison deals this many times its authored
// per-tick damage. Confusion is additionally cleared the moment its bearer takes
// damage (see applyDamage).
inline constexpr int kStatusDurationMult = 2;
inline constexpr int kPoisonDamageMult = 2;
// M75: Curse lasts 50% longer than every other status (owner decision
// 2026-08-05) — its scaled duration is multiplied by this percent in addStatus.
inline constexpr int kCurseDurationPct = 150;
// M75: poison ticks scale with the APPLIER's Magic — the authored magnitude
// gains caster Magic divided by this, snapshotted at application, so flat
// authored poisons stay relevant against endgame HP pools.
inline constexpr int kPoisonMagicDiv = 4;

enum class Side { Party, Enemy };
enum class Outcome { Ongoing, Victory, Defeat, Escaped };

struct StatusInstance {
    content::StatusType type = content::StatusType::None;
    int magnitude = 0;  // poison damage, or buff/debuff percent
    int turns = 0;      // remaining turns
};

// A resolved boss/elite trigger (M75): the content TriggerDef mirrored into
// plain fields plus its runtime state, the StatusInstance precedent — the pure
// model never needs a content struct. buildBattle converts the authored list.
struct TriggerRule {
    content::TriggerWhen when = content::TriggerWhen::None;
    int threshold = 0;
    content::TriggerDo action = content::TriggerDo::None;
    content::StatusType status = content::StatusType::None;
    int magnitude = 0;
    int duration = 0;
    int scaleAttackPct = 100;
    int scaleMagicPct = 100;
    int scaleDefensePct = 100;
    int scaleSpeedPct = 100;
    int mpDrainPct = 0;
    std::string text;   // authored announcement (may be empty)
    bool fired = false; // FirstTime* conditions fire exactly once
    int counter = 0;    // EveryNthOwnTurn's own-turn count
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
    // M96 (rules v18): a worn heirloom's conditional edge — the enrage
    // pattern, authored: while hp <= lowHpThresholdPct% of max, attacks hit
    // lowHpAttackPct% harder. Announced once via lowHpText. 0/0 = no heirloom
    // edge, which is every unit whose wearer carries none.
    int lowHpThresholdPct = 0;
    int lowHpAttackPct = 0;
    bool lowHpAnnounced = false;
    std::string lowHpText;
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
    // pure model never looks a class up. All inert by default. M61: bosses may
    // carry the same two (the Deadly Duck) — buildBattle resolves either source
    // into these fields, so the pure model never knows which side authored them.
    bool attackHitsAll = false;                 // basic attack sweeps every foe
    // Applied per connecting basic hit. Stored as StatusInstances (the same
    // {type, magnitude, turns} triple) so the pure model needs no content type
    // here; buildBattle converts the class's authored list once.
    std::vector<StatusInstance> attackStatuses;
    bool uncontrolled = false;                  // acts on its own, seeded

    // M61 (the Goose Town): a foe authored `doNothingPct` may spend its own turn
    // doing nothing — a pure seeded roll (see doesNothingThisTurn), flavour text
    // authored alongside it. A boss authored `immuneToAfflictions` blocks EVERY
    // affliction at the addStatus chokepoint (poison/confusion/silence/blind/
    // terrified/stunned) while stat debuffs still land. Both default inert.
    int doNothingPct = 0;
    std::string doNothingText;
    bool afflictionImmune = false;

    // M63 level-milestone battle effects, resolved from the character's chosen
    // milestones at buildBattle (party members only; every field default-inert
    // so a milestone-free battle is byte-identical). Grants of existing passive
    // hooks reuse the M36 fields above.
    int basicAttackPct = 0;       // basic attacks deal +N%
    int magicSkillPct = 0;        // magic-category skills deal +N%
    int aoeSpellPct = 0;          // all-enemy skills deal +N%
    int healCastPct = 0;          // heals this unit casts restore +N%
    int executePct = 0;           // +N% damage vs foes below half HP
    int vsAfflictedPct = 0;       // +N% damage vs foes carrying any negative status
    int weaknessBonusPct = 0;     // this attacker's weak hits deal N% (0 = the 150 default)
    int statusTurnsBonus = 0;     // statuses this unit applies last +N effective turns
    int doubleStrikePct = 0;      // basic attack strikes twice; the second at N%
    int sweepScalePct = 0;        // its attackHitsAll sweep strikes at N% (0 = full)
    int tauntDebuffPct = 0;       // its Taunt also inflicts ATK- (N%) on every foe
    int guardBlockPct = 0;        // guarding blocks N% (0 = the 50 default)
    bool firstHitImmune = false;  // the first damaging hit taken deals 0, once
    bool firstHitImmuneUsed = false;
    int ironWillHealPct = 0;      // Iron Will restores N% max HP when it fires
    int reviveAtPct = 0;          // revive-capable heals raise at N% when higher
    bool purifyHeals = false;     // its pure cleanses heal again (undoes M62 for it)
    int goldBonusPct = 0;         // battle gold +N% while it stands (partyGoldBonusPct)
    int itemPotencyPct = 0;       // items this unit uses are +N% potent
    bool noEnemyBuff = false;     // suppresses `alsoBuffsEnemies` on its casts
    int onKillPartyAtkUpPct = 0;  // felling a foe: its side gains ATK+ (N%)
    int onDeathFoeDebuffPct = 0;  // falling: the other side suffers ATK-/DEF- (N%)

    // Elements (M48), resolved at buildBattle so the pure model never reads
    // content. `weaponElement` is the element this unit's BASIC attacks carry —
    // from its equipped weapon, so enemies (which have no weapons) always swing
    // unelemented. The two lists are what this unit is weak/immune TO: empty for
    // every untagged foe and for every party member (the layer is
    // one-directional by design — see the M48 note). Stored as bare element
    // lists, like `attackStatuses`, so the pure model needs no content struct.
    content::Element weaponElement = content::Element::None;
    // M85: true when `weaponElement` came from a chosen class milestone (the
    // M63 Fire bite / Holy basic) rather than a wielded weapon. An INTRINSIC
    // element is never nullified (the M81-narrowed M48 absolute): against a
    // foe immune to it, the hit resolves at the neutral 100% instead of 0.
    // Inert for every shipped foe until M85's Dragon — the [elements] lint
    // has always guaranteed no fire/holy immunity existed anywhere.
    bool elementIntrinsic = false;
    std::vector<content::Element> weaknesses;
    std::vector<content::Element> immunities;

    // The revive clock (M49), resolved from the BossDef at buildBattle. 0 = this
    // unit never revives its fallen court, which is every unit but the King.
    // The counter advances only on this unit's OWN turns, so it cannot drift
    // with speed or turn order the way a round counter would.
    int reviveMinionTurns = 0;
    int reviveMinionCounter = 0;

    // M89 (rules v16): own-turn ordinal, advanced for EVERY unit in
    // beginUnitTurn (the same seam as the revive clock, so it cannot drift
    // with speed or turn order). Read by `basicAttackTurn` below: a boss
    // authored `basicAttackEveryNth` = N swings instead of casting on every
    // Nth of its own turns (the Dragon's lunge). 0 = the rule is off, which
    // is every pre-M89 unit. `basicAttackText` is the authored flavour line
    // (presentation-only, shown by BattleState like doNothingText).
    int ownTurnsTaken = 0;
    int basicAttackEveryNth = 0;
    std::string basicAttackText;

    // M75 (rules v15), every field inert by default so pre-M75 content is
    // untouched. `statusImmunities` extends the bespoke immunity flags with a
    // per-status list (the Dragon's matrix); `statScaleImmune` shrugs off a
    // battle-long stat-scale relic (the Deadly Spoon); the two AI manners are
    // read by chooseEnemyAction; `elementResist` (indexed by content::Element,
    // resolved from worn equipment) reduces incoming damage of that element in
    // elementModifier; `triggers` + `hitsTaken` drive the trigger framework;
    // `summonSlot` marks a prebuilt clone that lies dead until a summon_clone
    // trigger raises it (built at buildBattle so the unit roster never grows
    // mid-battle).
    std::vector<content::StatusType> statusImmunities;
    bool statScaleImmune = false;
    bool avoidSleepingTargets = false;
    bool noStunWhileAllFoesSleep = false;
    std::array<int, 7> elementResist{};  // one slot per content::Element value
    std::vector<TriggerRule> triggers;
    int hitsTaken = 0;
    bool summonSlot = false;

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

    // M95 (rules v17): summons cast THIS RUN — copied from Party at build,
    // appended by useSkill's shared once-per-run rule, written back by the
    // battle screen with HP/MP. The Simulator sees and honors the same list.
    std::vector<std::string> usedSummons;

#ifndef CRYSTAL_SHIPPING_BUILD
    // M53 debug god mode: while set, no PARTY unit can be reduced below 1 HP by
    // any damage source (the applyDamage chokepoint and the poison tick that
    // bypasses it both clamp it). Set once in the BattleState ctor from the debug
    // cheat flag; NEVER set by the Simulator or the tests, so the flag-off path
    // (default) is byte-identical and there is no kBattleRulesVersion bump. The
    // whole member is compiled out of shipping builds, so it cannot exist there.
    bool debugPartyUnkillable = false;
#endif

    // M60: record-only telemetry hook (see battle/BattleObserver.hpp for the
    // full contract). Non-owning, default null — the game and the Simulator
    // never set it; the editor's sim lab and the parity test do. Null means
    // every emit site is a single skipped branch: outcomes and rollCursor are
    // byte-identical either way, so there is no rules-version bump. A raw
    // pointer keeps Battle trivially copyable (copies share the recorder).
    BattleObserver* observer = nullptr;

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
    // M63: `extra` (when the caller has a log to grow) receives the
    // first-hit-glance and on-death lines; nullptr callers stay silent.
    void applyDamage(Combatant& d, int dmg, std::string* extra = nullptr);
    // M63 (Standing Ovation): called where an explicit killer is known.
    std::string rallyOnKill(int killer);

    // M63: `scalePct` scales the strike (a Double Nock second arrow, a Rain
    // of Arrows sweep); 100 = the ordinary full-strength attack.
    std::string attackOne(int actor, int target, int scalePct = 100);
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

    // M75 trigger framework (all shared code, so sim == live by construction).
    // reviveCourtRule is the M49 revive clock, extracted so beginUnitTurn can
    // compose it with the turn-start triggers. fireTurnTriggers evaluates the
    // state conditions at the start of the bearer's own turn; fireHitTriggers
    // reacts to a deliberate connecting hit (EveryNthHitTaken) from inside
    // dealPhysical/dealMagic; applyTriggerAction executes one fired rule
    // (`attacker` only meaningful for hit-reactive rules, -1 otherwise).
    std::string reviveCourtRule(int actor);
    std::string fireTurnTriggers(int actor);
    std::string fireHitTriggers(int target, int attacker);
    std::string applyTriggerAction(int owner, TriggerRule& tr, int attacker);
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
// M61: is this status an AFFLICTION — a "bad status" in the owner's sense?
// Poison, the M35 control trio, the M44 turn-takers, and (M75) Sleep and
// Curse. Deliberately broader than a cleanse's reach (a cleanse cannot refund
// a turn-control status or lift a Curse, but immunity stops one from ever
// landing). The ATK-/DEF- stat debuffs are NOT afflictions: an
// affliction-immune boss can still be debuffed by design. Reflect is a
// BENEFICIAL status and never an affliction.
inline bool isAffliction(content::StatusType t) {
    return t == content::StatusType::Poison || t == content::StatusType::Confusion ||
           t == content::StatusType::Silence || t == content::StatusType::Blind ||
           t == content::StatusType::Terrified || t == content::StatusType::Stunned ||
           t == content::StatusType::Sleep || t == content::StatusType::Curse;
}

// M40: whether this unit is immune to a status type. A stored status the unit is
// immune to has no effect (the queries above ignore it), so it must never be shown
// as afflicted either — display sites skip statuses for which this is true.
// M61: `afflictionImmune` (the Deadly Duck) covers every affliction at once.
// M75: `statusImmunities` lists bespoke per-status immunities (the Dragon).
inline bool isImmuneTo(const Combatant& c, content::StatusType t) {
    for (content::StatusType x : c.statusImmunities) {
        if (x == t) {
            return true;
        }
    }
    return (c.afflictionImmune && isAffliction(t)) ||
           (t == content::StatusType::Blind && c.blindImmune) ||
           (t == content::StatusType::Silence && c.silenceImmune) ||
           (t == content::StatusType::Confusion && c.confusionImmune);
}

// M75 status queries, the isConfused shape: immunity is honoured so an immune
// unit never reads as affected anywhere these are used.
inline bool isAsleep(const Combatant& c) {
    return hasStatus(c, content::StatusType::Sleep) &&
           !isImmuneTo(c, content::StatusType::Sleep);
}
inline bool isCursed(const Combatant& c) {
    return hasStatus(c, content::StatusType::Curse) &&
           !isImmuneTo(c, content::StatusType::Curse);
}
inline bool hasReflect(const Combatant& c) {
    return hasStatus(c, content::StatusType::Reflect);
}

// M75: the MP a skill actually costs THIS caster — double under a Curse. The
// single rule every affordability check and the deduction itself must share
// (BattleState's menu, both AIs, useSkill), or a cursed caster could pick a
// skill it cannot pay for and desync live play from the Simulator.
int mpCostFor(const Combatant& c, const content::SkillDef& skill);

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

// M95 (rules v17): has this run already spent the summon? False for every
// non-summon skill. One shared query for the battle menu ("USED"), both AIs,
// and useSkill's refusal, so a spent summon is refused identically everywhere.
bool summonSpent(const Battle& b, const content::SkillDef& skill);

// M96 (rules v18): is the wearer's heirloom edge biting right now (HP at or
// under the authored threshold)? Pure — the damage path applies
// lowHpAttackPct exactly when this is true, so tests and panels read the
// same rule the formula does.
bool lowHpEdgeActive(const Combatant& c);

// M94: the Training Hall's sparring mirror — the party against exact echoes
// of itself. The party side builds through the one real path; each member is
// then mirrored as an enemy-side unit ("Echo <name>", partyIndex -1 so
// nothing ever writes back to the real party) BEFORE the threat table and
// battle seed derive, so a spar is as deterministic as any fight. Echoes keep
// skills, passives, gear share, and elements; the enemy AI (or the M94 manual
// mode) drives them. Pays nothing anywhere — the caller restores all party
// state afterwards regardless of outcome.
Battle buildSparBattle(const Party& party, const content::ContentDatabase& db);

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

// M61: does an authored do-nothing foe (a Quacking goose) spend `actor`'s own
// turn doing nothing? True when the unit carries `doNothingPct` and a per-turn
// roll lands under it — the same pure-hash shape as the King's scare (its own
// salt, never advances `rollCursor`, identical in the Simulator and live play).
// Feeds `chooseEnemyAction` (part of rules v12); BattleState also calls it to
// show the authored `doNothingText` over the ordinary skip line.
bool doesNothingThisTurn(const Battle& b, int actor);

// M89 (rules v16): is this one of the unit's authored every-Nth basic-attack
// turns? Counter-based, not hash-based — `ownTurnsTaken` advances in
// beginUnitTurn, so the rule is deterministic and driver-identical by
// construction. Feeds `chooseEnemyAction` (the boss swings instead of
// casting); BattleState also calls it to show the authored flavour line.
bool basicAttackTurn(const Combatant& c);

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
