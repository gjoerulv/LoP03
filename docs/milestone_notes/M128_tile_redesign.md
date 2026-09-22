# M128 — Town & dungeon tile redesign (trees, seasons, wall variants)

**Status:** implemented, awaiting manual approval
**Authorization:** the M128–M130 "Peak 80s pixel art on Atari" program,
authorized 2026-09-22 by plan approval after a planning interview (one plan,
one authorization, the M117–M119 pattern). Branch `oyb13`, baseline
`5f6ff56` (v0.9.1).
**Version motion:** none. Battle rules 19, generation 25, save v1, settings
v1, content v1, manifest v2, `project(VERSION)` 0.9.1. The manifest gains 72
texture ids and loses 12; no id that a save, a score or a rule reads changed.

## Scope (the owner's words, 2026-09-22)

> I want to redesign the town looks. 1st town: Green trees. 2nd Town: Yellow
> trees. 3rd town: Orange trees. 4th town: Red trees. 5th town: White trees.
> 6th Town: bare trees. 7th town: Bare scary trees (kinda shaped like skulls).
> Trees should not repeat in the same pattern. Have 4 different per town in
> different patterns, so it looks more organic. In the dungeons, the walls
> also can have 4 different patterns so it's not just the same repeating over
> and over. It should look like the game is from the 80s, but it can look a
> bit more random and organic.

Clarified in the interview: "Peak 80s pixel art on Atari" is the **late-80s
Atari ST look** (bold flat fills, hard edges, 3–5 colours per element,
strong silhouettes, little or no dithering), and the **ground follows the
season subtly** per town alongside the trees.

## Design principles applied

From a survey of 2D-RPG tile practice made for this program:

- **Variants by position hash, never by RNG stream.** A pure integer mix of
  (seed, x, y, salt) picks a cell's variant, so a cell always draws the same
  tile and nothing touches generation. The pre-M128 accent pick,
  `(x*31 + y*17) % n`, is a linear congruence and stripes diagonally; the
  new mixer is the SplitMix64 finalizer the dungeon already uses for events,
  and the tests sweep rows, columns and diagonals for any short period.
- **Weighted picks with a dominant quiet tile.** Walls: one plain course at
  55 % over three motifs (20/15/10); ground: two quiet fleck scatters
  (45/25) over two tufted layouts (15/15); trees: four full crowns near
  equal (30/25/25/20) so no ring favours one shape.
- **One shared silhouette, several canopy states.** The five leafy towns
  share four crown shapes (round, tall oval, wide leaning right, twin-lobed)
  and differ by season palette and detail; the bare and the skull towns
  have their own four silhouettes each.

## What was built

### The town ladder's trees (28 tiles)

`tiles.town.<N>.tree.<1..4>` for N = 1..7, hand-placed 16×16 grids, no
outline pass (environment tiles tile edge to edge), the season's ground
behind each crown:

| Town | Family | Palette |
|---|---|---|
| 1 | green broadleaf | vegetation ramp (rim `veg0`, shadow `veg1`, base `veg2`, highlight `veg3`) |
| 2 | yellow | **gold-leaf** ramp (new) |
| 3 | orange | **ember** ramp (new) |
| 4 | red | **crimson-leaf** ramp (new) |
| 5 | white | the neutral-white ramp with stone shadows (frost) |
| 6 | bare | trunk and branch silhouettes on the earth ramp — a tall fork, a spreading crown of twigs, a wind-bent trunk with a stub, a dead snag |
| 7 | bare and scary | skull crowns on dead-wood trunks — a frontal skull, an antlered one, a cracked cranium with a gaping jaw, a small skull on a twisted trunk: bone in the stone ramp, sockets in the night ramp |

The trunk is shared (earth2 with an earth3 lit edge and a bark-shadow root),
the light comes from the top-left, and every canopy has a hard one-pixel rim
in its darkest step — the Atari-ST read, no anti-aliasing, no dithering.

### The seasonal ground (28 tiles)

`tiles.town.<N>.ground.<1..4>`: two plain fleck scatters and two tufted
layouts per town, tones following the trees — lush loam with green tufts,
yellowing, dry straw with ember flecks, crimson-flecked earth, a frosted
stone ground with white flecks and frost crystals, dead earth with stubble,
ash with grey stubble. **Verified during planning:** `buildTown` never
places `Tile::Grass` — the plaza is all `Ground` — so the grass tiles and
the flowers accent were dead code paths and stay as they are (the M32
tinted `grass`/`path`/`building` copies still ship and are still
lint-required for towns 2–7).

### Four wall variants per dungeon theme (16 tiles)

`tiles.<theme>.wall.<1..4>`, each theme's legacy identity (art bible §8b)
kept as the plain variant and three motifs added:

- **Ruined Keep** — coursed masonry / a stepped crack with a stone jarred
  loose / an arrow slit with a lit jamb and a sill / ivy creeping up from the
  foot.
- **Crystal Mine** — braced rock with two minerals / a cyan-violet crystal
  vein / the right post split with splinters and the beam sagging / a
  miner's lamp on an iron bracket.
- **Hollow Forest** — trunk-and-root mass under a moss cap / a hollow knot in
  the middle trunk / two shelf fungi on the left trunk / moss spilling down
  two trunks.
- **Goosy Gauntlet** — the reed palisade bound by two earth bands / a rope
  knot on the upper band / a gap in the reeds over dark water with a glint /
  a nest wedged on the lower band with one pale egg.

### Placement (code)

- `src/render/TileVariant.hpp` (pure, header-only, raylib-free):
  `tileHash(seed, x, y, salt)` — the spatial primes 73856093/19349663 on x
  and y, XOR the seed and the salt, then the SplitMix64 finalizer
  (`dungeon::themeEventHash`'s, duplicated so `render/` never depends on
  `dungeon/`) — and `tileVariant(seed, x, y, salt, weights, count)`, a
  weighted bucket walk; the shipped tables `kTreeVariantWeights`,
  `kGroundVariantWeights`, `kWallVariantWeights`.
- `TownState::render`: the town's tree and ground variant ids are resolved
  ONCE per frame (callers cache nothing across a manifest reload) and picked
  per cell with `seed 0, salt = town index` — every town's identical ring
  geometry reads as its own organic pattern. Town 1 now takes the per-town
  path too (it used the unsuffixed base ids before). Fallback chain: the
  variant → variant 1 → the M32 per-town id → the base id → the coloured
  rectangle.
- `DungeonState::render`: `Tile::Building` cells pick
  `tiles.<theme>.wall.<v>` with `seed = dungeon_.seed`, `salt =
  kSaltWallVariant + currentRoom_`, resolved once per frame so the cell loop
  allocates nothing; fallback variant 1, then the legacy `wall`. No layout,
  `roomMap_`, `kGenerationVersion` or RNG stream is touched — presentation
  only.

### Generator, manifest and the review gate

- `tools/asset_gen/generate_textures.ps1`: three foliage ramps in `$PAL`
  (`goldleaf0..2`, `ember0..2`, `crimson0..2`) and a named key for the
  already-shipped bark shadow; the section "M128 tile redesign" appended
  before the generator's last line adds the grid keys (`j k l` gold-leaf,
  `4 5 6` ember, `7 8 9` crimson-leaf, `0` bark shadow), `Save-TileGrid`
  (throws unless exactly 16×16, throws on ANY transparent pixel — tiles are
  opaque — and runs no `Outline` pass) and the 72 grids. RNG-free: after a
  full run `git status assets/` shows the 72 new PNGs, the 12 deletions
  below and nothing else — every earlier texture is byte-identical.
- `tree` and `ground` left the M32 `$townTiles` ColorMatrix list; the twelve
  tinted copies `town<2..7>_tree.png` / `town<2..7>_ground.png` and their
  manifest rows are **removed** (the first non-additive asset diff in a long
  time — deliberate, and `ShadeCopy` consumes no RNG, so the removal is
  byte-safe). The legacy `town_tree.png`, `town_ground.png` and
  `<theme>_wall.png` keep shipping as fallbacks: their generator lines
  consume the shared RNG stream, so removing them would shift every later
  speckled texture.
- `tools/asset_gen/preview_tiles.ps1` (new review harness, the
  `preview_icons.ps1` pattern; reads the PNGs, writes only into
  `docs/sprite_review/`): `tiles_towns.png` — every town's four trees and
  four grounds magnified beside its ring at 3× and 1×, placed by the same
  hash (replicated in inline C#: Windows PowerShell reads 64-bit hex as
  signed and never wraps) — and `tiles_walls.png`, the four walls per theme
  beside a room perimeter at 3× and 1×.

### Art process

Prototyped in a scratchpad Python/PIL script (labelled sheets at 6–8×, the
town ring and a room perimeter at 1× and 3× under the replicated hash),
reviewed by eye, refined (the first ground had eleven flecks per tile and
laticed at 80/20 — cut to five flecks over four layouts; the ivy was
stringy — thickened), then the PowerShell section was EMITTED from the same
data. The style was proved on towns 1–2 and the keep before the rest was
drawn. The grids in the generator are the source of truth; the scratchpad
prototypes die with the session.

## Tests

- `tests/test_tile_variant.cpp` (`[m128]`): determinism and range;
  degenerate tables; the salt and the seed each change the pattern; the
  weights hold within 3 % over 10 000 cells; no period ≤ 8 along the ring's
  rows and columns for every town's salt, nor along rows, columns and both
  diagonals of a 100-cell field; and every shipped M128 tile is a 16×16
  texture (the PNG header check, `[lint]`).
- `tests/test_presentation_lint.cpp`: towns 1..7 must resolve
  `.tree.1..4` and `.ground.1..4` (town 1 was covered by no loop before);
  towns 2..7 still `.grass`/`.path`/`.building`; every theme resolves
  `wall.1..4` beside its legacy `floor`/`wall`/`door`.

## Captures

Six new scenes, `183_town_2` … `188_town_7`, inserted after `25_town_ladder`
(each sets its town explicitly, like the ladder and road scenes around it,
and clears the black market and the guild records so no pending
town-milestone offer modals over the exterior). `06_town`, `14/15/16_dungeon_*`
and `118_dungeon_goosy` pick the new art up unchanged. **188 scenes**.

## Decisions taken without asking (veto any of them)

1. **Four ground layouts, not two.** The plan said a plain and a tufted
   tile; at 80/20 the plain tile's fleck pattern laticed across the plaza,
   so the ground has two quiet scatters and two tufted layouts (45/25/15/15).
2. **Town 1 is redrawn too** — one consistent seven-family ladder and one
   code path; the old ellipse tree is only the never-drawn legacy fallback.
3. **Floors and doors keep today's art** (walls only, as asked); the floors
   already carry the sparse accent variant.
4. **The tree ring's layout and collision are untouched** — art never
   changes collision; the organic feel comes from the four variants and the
   hash.
5. **The dead Grass/flowers branch is left as is** (the tufted ground gives
   the plaza its texture); it is cleanup for a later day.
6. **No canopy overhang** above the top row: tiles stay strictly 16×16 and
   the matte/label band stays clean.
7. **The white town is frosted white** — the neutral-white ramp with stone
   shadows on a frosted stone ground, rather than a fourth new ramp.

## Compatibility

No save, settings, content, generation or score change. A build without the
new textures falls back to variant 1, then to the M32 per-town tiles (path
and building still exist; tree and ground do not, so the base tile draws),
then to the coloured rectangle — the game never crashes on a missing tile.

## Known limitations

- The bare (town 6) branches are one pixel wide in places; at 1× they read
  as small forks. If the owner wants them bolder, the four grids are
  `town6_tree1..4` in the generator.
- The M32 tinted `grass`, `path` and `building` copies still carry the old
  darkening progression under the new trees; the facades and monuments are
  unchanged by design (the owner's scope named trees and ground).
- The tinted `grass` copies are never drawn (no Grass cell exists) but are
  still shipped and lint-required; removing them is a separate cleanup.

## Documentation updated

`docs/milestones.md` (row + program section), this note,
`docs/game_design.md` (§5 the ladder's seasonal identity),
`docs/technical_design.md` (draw-order rule, the M32 presentation
paragraph, the capture count, a new M128 section), `docs/art_bible.md` (§2
foliage ramps, §6 town, §8b wall variants), `docs/asset_pipeline.md` (the
tile family, `Save-TileGrid`, `preview_tiles.ps1`),
`docs/manual_test_matrix.md` (rows 279–281), `assets/credits.md` (the M32
row narrowed, three M128 rows), `assets/manifest.json`,
`docs/sprite_review/tiles_towns.png` + `tiles_walls.png`.

## Completion report

### 1. Implementation summary

**M128 — Town & dungeon tile redesign.** Complete: all three points of the
scope plus the interview's seasonal ground.

### 2. Files changed

- **Source (new):** `src/render/TileVariant.hpp`.
- **Source (changed):** `src/states/TownState.cpp`,
  `src/states/DungeonState.cpp`, `src/capture/CaptureRunner.cpp`.
- **Assets:** 72 new `textures/environments/town<N>_tree<v>.png`,
  `town<N>_ground<v>.png`, `<theme>_wall<v>.png`; 12 deleted
  `town<2..7>_tree.png` / `town<2..7>_ground.png`; `assets/manifest.json`
  (+72/−12 rows); `assets/credits.md`;
  `tools/asset_gen/generate_textures.ps1`; `tools/asset_gen/preview_tiles.ps1`
  (new); `docs/sprite_review/tiles_*.png` (new).
- **Tests:** `tests/test_tile_variant.cpp` (new), `tests/CMakeLists.txt`,
  `tests/test_presentation_lint.cpp`.
- **Docs:** see "Documentation updated".

### 3. Plan deviations

The four ground layouts (decision 1) instead of two. Nothing else.

### 4. Compatibility

See "Compatibility". Deterministic seeds: unchanged (no generation code or
RNG stream touched; the generation version stays 25).

### 5. Automated validation

All run 2026-09-22 from the VS 2022 developer shell (amd64):

- `tools/asset_gen/generate_textures.ps1` — **ran clean** (11 s); `git status`
  on `assets/` lists the 72 new PNGs, the 12 deletions and the manifest —
  no other texture changed.
- `tools/asset_gen/preview_tiles.ps1` — ran clean; both sheets read by eye.
- `cmake --build --preset debug` — **succeeded**, no warnings.
- `crystal_tests.exe "[m128],[lint],[town]"` — **30 test cases, 96 141
  assertions, all passed**.
- `ArePGeese.exe --capture <dir>` — **188/188 scenes clean** (six new). Read
  by eye: `06`, `183`–`188`, `14`, `15`, `16`, `118`, `59`.
- `ctest --preset debug` — **970/970 passed** (334 s).
- `cmake --build --preset release` — **succeeded**, no warnings
  (`ctest --preset release` runs once at the end of the program).

### 6. Manual owner validation

Matrix rows **279–281**: walk every town and judge the trees, the seasonal
ground and the organic ring; walk a room of every dungeon theme and judge
the wall variants; open the two review sheets. The art itself — whether the
seven families and the sixteen walls look right and read as "peak 80s" —
is the owner's to judge.

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`implemented, awaiting manual approval`
