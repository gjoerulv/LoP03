# M35 — Status Effects & Battle Rules v2

## A. Status and authority

- **Status:** complete (approved) — approved by the owner 2026-07-21 after manual
  testing; committed. Authored / re-audited and implemented 2026-07-21 against HEAD
  `cc1e93d` ("M34"). The foundation milestone of the M35–M42 endgame program.
  As-implemented record in §K.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. **First milestone of the
  M35–M42 endgame program.** Depends on M34 approved. M36 (passives) builds
  directly on the to-hit layer this milestone introduces.

## B. Problem statement (verified at re-audit, HEAD `cc1e93d`)

- **The status set is the M7 minimum.** `StatusType { None, Poison, AttackUp,
  AttackDown, DefenseUp, DefenseDown }` ([Enums.hpp:39](src/content/Enums.hpp:39)).
  `StatusInstance` + the shared `Battle::tickStatuses`
  ([Battle.hpp:35](src/battle/Battle.hpp:35), :105) keep sim/live in lockstep,
  and `addStatus`/`clearNegativeStatuses`/`statusSum`
  ([Battle.cpp:64](src/battle/Battle.cpp:64)) are the generic apply/cure/read
  helpers a new status plugs straight into. `attackPercent`/`defensePercent` fold
  buffs/debuffs into the damage formulas; the three new statuses do **not** touch
  those percentages.
- **No accuracy/miss/evasion layer exists anywhere.** Every `Battle::attack` and
  physical `useSkill` branch hits unconditionally. Blind introduces the game's
  first to-hit roll (Evasion, M36, reuses it).
- **The seeded stream to extend is `Battle.rngSeed`** ([Battle.hpp:90], M28),
  derived purely from the roster in `buildBattle`. **Pitfall discovered at
  re-audit:** the M28 targeting jitter hashes `Battle.turnsTaken`, but the
  `Simulator` never advances `turnsTaken` (it stays 0), while `BattleState`
  increments it per round. So `turnsTaken` is **not** a stream both drivers evolve
  identically. New per-action randomness (Blind miss, Confusion target) must ride
  a **new shared roll cursor advanced inside the shared `attack`/`useSkill`
  methods**, which both drivers call in the same order — never `turnsTaken`, never
  wall-clock, never a fresh RNG.
- **Statuses are applied through `SkillDef.statusEffect`** (single status +
  `statusMagnitude`/`statusDuration`), already loaded/validated
  ([Definitions.hpp:14], ContentLoader). New Confusion/Silence/Blind skills are
  data rows; only the enum + its parse/toString and the sim behaviour are code.
- **Stakes re-tune target is `game/StakesLadder.hpp`** — constants
  `kStakesPenaltyPerStep=15` / `kStakesPenaltyMaxSteps=6` / `kStakesPenaltyCapPct
  = perStep*maxSteps (90)`, formula `steps * perStep`. The Guild forewarning
  ([GuildState.cpp:164]) and result row already print the exact percent, so they
  follow the constants automatically.
- **Baseline:** 309/309 tests, 27/27 capture scenes, `battle::kBattleRulesVersion`
  = 1, `dungeon::kGenerationVersion` = 6.

## C. Goals

- **Slice 0 (self-contained): re-tune the stakes penalty** to −30 %/step, −99 %
  cap (steps pay 30/60/90/99), owner-decided.
- **Three new statuses in the pure sim:** Confusion (basic-attacks a seeded random
  member of its own side), Silence (cannot use MP-cost skills), Blind (physical
  attacks miss 75 %).
- **The game's first to-hit layer**, seeded off `Battle.rngSeed` via a shared
  per-action roll cursor, so Blind (and M36 Evasion) are deterministic and
  sim/live-agreed.
- **Class + item integration:** new status-applying/curing skills across the six
  learnsets; `Cure`/remedy items lift the new statuses.
- **Kit-only enemy overhaul:** every enemy/boss kit reviewed, status skills woven
  in where the role fits; base stats untouched unless the battery breaks.
- **Status UI:** battle labels, target-info panel, Details help, Miss floaters —
  all non-color-alone (M22 bar).
- `battle::kBattleRulesVersion` **1 → 2**; the scoreboard tag mechanism already
  exists. `dungeon::kGenerationVersion` unchanged at 6 (statuses do not change what
  a seed generates).

## D. Owner decisions (locked at program planning) & routine choices

Locked (not re-opened): **stakes re-tune** −30 %/step / −99 % cap (30/60/90/99);
**Blind** = physical attacks miss 75 % (100 % against an Evasion-passive holder —
Evasion arrives M36, so M35 ships the 75 % rule and leaves the Evasion hook for
M36); **Confusion** = attacks own team; **Silence** = cannot use MP-cost skills;
kit-only enemy overhaul (base stats untouched unless the battery shows breakage);
`kBattleRulesVersion` → 2.

Routine, reversible choices taken here (owner validates feel/balance at approval):

- **Roll stream.** A new `Battle.rollCursor` (starts 0) advanced by a private
  `Battle::nextRandom(salt)` that returns `mix64(rngSeed ^ mix64(++rollCursor) ^
  mix64(salt))`. It is drawn **only** when a status actually gates a roll (a
  blinded attacker, a confused attacker), so a battle with none of the new
  statuses never advances the cursor and resolves **byte-identically** to today —
  every existing battle/simulator/balance test passes unmodified. Salted per use
  (Blind vs Confusion) so the two draws are independent. Advancing per action (not
  per round) means a Blind attacker's misses vary across the fight in the
  Simulator too (where `turnsTaken` is frozen at 0), so the battery sees the true
  ~75 % miss rate.
- **Blind scope.** The 75 % miss applies to basic `attack()` and
  `SkillCategory::Physical` skills (per damaging target); Magic, Heal, Support, and
  items are unaffected (owner: "magic and items are unaffected by Blind"). A
  blinded physical skill still spends MP (the attempt is made).
- **Confusion target.** A uniform seeded pick among **all living members of the
  actor's own side, including itself** (note-time recommendation). Redirect/M28
  intercept is a no-op for own-side targets, so there is no interaction.
- **Silence enforcement.** A single predicate `!(silenced && mpCost>0)` gates the
  battle skill menu (disabled + reason), the sim party AI, and `chooseEnemyAction`
  (silenced enemies fall back to a 0-MP skill or basic attack); `Battle::useSkill`
  also **guards defensively** (a silenced MP-cost skill no-ops with a "silenced!"
  line, no MP spent), so no driver can diverge.
- **Cure.** `clearNegativeStatuses` (the `Cure` consumable + Cleric cures) also
  strips Confusion/Silence/Blind. Poison/ATK-/DEF- already cleared.
- **Miss signalling.** `Battle` records the last action's missed unit indices
  (`lastMissed`, cleared each action); `BattleState::stageNumbers` spawns a
  "Miss!" floater at each, and the returned log line states the miss.
- **Durations/stacking.** Follow the existing `StatusInstance` model (re-apply
  refreshes magnitude+turns; one instance per type). New statuses carry no
  magnitude (duration-only); a short duration (~2–3 turns) per the data rows.

## E. Proposed design & slices

0. **Stakes re-tune** (`game/StakesLadder.hpp`): `kStakesPenaltyPerStep` 15→30,
   `kStakesPenaltyMaxSteps` 6→4, `kStakesPenaltyCapPct` becomes a literal **99**
   (not perStep×maxSteps), and `stakesPenaltyPct` returns `min(cap, steps*perStep)`
   so step 4 caps 120→99. Update `tests/test_stakes_ladder.cpp` (the 15/30/45/…
   table → 30/60/90/99), `docs/game_design.md` §9, and the README How-to-play
   number. Land + build + test before the battle work.
1. **Enum + mappings:** `StatusType` gains `Confusion, Silence, Blind`;
   `parseStatusType`/`toString` in `Enums.cpp` map `"confusion"/"silence"/"blind"`.
2. **Sim behaviour** (`Battle.hpp`/`.cpp`): `rollCursor` + `nextRandom`; a helper
   set (`hasStatus`, `isBlinded`, `isSilenced`, `confusedTarget`); Confusion
   redirect + Blind miss in `attack()` and the physical branch of `useSkill()`;
   the Silence guard in `useSkill()`; `clearNegativeStatuses` extended;
   `lastMissed`. `chooseEnemyAction` (`Battle.cpp`) and `choosePartyAction`
   (`Simulator.cpp`) skip MP-cost skills when silenced. `kBattleRulesVersion` → 2.
3. **Content:** new skills in `data/skills.json` (a blind, a silence, a confuse, a
   mass-cure, plus enemy-side appliers); learnset rows in `data/classes.json`
   (e.g. Rogue blind, Mage silence, Cleric mass-cure, Guardian/ Knight remedy);
   a remedy item / extended `Cure` coverage in `data/items.json`; kit-only status
   skills woven into `data/enemies.json` + `data/bosses.json` where the role fits.
4. **UI** (`BattleState.cpp`): `statusShort`/`statusLabel` gain CNF/SIL/BLD;
   Miss floaters; the skill menu disables MP-cost skills while silenced with a
   reason; Details help text extended with the new shorthand. Non-color-alone
   (text labels already carry the info).
5. **Tests / capture / battery:** `[status]` cases (confusion redirect determinism,
   silence blocks MP skills, blind miss rate over the seeded stream, cure strips
   all three, un-statused battle byte-identical, sim/live shared-path agreement);
   a status-heavy capture scene; re-tuned `[economy-report]`/balance battery;
   presentation lint green.

## F. Determinism & save compatibility

- **`kBattleRulesVersion` 1 → 2** (rules can now resolve a battle differently when
  the new statuses are present); the scoreboard tags it (mechanism exists, M28).
  A battle with none of the new statuses is byte-identical to v1.
- **`kGenerationVersion` unchanged at 6** — statuses change how a battle resolves,
  not what a seed generates. Generation tests stay unmodified.
- **No `kSaveVersion` bump.** Statuses live only inside a live `Battle`; nothing new
  is persisted. `StatusInstance` is never serialized. New skills/items are content
  rows (unknown ids already dropped defensively on load).
- **Sim/live agreement** holds via the shared `attack`/`useSkill`/`tickStatuses`
  and the shared roll cursor advanced in the same order by both drivers.

## G. Out of scope

Passive skills (M36, incl. Evasion's 100 %-vs-Blind interaction); per-town
equipment/enemies (M37/M38); boss drops (M39); the castle (M40); any generation
change; new status *kinds* beyond Confusion/Silence/Blind; variance/crits.

## H. Balance / acceptance

Blind/Confusion/Silence read clearly and create real tactical decisions without
feeling arbitrary; the to-hit misses are legible ("Miss!" + log); the re-tuned
battery still shows the town × depth grid clearable by appropriately-levelled
parties (the new statuses must not soft-lock a fair party); stakes steps pay
30/60/90/99 to a −99 % cap; determinism holds (same seed + inputs reproduce
battles with statuses exactly); full suite green from 309; capture overflow-clean
with the status scene; presentation lint green.

## I. Manual validation for the owner

Whether the three statuses feel tactical and read at a glance; whether Blind
misses feel fair (not swingy); whether the stakes re-tune bites at the right
level; overall difficulty balance with the new kits.

## J. Required documentation updates on completion

This note (as-implemented + deviations, §K); `docs/milestones.md` (M35 status →
`implemented, awaiting manual approval`); `docs/game_design.md` (§8 combat
statuses + the re-tuned §9 stakes numbers); `docs/technical_design.md` (§10/§12
statuses, the to-hit roll stream, `kBattleRulesVersion` 2); the README
(status-set line + stakes number); `docs/manual_test_matrix.md` rows; a manual
test checklist per `docs/milestone_completion_template.md`.

## K. As-implemented record (2026-07-21)

- **Status:** complete (approved 2026-07-21). Implemented / audited against
  HEAD `cc1e93d`. The §D routine choices were taken as written; no new owner
  decision arose.
- **Slice 0 — stakes re-tune.** `game/StakesLadder.hpp`: `kStakesPenaltyPerStep`
  15→30, `kStakesPenaltyMaxSteps` 6→4, `kStakesPenaltyCapPct` is now a literal
  **99** (not perStep×maxSteps), and `stakesPenaltyPct` returns
  `min(99, steps*30)` → steps pay **30/60/90/99**. Tests, `game_design.md` §9, the
  README, and the two stakes capture scenes updated; the Guild forewarning /
  result row print the percent so they followed automatically.
- **New statuses & to-hit.** `StatusType` += `Confusion, Silence, Blind`
  (parse/toString in `Enums.cpp`). `Battle` gained `rollCursor` +
  `nextRandom(salt)` (SplitMix64 off `rngSeed`), `lastMissed`, `confusedTarget`,
  and the header-inline `hasStatus/isConfused/isSilenced/isBlinded` + free
  `canCast`. `attack()` redirects a confused actor to a seeded own-side member and
  applies the 75 % Blind miss; `useSkill()` guards Silence (MP-cost skills fizzle
  with no MP spent) and rolls Blind per physical target (a miss inflicts neither
  damage nor status). `chooseEnemyAction` and the Simulator party AI skip MP-cost
  skills under Silence via `canCast`; the enemy support-skill "already applied"
  check moved from `statusSum` (magnitude) to `hasStatus` (presence) so the M35
  magnitude-0 statuses are not re-cast every turn. `clearNegativeStatuses` (Cure)
  now strips all three. **A status-free battle draws no roll and is
  byte-identical** to the M28 rules — every pre-existing battle/simulator/balance
  test passes unmodified.
- **Class integration.** New skills in `skills.json`: `smoke_screen` (blind),
  `flash_arrow` (blind), `silence`, `bewilder` (confusion), `purify` (heal +
  cleanse). Learnsets: Rogue `smoke_screen`@7, Ranger `flash_arrow`@11, Mage
  `silence`@7 + `bewilder`@12, Cleric `purify`@10. The Cleric cure reuses a new
  `SkillEffect::Cleanse` (via the existing optional `control` field — **no loader
  or save-schema change**). The `Cure` item (renamed **Remedy**, id unchanged)
  now lifts every affliction.
- **Enemy kit overhaul (kit-only).** Status skills woven into mid/late kits where
  the role fits: `rune_sentry` + `smoke_screen`, `shadow_stalker` + `smoke_screen`,
  `void_weaver` + `silence`; bosses `crystal_sorcerer` + `silence`,
  `blight_matron`/`hollow_commander` + `bewilder`, `deep_king` + `smoke_screen`.
  The two tutorial-tier bosses (`keep_warden`, `rush_tyrant`) and the weakest early
  enemies were **intentionally left pure** to preserve early-game clearability.
  No base stats changed.
- **UI.** `statusShort` += CNF/SIL/BLD; a "Miss!" floater per `lastMissed` unit;
  the skill menu disables MP-cost skills under Silence and appends `[Silenced]`
  (non-colour-alone); the battle Details help explains CNF/SIL/BLD. Capture's
  `applyCaptureStatuses` now spreads all statuses (incl. a 3-status stack) so the
  five-enemy and targeting scenes show the new labels.
- **Version.** `battle::kBattleRulesVersion` 1 → 2 (scoreboard tags it, mechanism
  from M28). `dungeon::kGenerationVersion` **unchanged at 6**; `kSaveVersion`
  unchanged.
- **Validation (this session, VS 2022 / MSVC 14.44, `build-msvc`).** Debug build
  clean; full suite **321/321** (309 baseline +12 new `[status]` cases in
  `test_status_v2.cpp`; the only pre-existing tests edited were the skill-count
  guard 43→48 and the poison-expiry test for the doubled potency). `--capture`
  **27/27** overflow-clean (the new statuses appear in scenes 17/23; the scoreboard
  theme column was widened 6px so a pre-M35 entry's older-rules `~` marker fits the
  widest `T7 Hollow Forest  * ~` row — the version bump surfaced this). Release
  build + Release suite **321/321** clean. `[economy-report]` clearability grid
  (worst of 3 seeds, depths 1/2/3/4/5/6/8/10): clearing levels **1/4/3/2/4/8/8/9**,
  all depths clearable — the economy guards pass; exact per-depth levels are
  seed-variance sensitive and flagged for the owner's balance read.

### Post-implementation owner refinement (2026-07-21)

The owner asked for three status tweaks after the first implementation; all fold
into M35 (still awaiting approval, so `kBattleRulesVersion` stays 2 — v2 is
defined by whatever M35 ships):

1. **Confusion is cleared when its bearer takes damage.** Done in the shared
   `applyDamage` chokepoint (all attack/skill damage), so live play and the
   Simulator agree; the poison DoT (which bypasses `applyDamage`) deliberately
   does **not** clear it. A hit on a confused ally/foe now snaps it out.
2. **Statuses last twice as long.** `kStatusDurationMult = 2` applied in
   `addStatus`, so every applied status (buff, debuff, affliction) doubles its
   authored duration uniformly. The authored `statusDuration` values in the data
   are unchanged — the multiplier is the single reversible knob.
3. **Poison deals double damage.** `kPoisonDamageMult = 2` applied at the
   `tickStatuses` poison site (the log shows the actual doubled amount). Combined
   with the doubled duration, poison's total lifetime damage is ~4× the authored
   values — as the owner specified the two changes independently.

Both multipliers are named constants in `Battle.hpp`. Two `[status]` tests were
added (confusion-cured-by-damage, doubled-duration/poison), the poison-expiry
test was updated for the new potency, and the balance grid re-run (depth 6 moved
7→8; still fully clearable).

### Deviations from the plan / note

1. **Cleric cure via `SkillEffect::Cleanse`**, not a new SkillDef field — reuses
   the existing `control` field, avoiding any loader/schema/save change while
   still delivering the "mass-cure" skill. (Enum-only addition.)
2. **The `Cure` item was renamed Antidote → Remedy** (id `antidote` unchanged) to
   fit its broadened effect (it now lifts blind/silence/confusion too).
3. **The scoreboard theme column was widened 6 px** (`kThemeX` 302→296, right pad
   16→14) and its legend generalised from "pre-M28" to "older battle rules" — a
   required consequence of the `kBattleRulesVersion` bump making pre-M35 entries
   show the `~` marker, which pushed the widest row past the old budget.
4. **Enemy overhaul is deliberately partial** — appliers only (no enemy
   cleansers, to avoid new enemy-AI paths), and confined to mid/late/boss kits.

### Known items for owner validation

- Balance feel of the enemy status kits (esp. depth-2 clearing level 4 in the
  battery); whether Blind's 75 % miss feels fair; whether the stakes re-tune bites
  at the right pace.
- Visual/readability of the CNF/SIL/BLD labels and the "Miss!" floater in live
  play (captures show the static labels only).
