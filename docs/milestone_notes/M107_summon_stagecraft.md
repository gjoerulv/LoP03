# M107 — Summon stagecraft

**Status:** complete (approved 2026-09-02)
**Program:** M98–M108 "Are P Geese" (owner-authorized 2026-08-16).
Presentation-only — no battle-rules, generation, or save change; the
Simulator never sees any of it.

## Scope (owner item)

"The summoning skills as-is are underwhelming visually. They should show
the summoned creature at the center, and play an epic animation (epic
relative to fit the game's feel)."

## What was built

- **The apparition** ([render/SummonFx](../../src/render/SummonFx.hpp)):
  when a legend answers a call, it appears LARGE at the battlefield's
  center for the resolution beat — procedurally drawn (the ElementFx
  idiom: deterministic, palette-true, no RNG, no new sprite pipeline).
  The **Mighty G. Goose** wears the real goose sprite at triple scale
  with a golden burst and a bob; the **Starfall Sentinel** descends as
  falling star-streaks into a bright rotating lattice; the **Radiant
  Spring** blooms concentric rising ripples. The burst work collapses
  under Battle Flash = Off (the M18 gate); the beat's length scales with
  the message-speed setting like the quip it accompanies, so Fast play
  stays fast.
- **The arrival fanfare**: one new SFX role (`Sfx::Summon`,
  `sfx.battle.summon`, a rising G–C–E–G stack with a triangle shimmer,
  deterministic via `generate_audio.ps1`; manifest + credits rows; role
  tables grown 21 → 22). One shared voice for the three legends —
  per-summon voices are the owner's call for a later pass.
- **Wiring**: the M95 quip site now also stages the apparition and plays
  the fanfare; a timer renders it over the combatants and under the
  panels. New capture scene **117_summon_goose** freezes the goose
  mid-beat over a five-enemy field (`captureShowSummon`).

## Verification

- New tests: the skill→apparition mapping (three legends, nothing else);
  the audio file-count pin moved 45 → 46 and the role tables self-check
  through the existing [audio] coverage suites. Full suite + capture in
  the completion report.

## Deviations from the plan

- Procedural stagecraft instead of authored spritesheet animations: the
  plan's "3 generated spritesheets with manifest animations" would have
  meant blind-authoring pixel frames; the procedural route ships real,
  deterministic, settings-aware theatre now, with the goose anchored on
  its true sprite. If the owner wants richer creature art, the manifest
  route stands ready.
- No BattleSequencer surgery: the apparition rides its own timer beside
  the quip (the proven idiom) rather than a new sequencer stage.

## Manual owner checklist

Matrix row **203**: cast each legend (debug-grant + teach); the creature
appears centered with its burst and fanfare; Battle Flash Off strips the
burst; Fast message speed shortens the beat; enemies and panels stay
readable beneath. **Judge the "epic relative to the game's feel" bar —
this is the milestone's whole point.**

## Documentation updated

game_design summons passage, ledger row, matrix row 203, credits.md,
this note.
