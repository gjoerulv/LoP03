# M50 — Town travel rework

## A. Status and authority

- **Status:** ☑ complete (approved) — approved by the owner **2026-07-23** after
  manual testing, committed as `4640c50`. Authored just-in-time on 2026-07-23
  from the
  owner-approved M47–M51 "Court & Comfort" plan. Fourth milestone of that
  program; M51 follows, **then** M23 → M24. See §J for the as-implemented record.
- **Authority:** `CLAUDE.md` → this note → `docs/milestones.md` →
  `docs/game_design.md` → `docs/technical_design.md`.
- **Base checkout:** `ee079d4` ("M49"), working tree clean at authorization.
- **Baseline:** 477/477 tests, `--capture` 58/58 scenes overflow-clean.
- **Versions at baseline:** `battle::kBattleRulesVersion` **9**,
  `dungeon::kGenerationVersion` **10**, `kSaveVersion` **1**,
  `kSettingsVersion` **1**. **No bump anywhere in this milestone** — it is pure
  town-layer presentation/flow; spawn memory is transient.
- **Terminal state for this session:** `implemented, awaiting manual approval`.

## B. Re-audit against `ee079d4`

| Plan claim | Verified? |
|---|---|
| Town is code-authored 26×15 tiles at 16 px = 416×240 | ✅ `buildTown` in [TownData.cpp:22](../../src/town/TownData.cpp:22) (`kW=26, kH=15`); `Tilemap::kTileSize = 16` |
| Exits are Door tiles at the bottom border, entered by **Confirm** | ✅ `carveExit` puts them at `(col, kH-1)` = (2,14) / (23,14); `TownState::handleInput` travels on `Confirm` when `nearExit_` |
| Castle exit at (13,0), north edge | ✅ `col = kW/2 = 13`, row 0 |
| Spawn is fixed (12,8) → pixel (192,128) | ✅ `TownLayout.spawnPixel = {12*16, 8*16}` |
| `DungeonState`'s walk-on trigger is the pattern to copy | ✅ [DungeonState.cpp:776](../../src/states/DungeonState.cpp:776): walking onto a `DoorTile` calls `enterRoom` immediately; the destination spawns you at the **interior gap** (one tile in), so you never arrive standing on a trigger — that is the anti-bounce mechanism, and M50's latch generalizes it |
| The dungeon centers with an origin offset + M46 stage matte | ✅ [DungeonState.cpp:196](../../src/states/DungeonState.cpp:196) (`originX_`/`originY_`) and the matte at [855–894](../../src/states/DungeonState.cpp:855) — the template for the compact town |
| **The town renders at raw (0,0), no origin offset** | ⚠️ Confirmed and this is the milestone's biggest mechanical cost. `TownState::render` draws tiles at `tx*ts`; `player_` and **every** interaction query (`exitAtPlayerTile`, `buildingAtPlayerTile`, `onBardTile`, `onBlackMarketTile`) do tile math in that same origin-0 space. Centering means adding `originX_/originY_` and offsetting **render only**, keeping `player_`/collision/interaction in tile-local space — exactly as the dungeon does. §E2. |
| Footer strip height | ✅ `ui::style::kFooterHeight = 16`; the old exits at pixel y=224 sit under it, which is why the plan moves them |
| Bard tile | ⚠️ Hardcoded `kBardTileX=6, kBardTileY=9` in `TownState` — must be re-placed for the compact interior. §E1. |
| **Black-market tiles are layout-coupled and hardcoded** | ⚠️ `kBlackMarketTiles` in [BlackMarket.hpp:38](../../src/game/BlackMarket.hpp:38) = `{5,7},{20,7},{9,8},{16,6},{12,6}` "in the fixed 26×15 town", asserted by `test_black_market.cpp:59` (`x<25, y<14`, not spawn 12,8). Must be re-authored for 24×12 and the test updated. §E1. |
| **A persisted market offer stores its tile** | ⚠️ `blackMarketTileX/Y` are saved ([SaveSystem.cpp:69](../../src/save/SaveSystem.cpp:69)). A pre-M50 save with a present offer at e.g. (20,7) would place the NPC on a wall/out-of-bounds in the smaller town. Defensive snap on load (§E1) keeps it reachable — no crash either way, but without the snap that one offer is unreachable until a new run overwrites it. |
| `TownState` is constructed `(stack, context)` everywhere | ✅ 7 sites in `CaptureRunner`, plus `PartyCreationState` (new game) and `SlotMenuState` (load). A spawn-hint parameter must **default** to today's centre spawn so these are untouched. §E3. |
| Returning from the castle is a `push`/`onResume`, not a `travelTo` | ✅ `TownState` pushes `CastleState`; the player is still standing on the north exit when it pops. With a walk-through north trigger this would **instantly bounce back into the castle** — the latch (disarm on spawn AND on resume until the player steps off every trigger) is what prevents it. §E4. |
| Only layout test is `test_town.cpp` (dims 26/15, 7 buildings, doors reachable) | ✅ — dims assertions change; door-reachability stays; new latch + spawn-mapping tests added |

## C. Goals

A compact, centred, walk-through town: it looks like the dungeon rooms (M46
stage matte), its edges are **road triggers you walk into** rather than doors you
Confirm, and arriving from a neighbour drops you at the matching edge so travel
reads as continuous movement. Building entry is unchanged (Confirm on a door).
No version bumps; nothing new is persisted.

## D. Constraints

- Pure town-layer work. No battle, generation, save, settings, or scoring change.
- The spawn hint is **transient** — passed through construction/`travelTo`, never
  saved. A load or New Game uses the default centre spawn.
- Movement, collision, and interaction stay in **tile-local pixel space**; only
  rendering gains the origin offset (the dungeon precedent), so the change is
  contained and the pure movement tests are untouched.
- All new UI uses the M46 kit (`drawFrame`, `palette()` roles, the stage-matte
  construction copied from `DungeonState`).

## E. Slices

### E1 — The compact centred town (`buildTown` + the coupled coordinates)

`buildTown` shrinks to **24×12 tiles (384×192)** with the tree border kept. All
seven buildings, the bard spot, the market candidate tiles, the plaza spawn, and
walk paths are re-laid into the 22×10 interior; **door-Confirm building entry is
unchanged**. `TownState` computes `originX_ = (vw - 24*16)/2 = 21`,
`originY_ = (vh - kFooterHeight - 12*16)/2 = 16`, so the town sits centred above
the footer with the M46 matte behind it.

Coupled coordinates re-authored for the new layout, each validated against the
map (open Ground, clear of buildings/doors/exits/spawn):

- **Bard** `kBardTileX/Y` (`TownState`).
- **`kBlackMarketTiles`** (5 cells) + `test_black_market.cpp`'s bounds
  assertion (`x < 23, y < 11`, not the new spawn).
- **Save compat:** on load, if a present market offer's stored tile is now solid
  or out of bounds, snap it to `kBlackMarketTiles[0]` (defensive, in
  `SaveSystem`) so a pre-M50 offer stays reachable. No version bump — this is
  defensive loading of an existing optional field.

### E2 — Centring: the origin-offset refactor (`TownState`)

Keep `player_`, `resolveMove`, and every `…AtPlayerTile` query in tile-local
space (origin 0). Add `originX_/originY_`, set in `buildForCurrentTown`, and
offset **only** the draw calls in `render()` (tiles, building labels, exit
signposts, bard, market NPC, player sprite) — the exact split `DungeonState`
uses. Draw the stage matte (canvas + inset band + stepped corner brackets)
before the tiles, copied from `DungeonState::render`. This is mechanical but
touches every draw site, so it is its own slice.

### E3 — Walk-through exit triggers

Exits become **road/arch trigger tiles on the edges, clear of the footer**:

- **west edge, mid-height** → previous town;
- **east edge, mid-height** → next town;
- **north gap** → the castle (town 7 only).

Rendered as a short road/arch indicator into the border plus the existing
label/lock treatment. `TownExit` keeps its fields; `buildTown` places the
triggers at the edges instead of the bottom border. Walking onto an **unlocked**
trigger travels (no Confirm); walking onto a **locked** one shows the reason in
the footer and does nothing (today's locked-exit copy, moved from Confirm to
walk-on). `travelTo` is unchanged — it keeps the fade, the door SFX, and the
per-town audio switch.

The spawn-hint enum is passed to `travelTo`, which forwards it to
`buildForCurrentTown`:

```
enum class TownEntry { Default, FromWest, FromEast, FromNorth };
```

### E4 — The latch (anti-bounce) and entrance memory

**Latch.** A trigger is *armed* only after the player has occupied a
non-trigger tile since the last spawn/resume. `buildForCurrentTown` and
`onResume` set `travelArmed_ = false`; `update` sets it true the first frame
`exitAtPlayerTile() == nullptr`. A trigger fires only when armed. This kills two
bounce cases: spawning on a trigger (entrance memory places you one tile in, but
the latch is the belt-and-suspenders), and **returning from the castle**, where
`onResume` leaves the player on the north trigger — the latch holds until they
step off.

**Entrance memory.** `travelTo(dest, entry)` computes the arrival spawn:
- arriving from the **west/lower** town (you pressed east) → just inside the
  **east** trigger, facing west-into-town;
- arriving from the **east/higher** town → just inside the **west** trigger;
- new game / load / leaving a building or dungeon / castle return → the default
  centre spawn (castle return keeps the player where they stood, as today).

"Just inside" = one tile inward from the trigger, so arrival is never *on* a
trigger. The hint is derived at the call site from `toNext` (a west-edge trigger
of the destination is where you land when travelling east, and vice-versa) and
is never persisted.

## F. Tests

`test_town.cpp` (extended) + pure helpers:

- **Layout invariants** (compact `buildTown`): dims 24×12; 7 buildings; every
  building door in bounds, a `Door` tile, walkable; the spawn walkable; **every
  exit trigger tile walkable and reachable** (flood-fill from spawn reaches each
  trigger and each building door); triggers clear of the footer row.
- **Bard + market tiles** land on open Ground, clear of buildings/doors/exits/
  spawn (a pure check over the shipped layout, mirroring the existing market-tile
  test at the new bounds).
- **Spawn mapping:** a pure `townEntrySpawn(layout, entry)` returns the correct
  just-inside tile per `TownEntry`, and never a trigger tile.
- **Latch model:** a pure predicate — a trigger fires only once the player has
  been off all triggers since spawn — with the arrive-on-trigger and
  step-off-then-on sequences.
- **Save compat:** a present market offer at an out-of-bounds/solid tile snaps to
  a valid one on load; a valid one is untouched; absent offer unaffected.

## G. Capture

Refresh the three town scenes whose layout changed —
`06_town`, `25_town_ladder`, `22_town_high_contrast` — and add one scene
standing at an unlocked road trigger (the walk-through affordance). 58 → **59**.
Suite stays overflow-clean.

## H. Documents to update

`docs/game_design.md` (town travel is now walk-through; the compact centred
look), `docs/technical_design.md` (the origin-offset render split, the trigger
latch, the transient spawn hint, the market-tile re-author + defensive snap),
`docs/manual_test_matrix.md` (rows 37–39 town-travel — update to walk-through;
add a bounce-loop check), `docs/milestones.md`, `README.md` (the "roads at the
bottom corners" sentence), this note's §J.

## I. Acceptance criteria

1. The town is 24×12, centred, with the M46 stage matte; all 7 buildings, the
   bard, and the market spots are present and reachable; building entry is still
   Confirm-on-door.
2. West/east/north edges are walk-through road triggers clear of the footer;
   locked ones show the reason and do not travel.
3. Arriving from a neighbour spawns at the matching edge; no arrive→bounce loop
   in any direction, and no bounce back into the castle on return.
4. Travel keeps the fade, door SFX, and per-town audio switch.
5. No version bumps; old saves load (including a pre-M50 market offer, which
   stays reachable); the spawn hint is never persisted.
6. Full suite green in Debug **and** Release; capture 59/59 clean.

## J. As-implemented record

All slices landed as planned.

### As built

- **Compact town** (`buildTown`): 24×12, centred via `originX_=21`/`originY_=16`
  inside the M46 stage matte (band + stepped corner brackets copied from
  `DungeonState::render`). Five top services (Inn / Item / Equip / Guild /
  Training, bodies y1–2), two bottom (Scoreboard / Save, y8–9), the castle gap at
  column 13, spawn (11,5), bard (3,5). Layout constants live in `TownData.hpp`
  (`kTownWidth`/`kTownHeight`/`kExitRow`/`kCastleCol`/`kSpawnTileX`/`kSpawnTileY`)
  as the single authority the layout, the bard/market spots, and the tests read.
- **Origin-offset render split**: `player_`, `resolveMove`, and every
  `…AtPlayerTile` query stay in tile-local space; only `render()` adds
  `originX_/originY_`. HUD chips + footer stay screen-space. No movement/collision
  logic changed — the pure movement tests were untouched.
- **Walk-through triggers**: edges carry `Path` road gaps (west/east at
  `kExitRow`, castle at column 13 top). Travel resolves in `update()` on an armed,
  unlocked trigger; Confirm no longer travels. Locked roads show the footer reason
  and are inert.
- **Latch + entrance memory**: `travelArmed_` clears on every
  `buildForCurrentTown` and `onResume`, arms once off all triggers.
  `townEntrySpawnPixel(TownEntry)` lands the player one tile inside the matching
  edge (east travel → west road, and vice-versa); castle return is an `onResume`
  that disarms the latch so the player steps down off the north road rather than
  bouncing back in. The hint is never persisted.

### Deviations from this note

1. **Black-market tiles + bard re-authored** exactly as flagged in §B/§E1
   (`kBlackMarketTiles` → `{5,5},{18,5},{9,6},{14,6},{5,10}`; bard → (3,5)), and
   `test_black_market.cpp`'s bounds assertion now derives from
   `kTownWidth`/`kTownHeight`/`kSpawnTileX/Y` instead of hardcoded 26×15 numbers,
   so it tracks any future resize.
2. **Save defensive snap** added to `SaveSystem` (§E1): a present market offer
   whose stored tile is not a current `kBlackMarketTiles` cell snaps to
   `kBlackMarketTiles[0]` on load, so a pre-M50 offer stays claimable. No schema
   change. (In practice all five *old* tiles happen to remain walkable in the new
   map, but the snap removes the reliance on that luck.)
3. **Capture:** `06_town` / `25_town_ladder` / `22_town_high_contrast` refresh
   automatically (they rebuild the town), and **`59_town_road`** was added showing
   the player parked on an unlocked west road with the "Walk out to Town 2"
   footer — via a small `captureStandAtWestExit()` capture hook, since a live
   unlocked trigger would travel instantly. 58 → **59 scenes**. The
   `25_town_ladder` scenario's market tile was moved to a canonical (14,6).

### Files changed

- **Source:** `src/town/TownData.hpp/.cpp` (compact layout, constants,
  `TownEntry`, `townEntrySpawnPixel`), `src/states/TownState.hpp/.cpp` (origin
  split, walk-through triggers, latch, spawn hint, capture hook),
  `src/game/BlackMarket.hpp` (market tiles), `src/save/SaveSystem.cpp` (defensive
  snap), `src/capture/CaptureRunner.cpp` (new scene + market-tile fix).
- **Tests:** `tests/test_town.cpp` (dims, flood-fill reachability, trigger
  placement, spawn mapping), `tests/test_black_market.cpp` (bounds from the
  layout constants).
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md`,
  `docs/technical_design.md`, `docs/manual_test_matrix.md` (rows 37/37b/38),
  `README.md`.

### Compatibility

No version bumps (`kSaveVersion` 1, generation 10, rules 9, settings 1). Old
saves load unchanged; a pre-M50 market offer is snapped to a valid tile. The
`TownEntry` spawn hint is transient. Nothing about battle, generation, scoring,
or determinism changed — this is a pure town-layer presentation/flow milestone.

### Known limitations

- The `TownEntry::FromNorth` spawn is defined and unit-tested but not reached by
  a live caller: castle return is an `onResume` that keeps the player where they
  stood (on the north road), with the latch preventing a bounce. It documents
  intent and guards the model if a future path reconstructs the town on castle
  return.
- Walk-through travel fires the instant the player steps onto an unlocked road,
  which is the intended feel; a player brushing an edge will travel. The arrival
  spawn faces them inward and the latch prevents a *loop*, but the responsiveness
  itself is a feel item for owner validation (matrix row 37).
