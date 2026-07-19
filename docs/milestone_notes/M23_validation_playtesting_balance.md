# M23 — Automated Visual Validation, Playtesting, and Balance Hardening

## A. Status and authority

- **Status:** planned
- **Last reviewed repository commit:**
  `a316f244e870718aa27d9995dc871e11572ad429` (2026-07-19).
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M23 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Boundary note:** this milestone combines the former "automated validation"
  scope with the external-playtesting and balance-hardening work previously
  grouped under the release milestone, so that playtest-driven fixes land
  *before* final packaging. M24 is pure release packaging and final
  validation.
- **Planning fidelity:** bounded specification; refresh this note against the
  then-current repository before implementation begins.

## B. Problem statement

By this point the game will have measured layout, replaceable assets, compact
rooms, and full presentation — but nothing prevents regressions: there is no
reproducible way to reach representative presentation states, no automated
overflow/asset checks, and no external evidence that real players can play,
read, navigate, and understand scoring. Balance evidence so far is
simulation-plus-owner; uncoached players have never been observed. Shipping
without both a regression net and playtest-driven hardening would make M24
sign-off untrustworthy.

## C. Goals

**Validation tooling:**

- Deterministic debug/capture scenarios that reach representative states:
  title/help/settings; party creation with maximum names; full and empty save
  slots; each town service with longest descriptions; guild with maximum
  seed/depth strings; each dungeon theme and room archetype; battle with
  maximum statuses, longest skill descriptions, and five enemies;
  victory/defeat/result/scoreboard extremes.
- Native-resolution (426×240) screenshot capture for those scenarios.
- UI bounds/overlap/overflow diagnostics that **fail validation** on
  unintended overflow.
- Presentation-content linting: missing asset IDs, undefined action tokens,
  invalid animations, text-overflow-policy violations.
- Large-scale room generation/connectivity validation (thousands of rooms).
- Expanded deterministic battle simulations with machine-readable reports
  identifying outlier encounters and progression bands.

**Playtesting and balance:**

- Observed, uncoached playtests with at least: a player unfamiliar with the
  project; an experienced JRPG player; a roguelite/score-chasing player; a
  controller-first player; a keyboard-first player; a smaller-display or
  low-vision setup where available.
- Recorded observations: time to begin a run; wrong-button/navigation errors;
  missed interactables; unread/clipped text; room-navigation hesitation;
  battle decision clarity; score comprehension; run duration; dominant
  strategies; voluntary replay.
- High-value fixes (control/readability/navigation/score-comprehension) and
  final evidence-backed balance/pacing changes.

## D. Non-goals

- No release packaging (M24).
- No new features or content.
- No fragile pixel-perfect image comparison as the primary validation —
  geometry assertions plus reviewed screenshots first; image diffs, if added,
  need explicit tolerance and update policy.
- No balance changes without playtest or simulation evidence.

## E. Dependencies

- M12–M22 substantially complete (captures and playtests must reflect the
  real game).
- External testers recruited by the owner.
- **Owner approval required for:** the capture-tooling approach if it needs a
  new build flag or debug surface; any balance change that alters the player
  experience beyond tuning values.

## F. Proposed slices (provisional — refine before starting)

1. **Scenario/capture tooling** — deterministic state setup + native-res
   screenshot capture, excluded or disabled in Release builds.
2. **Diagnostics and linting** — bounds/overlap/overflow checks wired to the
   M12 layout system; presentation-content lint; room-scale validation;
   simulation report expansion.
3. **Playtest round(s)** — owner-run observed sessions with the tester
   profiles above; findings triaged into the defect register.
4. **Hardening pass** — fix repeated usability defects; evidence-backed
   balance/pacing changes; re-run captures/validation to confirm.

## G. Expected systems affected

- New debug/validation tooling (capture scenarios, diagnostics) behind
  development-only flags.
- `tests/` — large-scale generation/lint/simulation suites.
- Targeted fixes across `src/states/`, `src/ui/`, and `data/` tuning values
  as playtests dictate.
- `docs/manual_test_matrix.md` and the defect register — updated with
  playtest evidence.

## H. Data, schema, and save compatibility

- Expected **no impact** on saves, settings, content schemas, seeds, or
  scoring — balance changes are value tuning under the M19 comparability
  policy. Any schema-shape need discovered here is owner-gated.
- Debug/capture tooling must be excluded or safely disabled in Release.

## I. Automated validation

- The new validation suites themselves: overflow/overlap failures fail the
  run; missing required assets fail clearly; thousands of generated rooms
  pass invariants; simulation reports generate reproducibly.
- Representative captures reproduce deterministically.
- Full test suite green in Debug and Release.

## J. Manual validation for the owner

- Run/observe the external playtests; confirm findings were triaged and the
  repeated defects fixed rather than documented away.
- Review the capture set for visual regressions.
- Review balance reports and the final tuning changes.
- Confirm no debug/capture tooling is reachable in a Release build.

## K. Acceptance criteria

- Unintended UI overflow and missing required assets are failing validation
  results.
- Representative captures are reproducible.
- Thousands of generated rooms pass invariants.
- All listed tester profiles were observed; repeated usability and balance
  defects are fixed.
- No dominant strategy trivializes representative runs (simulation + playtest
  evidence).
- Balance reports identify outlier encounters and progression bands.
- Capture/debug tooling is excluded or safely disabled in Release builds.

## L. Risks and failure modes

- **Tooling rabbit hole** — building elaborate visual-diff infrastructure
  instead of shipping-grade checks; geometry-first policy is the control.
- **Playtest logistics** — recruiting testers is owner-dependent and can
  stall the milestone; schedule early.
- **Coaching bias** — helped testers invalidate onboarding evidence; observe
  uncoached.
- **Late-fix regression** — balance/usability fixes at this stage can break
  earlier acceptance; re-run captures and suites after each fix batch.
- **Scope breadth** — this milestone now spans tooling + playtesting +
  balance; slices must land independently, and the note refresh should
  re-check the split still fits in one milestone.

## M. Required documentation updates on completion

- `docs/milestones.md`; this note (as-implemented + deviations, playtest
  summaries).
- `docs/presentation_audit.md` — closed/remaining defects.
- `docs/manual_test_matrix.md` — final pre-release state.
- `docs/technical_design.md` — validation tooling architecture.
- `docs/game_design.md` — any tuned player-facing values that it documents.
- Completion report per `docs/milestone_completion_template.md`.
