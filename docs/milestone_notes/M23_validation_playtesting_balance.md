# M23 — Automated Visual Validation, Playtesting, and Balance Hardening

## A. Status and authority

- **Status:** in progress
- **Last reviewed repository commit:** M22 approval HEAD (2026-07-20).
  Re-audit: no capture/scenario tooling exists (window-screenshot
  automation only, from milestone verification); UiDraw's fitted/wrapped
  helpers log overflow per site but nothing counts or fails on it; mass
  room validation exists at moderate scale in test_room_layout; the M19
  economy battery is table-print only ([economy-report]); baseline battery
  (2026-07-20): depths 1/2/3/4 all clear at L1 (the flag M20 deferred
  here), 5/6/8/10 need 3/5/9/11. `main()` takes no arguments; the debug
  overlay's CMake-option pattern (CRYSTAL_ENABLE_DEBUG_OVERLAY →
  compile definition) is the template for a capture flag.
- **Owner decisions (2026-07-20):**
  1. **Capture tooling approved:** CRYSTAL_ENABLE_CAPTURE CMake option +
     `--capture <outdir>` CLI in dev builds only; deterministic scenario
     states rendered to the real 426×240 virtual screen, native-res PNGs,
     zero-overflow assertion per scene; excluded from Release (verified by
     building a Release configuration).
  2. **Early-game tuning + version bump approved:** composition-value
     tuning (starting elitePctPerDepth 9→12, final values from battery
     evidence; depth 1 stays L1-clearable, depth 8+ effectively
     unchanged); `kGenerationVersion` 3→4 per the comparability policy.
  3. **Flow confirmed:** tooling + diagnostics + sim-justified tuning +
     playtest kit now, then STOP with the milestone `in progress` while
     the owner runs the observed playtests; hardening lands on findings;
     only then `implemented, awaiting manual approval`.
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

## N. Interim as-implemented record (2026-07-20 — awaiting owner playtests)

Slices 1, 2, and the sim-justified half of 4 are done; slice 3 (observed
external playtests) is the owner's, per the confirmed flow, and the
findings-driven hardening completes the milestone afterward.

**Balance tuning (owner decision 2).** `data/composition.json`:
`elitePctPerDepth` 9→12, `deepMinDepth` 4→3; `kGenerationVersion` 3→4.
Battery (worst of 3 seeds, depths 1/2/3/4/5/6/8/10): clearing levels
1/1/1/1/3/5/9/11 → **1/1/1/2/3/7/9/11**. Depth 1 stays fresh-party
clearable (regression-asserted); depth 4 now demands growth; depth 8+
unchanged (elite cap). Depth 3 refuses to move at integer-level
granularity (its effort still rises: 27→37 rounds vs depth 1), and depth
6 stepped 5→7 because the elite cap now lands there — both flagged for
the playtest read on pacing. An elitePct=11 probe regressed depth 4 and
was rejected by the battery's own net.

**Capture tooling (owner decision 1).** `CRYSTAL_ENABLE_CAPTURE` CMake
option; the definition is additionally suppressed for Release
configurations unconditionally. `--capture <outdir>` renders **22
deterministic scenarios** (all screen families, 3 themes at depths
6/8/10, five-enemy battle with live statuses + 12-char worst-case names,
boss telegraph, maximal result breakdown, scoreboard extremes incl. a
legacy row and max seed, tutorial + Details overlays, High Contrast town)
to the real virtual screen in a hidden window and exports native 426×240
PNGs (`docs/screenshots/m23_captures/`, local-only). Every scene asserts
a zero delta on the new `ui::overflowEvents()` counter; export failure or
overflow → nonzero exit. Scratch temp dirs for all mutable state — player
data untouched. Reruns hash-identical. Verified excluded from the Release
binary (string probe) with the Release suite green (247/247).

**Diagnostics & reports.** `tests/test_presentation_lint.cpp`:
convention-derived asset ids (per-theme tiles, per-class battle actors,
tier fallbacks, markers, props, UI) must resolve in the shipped manifest;
name/description/telegraph budgets under a conservative measure; **2000+
generated rooms** across seeds×depths×themes pass the full M16 validator.
First run caught two over-budget boss telegraphs — all four were
tightened in `data/bosses.json` (mechanic + counterplay preserved).
`[sim-report]` emits reproducible JSON: progression bands + outlier
encounters (> 2× median rounds at clearing level); current outliers are
protector-heavy Crystal Mine teams (3–4× median) — input for the
playtest-informed hardening pass.

**Playtest kit.** `docs/playtest_protocol.md`: six tester profiles,
uncoached session rules, the observation sheet, debrief questions, and
the triage contract (repeated defects are fixed, deferrals need written
reasons).

**Interim status:** `in progress`. Next: the owner runs the playtests and
returns filled observation sheets; triage + hardening + capture re-run
complete the milestone.

**Owner decision (2026-07-20, after the interim report):** M24's
packaging engineering proceeds **in parallel** while this milestone stays
open for the playtests; playtest findings land here as hardening (or as
release-blocking fixes under M24's scope). M24's final matrix sign-off
and release-candidate approval wait until M23 completes.

## Post-audit release-hardening correction

A static audit of commit `356619d64d4511c7f047bef1a4ca82d1df561595` found that the original `CMAKE_BUILD_TYPE` capture gate was unsafe for Visual Studio multi-configuration Release builds. The repair uses `$<CONFIG:Release>` generator expressions, excludes both capture and overlay code from every Release configuration, adds atomic persistence and transactional scoreboard loading, and adds persistent packaged-build diagnostics. Automated and owner-run validation remain required; this note does not mark M23 approved.

