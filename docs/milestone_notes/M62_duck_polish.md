# M62 — Fixes & Duck stagecraft

First milestone of the owner-approved M62–M66 program (plan approved
2026-07-27, including the 54-entry class-milestone bonus table the later
milestones consume). Implemented 2026-07-27 on the working tree carrying
M59–M61 (base checkout `6a39693` + the uncommitted M61 delta).

## A. Status

**◑ implemented, awaiting manual approval** — set 2026-07-27. Evidence in §E.

## B. Scope (owner brief)

1. The two audit wording findings: the challenge result overlay's confirm
   prompt claimed "Return to the Castle" after the Duck gauntlet, and the
   pop comment beside it said "castle hub".
2. **Purify still heals if any ailments are cured. It should not.**
3. The Deadly Duck and his minions need graphics, and the Duck should have
   his own music.

## C. As implemented

### Purify heals nothing (battle rules 12 → 13)

Root cause: `purify` is a **heal-category** skill with `power: 0` and the
cleanse control, and the heal branch of `Battle::useSkill` applies
`healValue = power + magic/2` to every living target unconditionally — so
every Purify cast healed the caster's magic/2, on the whole party, since
M43. (It also accrued heal-threat.) The rule was invisible to the M47 test
because its board was at full HP, where the heal clamps to zero.

The fix is schema-driven and sits at the one shared chokepoint: a
heal-category skill with `power == 0` **and** the cleanse control skips the
heal, its log line, and its threat. The cleanse itself, the revive branch,
and powered cleanses (Generous Mending, power 18) are untouched. Sim ==
live by construction (one `useSkill`). This changes how any battle with a
Purify cast resolves → `kBattleRulesVersion` **13** (history comment in
`Battle.hpp` extended).

### Wording

`CastleChallengeState::render` builds a kind-aware prompt ("Return to
Goose Town" for `CastleChallenge::DuckGauntlet`, "Return to the Castle"
otherwise); the pop comment now says "whichever hub pushed us".

### Bespoke art (replaces the M61 placeholder rows)

`tools/asset_gen/generate_textures.ps1` gains an appended, reseeded M62
section (`$script:rng = 62620000` — every earlier PNG proven byte-identical
by `git status` after a full regeneration): a shared right-facing goose
base and five role-accented Evil Geese (24×24, elite Horns tuft) — helmed
vanguard, violet-hooded hexwing, cream-mantled mender with a green cross,
belled motley trickster, bog-stained bogfeather — plus the 32×32
`boss_deadly_duck`: a massive dark duck rising from pond water, green-
sheened head, burning eyes, great gold beak, and **no crown** ("the pond
needs none of that"). `assets/manifest.json` rows repointed from the
goose-actor placeholder; provenance rows added to `assets/credits.md`.

### The Duck's own music

New `MusicTrack::DuckBattle` (`music.duck`, battle-tier synth fallback) in
`AudioRoles.hpp` (enum + id table + synth map + count); an appended
`generate_audio.ps1` entry renders `duck.wav` — a lumbering comedic-ominous
F-minor march at 132 BPM with an oom-pah "waddle" bass, deliberately
weightier than the King's chromatic drive. `CastleChallengeState` selects it
for the gauntlet's Duck wave (the geese wave stays plain, the King keeps
his own theme). Manifest + credits rows added.

## D. Files changed

`src/battle/Battle.{hpp,cpp}` (rules 13 + the pure-cleanse guard),
`src/states/CastleChallengeState.cpp` (prompt, comment, music),
`src/audio/AudioRoles.hpp`, `tools/asset_gen/generate_textures.ps1`,
`tools/asset_gen/generate_audio.ps1`, `assets/manifest.json`,
`assets/credits.md`, 6 new PNGs + 1 new WAV (generated),
`tests/test_rules_v7_flow.cpp` (the wounded-board v13 case + a comment
correction), `tests/test_audio.cpp` (shipped-file count 38 → 39), docs
(`game_design.md`, `technical_design.md` §19, `milestones.md`,
`manual_test_matrix.md` rows 147–148, `README.md`, the M61 note's
addressed limitations).

## E. Automated validation (2026-07-27)

- New test `rules v13: Purify heals nothing even on a wounded ally`: on a
  wounded board Purify moves no HP and accrues no threat while still
  cleansing; Generous Mending still heals; rules pin ≥ 13.
- Both asset generators re-run: **only the 7 new files appeared; every
  pre-existing PNG/WAV stayed byte-identical** (verified via `git status`).
- Targeted batteries `[rules],[audio],[lint],[battle]`: 108 cases / 4944
  assertions green after the one honest count-pin update (38 → 39 shipped
  WAVs).
- Closing verification: **563/563 Debug and 559/559 Release tests green**
  (each one up on M61 for the new v13 case; the 4-case gap is the
  debug-only god-mode battery); `--capture` **77/77 scenes clean**; Release
  built clean.

## F. Manual owner checklist

1. Matrix rows **147–148**: cast Purify on a wounded, poisoned member —
   afflictions lift, **zero HP restored**, no "recovers" line; Generous
   Mending still heals; a Remedy is unchanged.
2. Fight the Duck gauntlet: five distinct goose sprites, the crownless
   Duck rising from the pond, his own waddling march on wave 2, and the
   result overlay's prompt reading **"Return to Goose Town"**.
3. Sanity-check a King fight (his theme and sprite unchanged) and any
   Cleric dungeon run (Purify's new honesty is a real nerf to sustain —
   flag if it needs compensation, which was deliberately not included).

## G. Known limitations

- Purify's nerf is uncompensated by design (the owner's brief); the M63
  Cleric level-20 option **Purifying Light** deliberately offers the old
  behaviour back as a choice.
- Goose Town's hub music and the Castle battle backdrop are unchanged
  (only the Duck wave got its own theme); a bespoke pond stage remains a
  possible later direction.

## H. Final status

`implemented, awaiting manual approval`
