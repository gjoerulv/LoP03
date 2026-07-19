# M18 — Battle Presentation and Game Feel

## A. Status and authority

- **Status:** planned
- **Last reviewed repository commit:**
  `a316f244e870718aa27d9995dc871e11572ad429` (2026-07-19).
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M18 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Planning fidelity:** architectural. Sequence timings and effect intensity
  are owner-tuned during implementation; battle-sprite scope depends on the
  M15 art bible. Re-audit and refresh this note against the then-current
  repository before implementation begins.

## B. Problem statement

Battle is mechanically complete and deterministic (`battle/` pure simulation,
80+ combat tests), but its presentation is minimal: rectangles for combatants,
floating damage numbers as the only action feedback, dense fixed-coordinate UI
in `BattleState.cpp` (601 lines — the largest state), no action staging, no
battle-speed options, and hierarchy problems recorded in the M11 audit.
Because the game's score premise is *fewest battle turns across many repeated
battles*, slow or unskippable presentation would damage the core loop — the
roadmap explicitly rejects "animation that punishes score play".

## C. Goals

- A clear information hierarchy, always answering: whose turn; legal commands
  (and why one is unavailable); selected target and valid target set; HP/MP
  and statuses; skill/item description and cost; current round count (score
  context); action message and result; boss telegraph/escalation state.
- Short action sequences (anticipation → motion/projectile → impact →
  number/status feedback → brief recovery → next action) that read clearly at
  Normal speed and compress at Fast/near-instant.
- Restrained, configurable feedback: hit stop, sprite flash, screen shake,
  impact particles, elemental/heal/status effects, KO/victory feedback, boss
  phase/enrage feedback — all honoring the M13 settings (flash/shake
  intensity, battle speed).
- **Presentation timing fully separated from simulation results:** effects
  visualize an already-computed deterministic outcome; no animation may change
  results or timing-dependent logic.

## D. Non-goals

- No combat-rule, formula, AI, or turn-order changes.
- No balance tuning (M19/M23).
- No audio content (M21) — hook points only.
- No long cinematic animations; brisk resolution is a design requirement.
- No changes to escape/score semantics.

## E. Dependencies

- M12 layout system (battle UI migration should already be layout-safe;
  this milestone builds hierarchy on top).
- M13 settings (speed/flash/shake options must exist to be honored).
- M14 manifest + M15 art bible (battle sprites/effects direction).
- M17 recommended first (actor sprite pipeline exists), confirm ordering at
  refresh.
- Owner tuning sessions for timing/intensity.

## F. Proposed slices (provisional — refine before starting)

1. **Information hierarchy** — restructure battle UI panels (turn indicator,
   command/target clarity, status display, description/cost panel, round
   counter) on the M12 primitives.
2. **Presentation sequencer** — a pure, time-injected action-presentation
   queue that consumes simulation results and emits staged visual events;
   testable headlessly; speed settings scale/skip stages.
3. **Battle sprites + effects** — manifest-driven combatant sprites and
   impact/element/status effects per the art bible; KO/victory/defeat
   feedback; boss telegraph/escalation visuals.
4. **Feel tuning + options wiring** — hit stop/flash/shake behind M13
   settings (off/reduced/full); Normal/Fast/near-instant verified with the
   owner.

## G. Expected systems affected

- `src/states/BattleState.cpp` — major restructure; extract presentation
  sequencing into a dedicated module rather than growing the state.
- New battle-presentation module (pure sequencing + raylib rendering split).
- `src/battle/` — **read-only**; simulation must not change (its tests are the
  tripwire).
- `assets/` — battle sprites/effects + manifest entries.
- `src/audio/` — SFX hook points (content in M21).
- `tests/` — sequencer logic, speed-scaling, settings-honoring tests.

## H. Data, schema, and save compatibility

- Simulation determinism: **unchanged by definition** — identical inputs keep
  producing identical outcomes; existing battle/simulator tests must pass
  untouched.
- Settings: uses M13 fields (battle speed, flash, shake); adding fields is a
  reviewed settings-schema revision.
- Saves, content schemas, seeds, scoring: **no impact.**

## I. Automated validation

- Existing battle and simulator tests pass **unmodified** (outcome
  invariance).
- Sequencer unit tests: stage ordering, duration scaling per speed setting,
  skip behavior, settings honoring (flash/shake off ⇒ no such events).
- Manifest validation for battle assets.
- Full suite green.

## J. Manual validation for the owner

- Fight normal, elite, boss, and five-enemy battles at Normal, Fast, and
  near-instant speeds, keyboard and gamepad.
- Status-heavy battle: statuses, costs, and telegraphs readable without
  opening any manual.
- Reduced/off flash and shake settings visibly honored.
- Mute the audio: combat remains fully understandable.
- Judge pacing: Normal feels punchy, Fast materially reduces downtime,
  nothing feels like a slog across a 10-battle run.

## K. Acceptance criteria

- Source, target, effect, and result are always understandable.
- Normal actions resolve briskly; Fast materially reduces downtime.
- Reduced-flash and reduced-shake settings work.
- Statuses and telegraphs are visible without external reference.
- Combat is fully understandable with audio muted.
- Deterministic battle outcomes are unchanged (tests prove it).

## L. Risks and failure modes

- **Sim contamination** — presentation reaching into `battle/` and altering
  outcomes; the read-only rule and untouched tests are the control.
- **Pacing damage** — cumulative animation time punishing score play; the
  near-instant path and owner pacing sessions are required.
- **BattleState monolith growth** — the 601-line state absorbing a sequencer;
  extraction into a module is part of the milestone, not optional cleanup.
- **Effect accessibility** — flash/shake defaults harming sensitive players;
  settings must apply to every effect, and M22 will harden further.
- **Per-frame allocation** — particles/effects in the hot loop; pool/reuse.
- **Second-order:** stronger presentation may change perceived difficulty;
  M19's balance audit runs after this and will catch drift.

## M. Required documentation updates on completion

- `docs/milestones.md`; this note (as-implemented + deviations).
- `docs/technical_design.md` — presentation-sequencer architecture and the
  sim/presentation boundary.
- `docs/game_design.md` — battle-presentation and speed-option description.
- `docs/manual_test_matrix.md` — battle presentation rows.
- `docs/asset_pipeline.md` — battle effect/sprite authoring if new metadata.
- Completion report per `docs/milestone_completion_template.md`.
