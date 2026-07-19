# M19 — Progression, Economy, and Score-Integrity Hardening

## A. Status and authority

- **Status:** planned
- **Last reviewed repository commit:**
  `a316f244e870718aa27d9995dc871e11572ad429` (2026-07-19).
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
