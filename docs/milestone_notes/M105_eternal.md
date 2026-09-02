# M105 — Eternal: the town-7 endless descent

**Status:** implemented, awaiting manual approval
**Program:** M98–M108 "Are P Geese" (owner-authorized 2026-08-16). NO
generation bump (a new mode; every existing shape generates byte-identical)
and no battle-rules bump. Save schema additive-only: `eternalBestFloors`
(optional int; old saves → 0).

## Scope (owner item + interview rulings)

A fourth Floors choice at town 7: endless, no score, floors-beaten record,
a boss on every floor, always depth 20, **escalating per floor** (owner
chose the Endless-Rush-style curve over flat), warning before entry that
the stakes rise to depth 20 regardless of outcome.

## What was built

- **Generation** (`dungeon::generateEternalFloor`): floor k is the
  STANDALONE generation of `floorSeed(runSeed, k)` at the fixed depth 20 —
  which already carries its real theme boss, so "a boss on every floor"
  needed no new machinery, only NOT applying the M82 warden swap. Teams
  gain a flat +10 %pts per floor past the first (danger tiers recompute,
  labels stay honest); the map-piece room is cleared. Deterministic at any
  index — a reload replays the identical staircase.
- **The run** (`DungeonState`): `Dungeon.eternal` + an unreachable
  floorCount sentinel make `finalFloor()` never true, so every boss-slot
  fight is the existing StairGate flow (victory opens the stairs, nothing
  completes, nothing scores) and `descendFloor` generates the next floor
  on demand. Each felled floor-boss increments the run counter and banks
  `party.eternalBestFloors = max(...)` immediately. A persistent
  "Eternal N" HUD chip rides beside the patrol counter. Trove/map/black
  market are naturally absent (all keyed to completion, which never
  fires); patrols, the Surveyor, chests and events all work normally.
- **The Guild**: the Floors stepper cycles 1 → 4 → 20 → **Eternal** at
  town 7 (elsewhere the cycle is unchanged); Depth reads "20 (fixed)" and
  refuses adjustment while Eternal is selected; "Eternal best: N floors"
  shows under the panel at town 7. Enter Dungeon first raises the
  M87-viewport warning (ConfirmPromptState, safe answer "Not today")
  stating the whole price; Descend then applies the stakes rule — a
  genuine M33 raise (baseline → (town 7, depth 20); if that raised, the
  penalty ladder resets as any raise does) — BEFORE the entry autosave,
  so no reload sheds it.

## Verification

- New tests (`test_floors.cpp` [eternal]): per-floor determinism at
  indices {0, 1, 7, 42}, real-boss-every-floor, exact escalation vs the
  standalone generation, sentinel/flag/map-room stamps, and the stakes
  rule's raise semantics. Suite + capture recorded in the completion
  report.

## Deviations from the plan

- No separate Eternal result screen: runs end by retreat or carry-out
  like any dungeon, and the record is banked per floor (live), so a
  result panel had nothing to add. The Guild row and HUD chip carry the
  record instead. Flagged for the owner's judgment.
- No new record BOARD: `eternalBestFloors` is one number on the party
  (the plan's "record" wording), not a scoreboard.

## Manual owner checklist

Matrix row **201**: the stepper cycle at town 7 and at town 1–6; the
locked depth; the warning's exact price (decline once); an entry followed
by an instant retreat (stakes STILL raised — check the Guild's penalty
forewarn afterwards); a few floors of descent (boss every floor, +10 %pts
visible in danger tiers, the Eternal chip, the record updating live and
surviving save/reload); a wipe (carry-out as usual, record kept).
**Judge the escalation pace and the stakes price.**

## Documentation updated

game_design §6 (the Eternal descent passage), ledger row, matrix row 201,
this note.
