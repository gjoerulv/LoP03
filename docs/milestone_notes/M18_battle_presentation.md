# M18 — Battle Presentation and Game Feel

## A. Status and authority

- **Status:** implemented, awaiting manual approval (see §N).
- **Last reviewed repository commit:** `a9426b9` (M17, 2026-07-19). Re-audit:
  much of §C's hierarchy already exists from M12/M15/M17 (turn line + cursor,
  disabled commands, skill/item costs + descriptions, round counter, boss
  telegraph at intro, framed panel, tier sprites, KO tint, target box);
  missing are action staging, impact feedback (flash/shake/hit-stop), KO/
  victory presentation, effect settings, and a disabled-command reason.
  `BattleState.cpp` is 726 lines; the sim applies results synchronously and
  presentation is a flat message+pause (`resolveSeconds`).
- **Owner decision (2026-07-19): settings revision approved** — optional
  `effectFlash` / `effectShake` fields ("full" / "reduced" / "off", default
  full, no version bump; old files load unchanged). Flash is a brighten
  pulse (no strobing at any level); shake is 2px full / 1px reduced.
- **Scope note from re-audit:** the simulation has no boss phase/enrage
  state (BossDef carries only a telegraph line), so "escalation visuals"
  would require a combat-rule change — excluded by §D; the telegraph remains
  the boss-mechanic surface.
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

## N. As-implemented record (2026-07-19)

**Delivered.**

- **Effect settings** (owner-approved revision): `settings::EffectLevel`
  (full/reduced/off) with optional `effectFlash`/`effectShake` fields —
  absent = full, no version bump, pre-M18 files load unchanged; two new
  Settings rows (menu respaced to fit 12 rows); round-trip/defaulting/
  invalid-value tests.
- **Pure sequencer** (`src/render/BattleSequencer`): Windup (0.18s) →
  Impact (0.14s) → Settle (`resolveSeconds`) staging of an already-final
  sim result. Fast halves staging; Instant and impact-less actions (guard)
  skip straight to settle; `skip()` jumps to the end; a one-shot commit
  signal marks when numbers/SFX/displayed HP appear. Flash strength and
  shake offset honor their levels (flash is a decaying brighten pulse —
  never a strobe; shake ±2px full / ±1px reduced, impact-only).
- **BattleState integration:** `displayHp_` mirror commits at impact so HP
  bars, HP text, KO tags, and enemy fade-outs change when the hit lands,
  not when the sim computed it; acting unit lunges 4px toward the foe; hit
  units brighten; floating numbers hold during impact (hit-stop) then rise;
  fallen enemies sink away over 0.4s (party stays visible for revives);
  SFX moved to the impact beat; Confirm skips any point of the sequence
  with the commit still firing; grayed commands explain themselves ("No
  skills learned." / "No usable items."); the defeat message now states
  its consequences (fixes audit UI-INFO-014).
- **Sim boundary:** `src/battle/` untouched; every pre-existing battle and
  simulator test passes **unmodified**. 208/208 total (7 new).

**In-game evidence** (`docs/screenshots/m18_battle/`, scripted): Settings
screen with the two new rows; a real 3-enemy Crystal Mine gate battle —
target selection, a live impact frame (Frost Imp brightened by the flash
with the damage number held during hit-stop), the post-impact frame
(committed HP bar), and the field after the imp's KO (sunk away, target
box on the next enemy, party damage taken). Clean exit code 0.

**Deviations / notes:**
1. No impact particles or elemental effect sprites — the flash/shake/
   number/lunge set covers §C's "restrained feedback" without new assets;
   elemental effect art can ride M21/M20 if wanted (recorded as a possible
   follow-up, not a gap in acceptance).
2. Boss escalation visuals excluded (no sim phase state — see §A scope
   note); the telegraph at intro remains the boss surface.
3. Hit-stop is implemented as the impact-beat number hold rather than a
   global freeze — full freezes read as jank at 0.14s scale.
4. MP changes display instantly (only HP is staged); staging MP added no
   readability value.
