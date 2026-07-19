# M19 — Progression, Economy, and Score-Integrity Hardening

## A. Status and authority

- **Status:** complete (approved) — approved by the owner on 2026-07-19
  after manual testing (see §N).
- **Last reviewed repository commit:** `97a1871` (M18, 2026-07-19). Re-audit:
  score entries already carry depth/theme/seed/`generationVersion` (M16) but
  no power context; scoring coefficients live in `Scoring.cpp`; battle gold
  does not feed the treasure bonus and no per-turn income exists, so
  farming/stalling cannot inflate a single run's score by construction;
  Training Hall = 40+30·level gold per level vs battle XP 30+20·(level−1)
  to next; Inn healing is free; equipment is class-unrestricted.
- **Owner decisions (2026-07-19):** (1) **comparability policy approved** —
  optional `partyLevel` tag on score entries (highest level at completion;
  absent = legacy "-"), an Lv column, and a visible conditions legend;
  ranking math unchanged, no hidden normalization; (2) **economy rules kept**
  (free Inn, unrestricted equipment) — data-value tuning only, each change
  evidence-backed.
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M19 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Planning fidelity:** bounded specification. Concrete tuning targets depend
  on the game as it exists after M12–M18; slices below are provisional and
  must be refined — and this note refreshed against the then-current
  repository — before implementation begins.

## B. Problem statement

M10 added XP/leveling (battle XP + paid Training Hall levels), starting gold,
and shops on top of the M7 equipment economy — all lightly tuned (README:
"the economy is lightly tuned"; equipment has no per-class restrictions).
Because score is primarily *fewest battle turns*, permanent power progression
can silently make runs incomparable: a level-30 party's 12-turn clear is not
the same achievement as a level-1 party's. The scoreboard currently records
runs without any run-condition context. There has been no exploit/dominant-
path analysis of the gold↔level↔equipment loops since leveling landed.

This is an audit-and-tuning milestone, not a reimplementation of XP.

## C. Goals

- Evidence: simulations (via the existing pure `battle/Simulator`) plus
  owner-run sessions covering XP-curve and level-cap pacing; battle-XP vs
  Training Hall roles; gold inflow/sinks by depth; healing and failure
  consequences; consumable/equipment purchase dominance; class power growth
  and equipment scaling; farming incentives before scoring runs.
- An **explicit score-comparability policy**: scoreboards segmented or tagged
  by meaningful run conditions (candidates: seed, depth, theme, generation
  version, party level/power band) rather than an opaque normalization
  formula.
- Data-driven tuning changes only (values in `data/*.json` and named
  constants), each backed by simulation or play evidence.
- Confirmed class identity: every class retains a meaningful tactical role
  after tuning.

## D. Non-goals

- No new content or mechanics (M20).
- No XP-system reimplementation.
- No hidden power-normalization formula the UI cannot explain.
- No scoring changes that reward farming, stalling, or ignoring the
  fewest-turns premise.

## E. Dependencies

- M12–M18 substantially complete (presentation-stable game so evidence
  reflects reality).
- M16's generation-version decision (a run-condition candidate).
- **Owner approval required for:** any scoreboard/save schema addition
  (run-condition tags), any player-visible economy rule change, and the final
  comparability policy.

## F. Proposed slices (provisional — refine before starting)

1. **Evidence pass** — simulation battery + analysis report (curves, dominant
   paths, exploits); owner-run confirmation sessions.
2. **Comparability policy** — proposal with options and a recommendation;
   owner decision; schema addition (if any) with backward-compatible loading;
   scoreboard UI shows the conditions.
3. **Tuning pass** — data-driven adjustments with before/after simulation
   evidence; class/equipment review fixes.
4. **Regression net** — tests encoding the tuned expectations (e.g. depth-1
   clearable by starting party; no infinite gold loop found by the sim
   battery).

## G. Expected systems affected

- `data/*.json` — tuning values.
- `src/score/Scoreboard`/`src/save/` — only if tagging fields are approved.
- `src/states/ScoreboardState` — condition display.
- `tests/` — expanded balance/economy suites.
- Simulation tooling growth in `battle/Simulator` usage (pure).

## H. Data, schema, and save compatibility

- **Potential scoreboard/save schema addition** (run-condition tags):
  owner-gated; old entries must still load and be displayed honestly (e.g. as
  "untagged/legacy").
- Content schema value changes are tuning, not shape changes; any *shape*
  change is owner-gated.
- Deterministic seeds: no impact. Settings: no impact.

## I. Automated validation

- Simulation battery producing machine-readable results (feeds M23).
- Economy/XP-curve tests; scoreboard backward-compatibility tests.
- Existing balance sanity tests updated to the tuned values, all green.

## J. Manual validation for the owner

- Play the identified exploit paths (gold farming, Training Hall rushing,
  shop dominance) and confirm they are closed or acceptably bounded.
- Review the comparability policy in the scoreboard UI: are the conditions
  understandable and honest?
- Confirm class identity across a few runs with different party mixes.

## K. Acceptance criteria

- No obvious infinite or dominant gold/XP loop (simulated and play-checked).
- Training Hall and battle XP have distinct, defensible roles.
- Every class retains a meaningful tactical identity.
- Score comparison conditions are visible and honest in the UI.
- Progression and economy values remain data-driven.
- Old saves and scoreboard files still load.

## L. Risks and failure modes

- **Comparability half-measure** — tagging that players cannot understand is
  as bad as none; the UI explanation is part of the deliverable.
- **Over-tuning** — chasing simulator artifacts instead of play reality;
  owner sessions are the check.
- **Schema regret** — tag fields are permanent; owner-gated design first.
- **Second-order:** tuning shifts difficulty pacing (M22 onboarding) and
  M23's balance baselines — record final values clearly.
- **Score-incentive damage** — any change must be re-checked against the
  "never reward farming/stalling" design guard.

## M. Required documentation updates on completion

- `docs/milestones.md`; this note (as-implemented + deviations).
- `docs/game_design.md` — comparability policy and any economy rule changes.
- `docs/technical_design.md` — scoreboard schema if changed.
- Save/schema docs if fields added.
- Completion report per `docs/milestone_completion_template.md`.

## N. As-implemented record (2026-07-19)

**Audit findings** (battery: `tests/test_economy.cpp`; table via
`crystal_tests "[economy-report]" -s`, worst of 3 seeds per depth):

| depth | clearing lv | rounds | xp/member | gold/clear | train-4 @clearing lv |
|---|---|---|---|---|---|
| 1 | 1 | 20 | 486 | 478 | 280 |
| 2 | 2 | 32 | 707 | 625 | 400 |
| 3 | 2 | 46 | 928 | 796 | 400 |
| 4 | 3 | 43 | 950 | 915 | 520 |
| 5 | 3 | 55 | 1126 | 969 | 520 |
| 6 | 5 | 48 | 1332 | 1162 | 760 |
| 8 | 3 | 65 | 1257 | 967 | 520 |
| 10 | 3 | 65 | 1257 | 1029 | 520 |

- **No exploit loops.** Battle gold never enters the treasure bonus, no
  per-turn income exists, stalling and escapes strictly cost score, and an
  unfinished run scores 0 — all now guarded by pure tests.
- **Roles hold.** A depth-1 clear (~478g) cannot fund training the party
  even one level at mid-game prices (~760g); battle XP levels a fresh party
  out of L1 in one clear. Training = instant targeted gold sink; battles =
  broad free income. Class stat identity verified at levels 1/10/25/50.
- **One structural finding:** the difficulty curve **plateaus past depth
  ~6** (team size caps at depth 3, elite chance at ~depth 4), so depth 8–10
  clears at the same level as depth 4–5. Fixing it changes what published
  seeds generate → out of M19 scope; **deferred to M20** (externalized
  composition constraints), recorded there.
- **No data-value tuning applied** — the evidence showed nothing broken at
  the depths the generator actually scales across. The battery is the
  regression net (playable-and-rising curve, sink/income relation, class
  identity, score-incentive guards).

**Comparability policy delivered** (owner-approved): optional `partyLevel`
on `ScoreEntry` (highest member at completion; absent = legacy), written/
loaded without a format bump; recorded at dungeon completion; scoreboard
gains an Lv column ("-" for legacy) and a two-line legend naming the honest
comparison conditions. Ranking math untouched. Backward-compat round-trip
and absent-default tests added. Economy rules kept as-is per the owner
(free Inn, unrestricted equipment) — reviewed, not changed.

**Evidence:** 216/216 tests (8 new); scoreboard capture with mixed tagged/
legacy entries in `docs/screenshots/m19_score/` (seeded board, owner's real
scoreboard backed up and restored).

**Deviations:** none beyond the explicitly deferred depth-plateau fix; no
owner-run confirmation sessions occurred yet (the manual checklist covers
them).
