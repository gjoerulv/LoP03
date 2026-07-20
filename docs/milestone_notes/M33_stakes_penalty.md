# M33 — Stakes-Escalation Penalty

## A. Status and authority

- **Status:** in progress — authored / re-audited 2026-07-21 against HEAD
  `25f5e1e` ("M32"). Owner authorized beginning M33 on 2026-07-21 after approving
  M32. As-implemented record in §K.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. Third milestone of the
  **M31–M34 expansion program**; depends on M32 (the `(town, depth)` stakes
  vector). M34 (black market) will reuse this milestone's stakes-raise event as
  its spawn trigger.
- **Scope guard:** M34 is direction only. This note implements the stakes
  penalty and nothing downstream.

## B. Problem statement (verified at re-audit, HEAD `25f5e1e`)

- **Nothing discourages farming a safe stake.** A run's difficulty/reward is
  `(town, depth)` (M32), but repeating the same easy `(town, depth)` is as
  score-rewarding as pushing further. The town score bonus rewards *being* high,
  not *climbing*.
- **The seams this milestone plugs into already exist.** Scoring is pure —
  `scoreBreakdown(RunSummary)` now yields `base…wager` + `townBonus`
  ([Scoring.cpp](src/score/Scoring.cpp)); `ScoreEntry` carries optional
  comparability tags (`generationVersion`, `partyLevel`, `battleRulesVersion`,
  `townIndex`) with no format bump. The completed-run path
  (`DungeonState::completeDungeon`, [DungeonState.cpp:541](src/states/DungeonState.cpp:541))
  builds the `RunSummary`, computes the score, writes the `ScoreEntry`, and (M32)
  already reads `dungeon_.town`/`dungeon_.depth` and updates persistent party
  state. The Guild ([GuildState.cpp](src/states/GuildState.cpp)) configures the
  run with a live `depth_` picker at the fixed `party.currentTown`, and
  **autosaves on entry** (`enterDungeon`) — the anchor for save-scum resistance.
- **Optional party save state is the established pattern** — `currentTown`,
  `highestUnlockedTown`, `restTokens` are all optional, defaulted, no
  `kSaveVersion` bump.

## C. Goals

- A **score penalty that grows when a run fails to raise the stakes** and clears
  when it does, per the owner rule (§D), pushing the player to climb town/depth
  rather than farm.
- **Forewarned, visible, and honest (M19):** the Guild shows the penalty the
  configured run will incur, live as depth changes; the result screen and the
  score record show the penalty applied. Never silently normalized.
- **Save-scum-proof:** the penalty state persists and travels with the
  autosave-on-entry, so reloading cannot shed an earned penalty.
- No battle-rule change (`battleRulesVersion` stays 1), no generation change
  (`generationVersion` stays 6), no save-format bump; town 1 / legacy runs score
  exactly as before.

## D. Owner rule (locked at program planning) and derived math

### D.1 — The stakes state machine (owner-decided; not re-opened)

A run's **stakes = (townIndex, depth)**, compared **lexicographically, town
first**, against the **previous completed run**. On each **completed** run:

- stakes **strictly higher** than the previous completed run → penalty **resets
  to 0**;
- **not** strictly higher → penalty **grows one step** (**−15 % per step**,
  capped at **−90 %** = 6 steps);
- **score-0 runs** (retreats/wipes — which never reach the completed-run path)
  neither grow the penalty nor move the baseline.

State: `{ prevTown, prevDepth, penaltySteps }` (all 0 initially, so the first
real run always "raises" and is unpenalized). The penalty a run at `(town,
depth)` incurs = raised ? 0 : `min(90, (penaltySteps+1) × 15)`; a completed run
sets `penaltySteps = raised ? 0 : min(6, penaltySteps+1)` and moves
`(prevTown, prevDepth)` to its own stakes.

### D.2 — Score combination → *penalty and town bonus are both percentages of the run subtotal; bonus added, penalty subtracted (as the plan specifies: "after the town bonus, on the same subtotal rules")*

```
subtotal      = base + boss − turns + chests + danger + treasure + noDeath − escapes + wager
townBonus     = max(0, subtotal) × townBonusPct / 100      (M32, ≥ 0)
stakesPenalty = max(0, subtotal) × stakesPenaltyPct / 100  (M33, ≥ 0, subtracted)
total         = max(0, subtotal + townBonus − stakesPenalty)
```

Both modifiers key off the same non-negative subtotal, so at town 1 (no bonus) a
−90 % penalty leaves 10 % of the subtotal — a real bite — while at a high town
the town bonus cushions it (e.g. town 7 +100 % with −90 % → 110 % of subtotal),
which matches the owner's "push escalation **without feeling punitive**" bar.
*Alternative considered and rejected:* compounding the penalty on the
town-boosted running total (town 7 −90 % → 20 % of subtotal) reads as punitive
and is not what "same subtotal rules" says. This math is a data-like constant of
the design and reversible; the owner validates the feel and can veto at approval.

### D.3 — Routine decisions taken locally

- **Storage:** a `StakesState stakes` on `Party` (rules in a pure
  `game/StakesLadder.hpp`, mirroring `WorldLadder.hpp`); three optional save
  fields (`stakesPrevTown/stakesPrevDepth/stakesPenaltySteps`), old saves → 0/0/0.
- **Baseline-move guard:** the state update in `completeDungeon` is guarded on
  `total > 0`, so the theoretical completed-but-zero run (extreme turn penalty)
  is treated like a score-0 run and does not move the baseline — the literal
  owner rule.
- **Scoreboard display:** the penalty appears on the **result screen** (a
  `stakesPenalty` breakdown row) and is stored as an optional
  `ScoreEntry.stakesPenaltyPct` tag; the **scoreboard row is left unchanged**
  because the town/depth conditions that make runs comparable are already shown
  and the penalty is already folded into the visible Score — adding a fourth tag
  to the 108px Theme column would reintroduce the overflow M32 just fixed. The
  score Details text notes the penalty. (Flagged for owner veto.)

## E. Proposed design & slices

1. **Pure state machine** — `game/StakesLadder.hpp`: `StakesState`,
   `stakesRaised(state, town, depth)`, `stakesPenaltyPct(state, town, depth)`,
   `afterCompletedRun(state, town, depth)`, constants
   (`kStakesPenaltyPerStep=15`, `kStakesPenaltyMaxSteps=6`,
   `kStakesPenaltyCapPct=90`). Headless-tested.
2. **Save** — `Party.stakes`; `SaveSystem` writes/reads the three sub-fields
   (optional, clamped ≥ 0; steps clamped to `[0, kStakesPenaltyMaxSteps]`).
3. **Scoring** — `RunSummary.stakesPenaltyPct`, `ScoreBreakdown.stakesPenalty`
   (D.2), pure + tested; `ScoreEntry.stakesPenaltyPct` optional tag +
   `Scoreboard` round-trip.
4. **Completed-run wiring** — `completeDungeon` computes the penalty from the
   pre-run `party.stakes`, folds it into the score and the `ScoreEntry`, then
   (if `total > 0`) advances `party.stakes = afterCompletedRun(...)`. Order vs
   the M32 unlock is independent.
5. **Forewarning UI** — the Guild shows, live under the menu, "Raises the stakes —
   no penalty" or "Stakes penalty: −N%  (raise town or depth to clear)", computed
   from `party.stakes`, `party.currentTown`, and the live `depth_`. First time a
   penalty is shown, the `kFirstPenalty` M22 beat fires (from `handleInput`,
   never `render`). The result screen gains a "Stakes penalty (−N%)" row.
6. **Tests & capture** — state-machine table tests (raise / repeat / cap / reset /
   score-0), save round-trip, scoring rows, a Guild-with-penalty capture scene,
   and the result scene extended with a penalty row.

## F. Determinism & save compatibility

- **`battleRulesVersion` stays 1; `generationVersion` stays 6.** Scoring is town/
  history math only; battle and generation are untouched (their tests stay
  unmodified).
- **No `kSaveVersion` bump.** The three stakes fields are optional (old saves →
  0/0/0 ⇒ first run raises, no penalty). Scoreboard `stakesPenaltyPct` optional.
- **Save-scum resistance.** The penalty is a pure function of the persisted
  pre-run state; the autosave-on-entry captures that state, so reloading restores
  the same state and the same run yields the same penalty.
- **Comparability (M19).** The penalty is tagged and shown, never used for
  ranking; ranking logic is untouched.

## G. Out of scope

The black market / legendary tokens (M34); any change to the M32 town bonus, the
stakes *values* (−15 %/−90 %), or the comparison rule; any battle/generation/
save-format change; a scoreboard layout reflow.

## H. Balance / acceptance

The penalty must push escalation without feeling punitive: repeating a stake
visibly erodes the score (−15 % steps), raising town or depth immediately clears
it, and the Guild forewarns clearly enough that a player is never surprised by a
penalty after the fact. State-machine table tests pin every transition.

## I. Manual validation for the owner

Whether the penalty pushes escalation without feeling punitive; Guild
forewarning clarity (does "raise town or depth to clear" read right, and update
live with depth?); that reloading an autosave cannot shed a penalty; that the
result screen's penalty row is legible.

## J. Required documentation updates on completion

This note (as-implemented + deviations); `docs/milestones.md` (status);
`docs/game_design.md` (§9 scoring — the stakes penalty rule); `docs/technical_design.md`
(`StakesLadder`, `Party.stakes` save fields, `stakesPenalty` scoring,
`ScoreEntry.stakesPenaltyPct`, Guild forewarning); `docs/manual_test_matrix.md`
rows (Guild forewarning, result penalty row, save-scum check).

## K. As-implemented record (2026-07-21)

- **Status:** implemented, awaiting manual approval. No new owner decisions arose
  (the stakes rule was locked at planning; the score-combination math follows the
  plan's "after the town bonus, on the same subtotal rules" — §D.2).
- **State machine (pure).** `game/StakesLadder.hpp`: `StakesState { prevTown,
  prevDepth, penaltySteps }`, `stakesRaised` (town-first lexicographic),
  `stakesPenaltyPct` (raised → 0, else `min(90, (steps+1)*15)`),
  `afterCompletedRun` (reset on raise / +1 step capped otherwise, baseline moves),
  `clampStakesSteps`, constants (15 / 6 / 90). Headless-tested.
- **Save.** `Party.stakes`; `SaveSystem` writes `stakesPrevTown/stakesPrevDepth/
  stakesPenaltySteps` and reads them optionally (non-negative; steps clamped),
  old saves → fresh zero state. No `kSaveVersion` bump. New Game (`begin()`) now
  also resets `stakes` (clean slate, alongside the M32 town/M30 token reset).
- **Scoring.** `RunSummary.stakesPenaltyPct` → `ScoreBreakdown.stakesPenalty =
  max(0,subtotal)*pct/100`; `total = max(0, subtotal + townBonus − stakesPenalty)`
  (pure, tested; pct 0 ⇒ unchanged). `ScoreEntry.stakesPenaltyPct` optional tag,
  round-tripped by `Scoreboard`.
- **Completed-run wiring.** `completeDungeon` computes the penalty from the
  **pre-run** `party.stakes` (so it equals the Guild forewarning), folds it into
  the score and the `ScoreEntry`, then advances `party.stakes` **only when
  `total > 0`** (a completed-but-zero run doesn't move the baseline). The
  autosave-on-entry captures the pre-run state, so reloading can't shed a penalty.
- **Forewarning UI.** The Guild shows, live under the menu, "No stakes penalty -
  this run raises the stakes." or "Stakes penalty: −N% (raise town or depth to
  clear)", from `party.stakes` + `currentTown` + the live `depth_`. First time a
  penalty is applicable, the `kFirstPenalty` M22 beat fires from `handleInput`.
  The result screen gains a "Stakes penalty (−N%)" row.
- **Validation (this session, VS 2022 / MSVC 14.44, `build-msvc`).** Debug build
  clean; full suite **299/299** (31009 assertions), incl. 6 new `[stakes]` cases
  and the extended save round-trip; battle & simulator tests **unmodified and
  green**. `--capture` **26/26** overflow-clean (new `26_guild_penalty` scene;
  result now carries both the town-bonus and stakes-penalty rows). Release build
  clean. Determinism/generation unchanged.

### Deviations from the plan / note

1. **The scoreboard row is unchanged; the penalty shows on the result screen and
   is stored as `ScoreEntry.stakesPenaltyPct`** (§D.3). The 108px Theme column
   (widened in M32 to just fit `T7 Hollow Forest  *`) has no room for a fourth
   tag without reintroducing overflow, and the penalty is already folded into the
   visible Score while the town/depth conditions that make runs comparable are
   shown. The score Details text mentions the penalty. Flagged for owner veto —
   a per-row tag would need a column reflow.
2. **`stakes` lives on `Party`** (like the M32 town fields), not a separate
   `WorldState` — least churn, same save owner.
