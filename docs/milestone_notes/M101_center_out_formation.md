# M101 — Center-out battle formation

> **Superseded in part (M115, 2026-09-02):** the fixed boss headroom became a
> bounds-aware envelope layout (`battle_ui::enemyRowYs`) that reproduces
> these rows for the M101 cases; see
> `docs/milestone_notes/M115_last_dragon.md`.

**Status:** complete (approved 2026-09-02)
**Program:** M98–M108 "Are P Geese" (owner-authorized 2026-08-16).
Presentation ONLY — no battle-rules, generation, or save change; the sim's
unit order and every seeded outcome are byte-identical (the [battle]
suites run unmodified).

## Scope (owner item)

"As-is, bosses always appear at the top. The boss should be at the center,
always. In fact, enemy should spawn in in this order: center → center-top
→ center-bottom → top → bottom."

## What was built

- **The mapping** ([BattleFormation.hpp](../../src/states/BattleFormation.hpp)):
  a pure `enemyRowSlot(ordinal)` — fill order `[2, 1, 3, 0, 4]` on the
  five-row grid, the owner's spec verbatim. `buildBattle` constructs the
  boss as the FIRST enemy unit (M7 rule), so the boss always lands
  dead-center with its court around it; plain teams lead with their first
  member there. Past five (the M75 summon clone as a sixth unit) rows
  continue downward exactly as the old sequential stack did.
- **Both geometry sites** use it: the render loop's row placement and
  `unitScreenPos` (which anchors floating numbers, the Miss/Weak/Immune
  marks, and the M91 element accents) — one mapping, no drift. Rows are
  fixed per unit for the whole battle: the dead keep their seat, nothing
  reflows mid-fight.
- **Target cycling reads visually**: `sortTargetsByScreenY()` reorders
  `targetCandidates_` top-to-bottom after every build site (attack, skill,
  item, and the capture path), so Up/Down walks the column the eye sees.
  Party-side target lists keep sequential rows, where the sort is a
  stable no-op.

## Verification

- Pure mapping tests (`test_battle_formation.cpp`): the verbatim fill
  order, distinct rows for every count 1–6, the clone's row-5 seat.
- Full suite + capture lint: green (run recorded in the completion
  report). Battle captures (5-enemy formations, boss fights, targeting
  scenes) re-render with the new rows and stay overflow-clean.

## Deviations from the plan

None. (The plan's "extend template >5" resolved to pass-through ordinals,
which is exactly the old behavior for a sixth unit.)

## Manual owner checklist

Matrix row **194**: 1-, 3- and 5-enemy fights + a boss-with-court fight —
the boss centered, the fill order as specified, target cycling top-to-
bottom, floats/accents anchored on the right sprites, fallen enemies
keeping their rows.

## Documentation updated

game_design §8 (Formation paragraph), ledger row, matrix row 194, this
note.

## Post-implementation defect fixed (2026-08-17)

Owner-reported overlap: with the boss seated CENTER, the minion in the
row above (center-top) reached into the boss's 36px crown — its sprite
bottom and HP meter cut across the boss's head (rows sit on a 34px pitch;
a 36px boss rises 12px higher into its cell than a 24px enemy; before
M101 the boss held the TOP row, so nothing sat above it and the tight
pitch never showed). Fixed in `BattleFormation.hpp`: `enemyRowOffset`
lifts the two visual rows above the center seat by 10px whenever a boss
is on the field (`kBossHeadroom`), keeping their own 34px spacing; the
render loop and `unitScreenPos` share the mapping so floats and targeting
stay anchored. The geometric contract (the cell above ends above the
boss's crown) is pinned in `test_battle_formation.cpp`; capture
`121_battle_goosy_boss` shows the cleared crown.
