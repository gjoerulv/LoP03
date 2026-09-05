# M115 — The Last Dragon: redesign, clone identity, bounds-aware formation

**Status:** implemented, awaiting manual approval
**Program:** M109–M116 (owner-authorized 2026-09-02 via the approved plan).
**No version motion:** rules 19, generation 24, saves v1, content v1,
manifest v2 (the Dragon's id and path unchanged; the PNG's bytes are new).

## Scope (plan section M115; owner sections 19, 21–22)

The Last Dragon redrawn as a dragon silhouette within the 36×36 boss
canvas; the clone bug fixed at the root (a `Combatant` visual identity
independent of `isBoss`); a bounds-aware enemy formation with pure
geometry tests so the Dragon and its clone never overlap.

## What was built

- **Clone identity** (`Combatant.bossArt`): set beside `isBoss = true` in
  `buildBattle`'s boss block; the `SummonCloneSelf` struct copy carries it
  to the clone while `isBoss = false` stays. The bug: `drawUnit` branched
  on `isBoss` alone, so the clone (a non-boss `summonSlot` unit with the
  Dragon's `sourceId`) missed `boss.the_dragon.battle` and the class-echo
  fallback and landed on the tier-generic beast. Now `enemySpriteId` —
  the ONE resolver shared by `drawUnit` and the formation — draws the boss
  family for `isBoss || bossArt`. Bestiary, telemetry (`defeats` never
  counts a `summonSlot`; `dragonDefeats` increments in the castle finish),
  boss rules and the M46 accent keep their `isBoss`/`summonSlot`
  semantics.
- **Formation** (`battle_ui::UnitEnvelope`, `kEnvelopeGap = 2`,
  `enemyRowYs(envelopes, baseY)`; `BattleState::rebuildFormation`,
  `enemyRowY_`): each enemy ordinal's row line is computed from every
  unit's real envelope (`above` = texture height − 16, `below` = 22, the
  meter's bottom, which the M114 status column stays inside) with the M101
  centre-out seating; adjacent screen rows need `below + above + gap`, a
  short pair lifts the upper row and every row above it (24-over-36 gives
  exactly the old `kBossHeadroom`; 36-over-36 gives 10 more... exactly
  what the Dragon and its clone need: 2 px between the clone's meter and
  the Dragon's crown, the bracket clear too), and a block that would
  climb above y 0 is pushed down whole. Rows are computed once per roster
  (the ctor, the Mimic morph) from the loaded textures (family defaults
  36/24 when a texture is missing); `unitScreenPos`, the render loop and
  the target sort read them; `enemyRowOffset` stays as the M101 pin and
  the fallback; `bossOnField()` now serves only the M46 accent pair.
- **Art:** `boss_the_dragon` redrawn — a horned head with a snout and
  exactly one red eye, a neck, one raised wing with daylight between its
  membrane fingers (S5 negative space), gold wing claws, foreclaws and
  hind claws, a stone-banded torso, a tail curling out the trailing side
  with a spade tip, gold spines, smoke from the nostril; void ramp with
  stone highlights; asymmetric, facing right. Hand-placed rows in the
  generator (drawn as literal grid rows), RNG-free; the generator re-run
  changed only `boss_the_dragon.png`. Credits row updated.
- **Captures:** `140_dragon_clone` (the clone raised, centre-top,
  targeted) and `141_dragon_target` (the Dragon targeted);
  `captureEnterTargeting(cursor)` gained the cursor argument.

## Deviations from the plan

- `enemyRowOffset` was kept (not retired) as the fallback for an ordinal
  beyond the computed rows and as the M101 pin the new API is checked
  against; `bossOnField()` was kept for the M46 accent.
- The envelope's `below` is a constant 22 for every enemy (the meter's
  bottom); the status column's extent is inside it by construction (two
  9 px lines from y+4 reach y+22).

## Tests

`tests/test_battle_formation.cpp` [battle][formation][dragon]: the M101
pins reproduced through `enemyRowYs` for every count with and without a
centre boss (the old headroom falls out of the envelope rule); the Dragon
+ clone rows never overlap (sprite, meter, bracket) with the gap, are
deterministic, and a mixed five-unit roster with two tall units stays
apart and on screen; a tall unit in the top slot pushes the block down
rather than off-screen while the M101 order holds; a sixth-slot clone
under a boss stays below everything. `tests/test_dragon_art.cpp`
[dragon][formation]: the Dragon's clone slot carries `bossArt` and stays a
non-boss with the Dragon's `sourceId`; ordinary foes and a dungeon boss's
minions carry none. The existing [dragon] battery is unmodified.

## Compatibility

No version motion. One PNG's bytes change.

## Automated validation

See the completion report below.

## Manual owner checklist

Matrix rows **227–228**: the Dragon's read against the other bosses; the
clone wearing the same art, seated centre-top with nothing overlapping;
every other boss fight and a full 5-enemy patrol seated exactly as before.
**Judge the silhouette.**

## Documentation updated

`docs/milestones.md` (rows + section), `docs/game_design.md` (the Dragon
passage), `docs/technical_design.md` (§56), `docs/art_bible.md` (§5 the
roster line), `assets/credits.md` (the Dragon row), the M85 and M101
notes (superseded-in-part pointers), `docs/manual_test_matrix.md` (rows
227–228), this note.

## Completion report

### 1. Implementation summary

- **Milestone:** M115 — The Last Dragon: redesign, clone identity,
  bounds-aware formation.
- **Slices completed:** the clone identity; the shared sprite resolver;
  the envelope formation and its rebuild seams; the art; captures; tests;
  docs. All completed.
- **Player-facing changes:** the Dragon looks like a dragon; its clone
  looks like the Dragon; the two never overlap.
- **Engineering changes:** `Combatant.bossArt`, `battle_ui::UnitEnvelope`
  / `enemyRowYs`, `BattleState::enemySpriteId` / `rebuildFormation` /
  `enemyRowY_`, `captureEnterTargeting(int)`.

### 2. Files changed

- **Source:** `src/battle/{Battle.hpp,Battle.cpp}`,
  `src/states/{BattleFormation.hpp,BattleState.hpp,BattleState.cpp}`,
  `src/capture/CaptureRunner.cpp`.
- **Tests:** `tests/test_battle_formation.cpp` (extended),
  `tests/test_dragon_art.cpp` (new), `tests/CMakeLists.txt`.
- **Content/data:** `assets/textures/enemies/boss_the_dragon.png`
  (regenerated), `assets/credits.md`, `tools/asset_gen/generate_textures.ps1`.
- **Documentation:** as listed above.
- **Build/release configuration:** none.

### 3. Plan deviations

See "Deviations from the plan" — routine.

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

- **Build:** `cmake --build --preset debug` — clean, zero project-code
  warnings (VS 2022 developer shell).
- **Targeted tests:** `crystal_tests.exe "[formation],[dragon],[battle],
  [lint]"` — **136 cases, 84 562 assertions green** (the three new
  formation cases, the two clone-art cases, the unmodified [dragon]
  battery).
- **Art:** `generate_textures.ps1` re-run — `git status` shows only
  `boss_the_dragon.png` changed among the shipped textures.
- **Capture lint:** `ArePGeese.exe --capture <dir>` — exit 0, **141/141
  scenes clean** (`140_dragon_clone`, `141_dragon_target` new; every boss
  and 5-enemy scene re-rendered through the envelope layout at its old
  rows).
- **Full suite / Release:** ride the program's closing battery.

### 6. Manual owner validation

Matrix rows 227–228.

### 7. Known limitations

- The silhouette is the owner's judgment; the preview sheet and the two
  captures are the evidence.

### 8. Documentation updated

As listed above.

### 9. Final status

`implemented, awaiting manual approval`
