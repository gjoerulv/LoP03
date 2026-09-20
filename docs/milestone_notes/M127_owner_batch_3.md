# M127 — Owner batch 3

**Status:** complete (approved 2026-09-20)
**Authorization:** owner request 2026-09-20 ("A new small batch"), made in the
same session as M126; one milestone, like the batch before it. Branch
`oyb12`, baseline `d789f31` plus the uncommitted M120–M126 work.
**Version motion:** none. Battle rules 19, generation 25 (M126), save v1,
settings v1, content v1, `project(VERSION)` 0.9.0. The manifest gains
seventeen new texture ids; no existing id or file changed.

## Scope (the owner's words)

1. The character sprites are shown on the save slots, left of the play time,
   vertically centred in the slot.
2. The highlighted slot's party has a jumping animation, like the victory
   screen.
3. The map pieces are redesigned.
4. The curios have their own icons beside their names, like equipment and
   skills; the icon is also shown when a curio is inspected.

## Clarified before work began (owner answers, 2026-09-20)

- **"The map pieces should be redesigned as how they are shown in the Map
  menu"** could mean the Maps screen's drawing or the dungeon pickup. Asked;
  the owner chose **both**: new parchment art on the Maps screen **and** a
  matching scrap on the dungeon floor.
- **Curio icons**: one shared, one per theme, or one each. The owner chose
  **twelve, one per curio**.

## What was built

### The party on the save slots

- `save::SlotSummary::members` — class id and a fallen flag per member, in
  party order, filled by `SaveSystem::summary` (no save-format change: it is
  read from the party the slot already stores).
- `SlotMenuState` draws the members' battle sprites at 1× between the label
  and the clock, centred on the row's slab (titled rows have a taller slab and
  centre on that). The clock moved 6 px right and the sprites step 20 px (their
  24 px canvases have empty margins), so the widest label — the autosave row
  at level 99 with seven digits of gold — still fits beside four sprites.
- The highlighted, enabled row's party hops on the state's own clock:
  **`src/render/PartyHop.hpp`** holds the victory screen's per-member
  frequency, amplitude and phase tables, lifted out of `CelebrationState`
  (which now calls the same function — the arithmetic is unchanged); the
  slots use 30 % of the amplitude. A member saved at 0 HP lies dimmed and
  does not hop; a greyed row dims its sprites and keeps them still; an
  unknown class id draws nothing.

### The treasure map

- One hand-placed **100×56** parchment grid in the generator, shown at 2×:
  the sea and a ragged coast with two gulls, three peaks under a snow cap, a
  wood of seven crowns, a dotted trail crossing three pieces to a red X, a
  compass star, two old stains, a ragged edge with a dark rim. The generator
  cuts it into four pieces along two zigzag tears and saves each on the full
  canvas, so `MapsState` simply draws every owned piece at the same origin —
  they fit by construction — over a dark board; a piece not yet found leaves
  the board and its "?". The old primitive sketch remains only as the
  fallback for a missing texture.
- The dungeon's Secret Map Piece is `prop.map_piece`: a 12×12 torn scrap of
  the same parchment with a trail and the X (outlined like every prop). The
  gold "?" box stays underneath as the missing-texture fallback.

### Curio icons

- Twelve 10×10 icons, `ui.icon.curio.<curio id>` (`curioIconTextureId`): a
  broken coronet, a tattered pennant, a rusted key, a stone ear, a crystal and
  its note, an oil lamp, a split geode, a scratched slab, an acorn, a barred
  quill, a bearded idol, a glowing jar.
- The Maps grid shows the icon before every **owned** curio's name (the icon
  column is reserved on every row so names align; unfound curios stay
  "? ? ?"). The rows pitch at 12 px now; the map moved up 5 px and its frame
  tightened 2 px to make the room.
- The inspect panel shows the icon at 3× in an inset well left of the lore;
  the lore viewport is narrower beside it and scrolls as before.
- The buried treasure's result lists the curio on a row (the M126 listing
  idiom): icon + name in gold over "Buried treasure! Curios: N of 12 - see
  Maps in town."

## Art process (generator discipline)

Both drawings were prototyped outside the repository (a Python sheet at 8×
and at native size) and reviewed by eye before they entered
`generate_textures.ps1` as ASCII grids — one refinement round (the crown
shard read as a blob, the etching's vein as a second treasure X; the map's
side peaks were hidden and the tear crease too dark). The section is
RNG-free and appended last; after a full generator run `git status` shows
only the seventeen new PNGs — every earlier texture is byte-identical.

## Decisions taken without asking (veto any of them)

1. **A downed member lies in the slot row**, dimmed and still — the victory
   screen's own rule, since the owner named that screen as the model.
2. **The slot layout gave way by 6 px** (clock further right, sprites
   overlapping their empty margins) rather than shrinking the label font.
3. **The map is drawn at half resolution and shown at 2×** (chunky pixels,
   like battle sprites on the victory screen) — it keeps the art a reviewable
   hand-placed grid, the project's sprite idiom.
4. **The red X** is the danger red, not the reward gold: gold on parchment
   does not read, and the treasure *is* guarded.
5. **Only the Secret Map Piece's marker was redrawn.** The single-use chart
   ("M") and the buried spot ("X") keep their glyph boxes — the owner asked
   about the map pieces. They are the obvious next candidates.
6. **The dig result** moved the curio's name out of the sentence onto an icon
   row, following M126's "what was received is listed".

## Compatibility

No save, settings or content change. A build without the new textures still
works: the map falls back to the primitive sketch, the pickup to its glyph
box, curios to plain names.

## Known limitations

- The hop's height (about 3–5 px) lets a hopping sprite's head rise just
  above the highlight slab; intended, but the owner judges the feel.
- Four 24 px sprites at a 20 px step overlap by their margins; the Dragon's
  wide sprite sits close to its neighbours.
- A curio's lore is narrower beside the icon well, so long entries scroll a
  line or two sooner.
- Parties of more than four are not drawn beyond the fourth member (the game
  never makes one).

## Documentation updated

`docs/milestones.md`, this note, `docs/game_design.md` (the slot, the puzzle
map, the curios), `docs/technical_design.md` (scene count, §68),
`docs/asset_pipeline.md`, `docs/art_bible.md`, `docs/ui_style_guide.md`
(§13), `docs/manual_test_matrix.md` (rows 275–278), `assets/credits.md`,
`assets/manifest.json`, one-line pointers in the M65, M66, M71, M85 and M123
notes.

## Completion report

### 1. Implementation summary

**M127 — Owner batch 3.** Complete: all four points, with the two
clarifications applied.

### 2. Files changed

- **Source (new):** `src/render/PartyHop.hpp`.
- **Source (changed):** `src/save/SaveSystem.{hpp,cpp}`,
  `src/states/SlotMenuState.{hpp,cpp}`, `src/states/CelebrationState.cpp`,
  `src/states/MapsState.cpp`, `src/states/DungeonState.{hpp,cpp}`,
  `src/game/Curios.hpp`, `src/game/TreasureMap.hpp`,
  `src/capture/CaptureRunner.cpp`.
- **Assets (new):** twelve `textures/ui/icons/curio_*.png`, four
  `textures/ui/map/piece_*.png`, `textures/props/map_piece.png`;
  `assets/manifest.json`, `assets/credits.md`;
  `tools/asset_gen/generate_textures.ps1`.
- **Tests:** `tests/test_owner_batch_3.cpp` (new), `tests/CMakeLists.txt`.
- **Docs:** see "Documentation updated".

### 3. Plan deviations

None from the request; see "Decisions taken without asking".

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

All run 2026-09-20 from the VS 2022 developer shell (amd64):

- `tools/asset_gen/generate_textures.ps1` - **ran clean** (21 s); `git status`
  on `assets/` afterwards lists only new files - no tracked texture changed.
- `cmake --build --preset debug` - **succeeded** (game, CrystalForge, tests).
- `crystal_tests.exe "[m127],[lint],[save],[m126]"` - **58 test cases, 112780
  assertions, all passed** (the three `[m127]` cases among them).
- `ArePGeese.exe --capture <dir>` - **182/182 scenes clean** (two new:
  `181_map_piece_floor`, `182_dig_curio`). Read by eye: `11`, `43`, `167`,
  `172` (slots - standing, hopping, fallen, dimmed), `80`, `82` (the map at
  two and four pieces, the icon grid), `96` (the inspect well), `181`, `182`.
- `cmake --build --preset release` - **succeeded**.
- `ctest --preset debug` - **964/964 passed** (637 s).
- `ctest --preset release` - **960/960 passed** (590 s).

### 6. Manual owner validation

Matrix rows **275–278**: the slot sprites and their hop, the parchment map at
every piece count, the floor scrap, the curio icons in the grid, the dig
result and the inspect panel. The art itself — whether the map and the
twelve icons look right — is the owner's to judge.

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`complete (approved 2026-09-20)`
