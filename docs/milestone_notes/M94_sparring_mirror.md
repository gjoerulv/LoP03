# M94 — The sparring mirror

**Status:** complete (approved 2026-08-16)
**Program:** M88–M97 (authorized 2026-08-14). No version bumps — a new
battle CONSTRUCTION (shared builder), a driver-side routing flag, and a
state-side snapshot; the engine's rules are untouched and normal play is
byte-identical (the targeting sites became actor-relative, which
degenerates to the historical values for party actors).

## Scope (plan-review item 1)

"In the training hall you should be able to fight a copy of your own team
(AI controlled, or manually controlled)."

## What was built

- **`battle::buildSparBattle`** (shared, deterministic): the party side
  through the one real path, then every unit mirrored as an enemy-side
  echo — "Echo <name>", `partyIndex -1` (the writeback can never reach the
  real party), `uncontrolled` cleared (the driver decides for a Jester
  echo); threat table + battle seed re-derived over the full roster in
  lockstep with buildBattle's finalization.
- **Manual mode**: `BattleState(..., manualEnemies)` — a non-forced
  enemy-side turn routes through the SAME command phases. Echo turns offer
  Attack/Skill/Guard (Item disabled — echoes carry no bag; Escape
  disabled — that verb ends the spar from the player's side). Forced
  echo turns (Terrified/Stunned/Confused) still auto-resolve, exactly as
  they do for party members.
- **`SparState`**: snapshots the WHOLE `Party` object before the fight and
  restores it in `onResume` regardless of outcome — zero stakes by
  construction (HP, MP, bag, gold, records, and bestiary can never move;
  the anti-farming rule is structural, not a checklist). Closes on a
  one-line modal. Null spoils + a discarded local `RunStats` sink.
- **Training Hall**: two rows under the roster — "Spar: face your echoes"
  and "Spar: manual control (both sides)". The M79 member cycling stays
  member-scoped.
- Also makes the spar a safe future testbed: heirlooms (M96) and summons
  (M95) will fire inside it with everything restored after (summon
  `usedSummons` resets on spar entry per the M95 plan).

## Compatibility

Saves, schemas, seeds, scores, battle rules: untouched.

## Automated validation

- `test_spar.cpp`: mirror fidelity (stats/pools/kit/element equality,
  Echo naming, severed partyIndex), deterministic seed, threat-table
  lockstep, both-sides-AI resolvability inside the round cap, and the
  empty-mirror reward belt. Build/full-suite/capture results ride the
  program's session report (run after the M93+M94 batch).
- Captures: `111_spar_closing` (new) + `10_training_hall` (lints the new
  rows).

## Manual owner checklist

Matrix row **181** — both modes, all three outcomes, before/after ledger
equality, and the feel judgment (especially manual mode's echo turns).

## Known limitations

- Echo turns in manual mode show the standard command UI with no special
  "ECHO" banner beyond the acting unit's name — candidate polish if the
  owner finds sides hard to track.
- Echoes fight without the party's item bag by design (their kit is the
  members' own skills).

## Documentation updated

`docs/milestones.md` (M94 row) · `docs/game_design.md` (§4 sparring
paragraph) · `docs/technical_design.md` (§47) ·
`docs/manual_test_matrix.md` (row 181).

## Final status

`implemented, awaiting manual approval`
