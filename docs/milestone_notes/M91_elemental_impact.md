# M91 — Elemental impact presentation

**Status:** implemented, awaiting manual approval
**Program:** M88–M97 (authorized 2026-08-14). Presentation-only — no
battle-rules, generation, save, or schema change; the Simulator never sees
any of it.

## Scope (owner item 5)

"Attacks with elements should have an associated simple animation on the
target and sound effect."

## What was built

- **Accents** (`src/render/ElementFx.hpp/.cpp`): six procedural
  stepped-pixel motifs (fire wedges, ice shards, a lightning bolt, earth
  rubble, a holy radiant plus, a dark wisp ring) drawn over each HIT unit
  during the sequencer's impact beat — M46 language, no textures, no RNG
  (the single growth step derives from the pulse strength, so captures
  stay stable). Strength = `BattleSequencer::flashStrength()`, so the
  Battle Flash accessibility gate is inherited (off = nothing; floats
  still carry the information); High Contrast collapses to shape-only in
  the palette text color.
- **Wiring** (`BattleState.fxElement_`): set beside every resolved
  `useSkill`/`attack` across all five action paths (player, enemy,
  confused, uncontrolled/Jester, the M48 capture scene) — a skill carries
  its own element, a basic attack the actor's `weaponElement` (wielded
  AND intrinsic), items/status-ticks carry None. Accents draw on exactly
  the `hitFlags_` units, in both sprite and fallback-rectangle branches —
  so immune targets (no HP delta) never flash an element, automatically.
  Both directions by construction: the Dragon's breaths land the same
  accents on the party.
- **Voices**: six Sfx roles appended (`HitFire..HitDark`, ids
  `sfx.battle.hit_*`) + pure `audio::elementHitSfx()` (None → HitMagic);
  `commitPresentation` routes damage beats through it; a role whose FILE
  is missing remaps to HitMagic inside `AudioManager::play` (M14 degrade
  rule). Six deterministic WAVs added to `generate_audio.ps1` (crackle /
  glass / crack / thud / bloom / breath), manifest entries, credits row
  now 21 SFX files.

## Compatibility

Nothing but presentation: identical battles, seeds, saves, scores.
Existing battle/sequencer tests pass unmodified.

## Automated validation

- Debug + Release builds clean (VS2022 shell).
- `[audio]` battery green (new: the element→role mapping + table-append
  pins; the shipped-WAV validator now counts 45 files, all PCM16 mono
  22050 Hz).
- Full `ctest --preset debug` run started this session — see the ledger
  status line in the completion report; all previously-green suites were
  re-run after the only test edit (the WAV count pin 39 → 45).
- Capture — 109/109 scenes clean.

## Manual owner checklist

Matrix row **175**: each element readable and audible both ways; Battle
Flash off and High Contrast honored; taste judgment on motifs and voices
is the owner's (regenerate/replace WAVs freely — the manifest is the
contract).

## Known limitations

- Accents are single-frame-pulse motifs, not multi-frame animations — the
  deliberate floor of "simple animation"; richer sheets can replace them
  later without touching the wiring.
- The battle log does not name the element (the float pair Weak!/Immune
  already covers the tactical read).

## Documentation updated

`docs/milestones.md` (M91 row) · `docs/game_design.md` (§9 "Elements are
seen and heard") · `docs/technical_design.md` (§44) ·
`docs/manual_test_matrix.md` (row 175) · `assets/credits.md`.

## Final status

`implemented, awaiting manual approval`
