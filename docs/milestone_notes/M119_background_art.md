# M119 — Background art

**Status:** complete (approved 2026-09-18; the battle stages were corrected
2026-09-16, before approval — see the corrective section at the end)
**Program:** M117–M119 (owner-authorized 2026-09-14 via the approved plan;
branch `oyb11`, baseline `72f4c3e`).
**No version motion:** manifest v2 with six new texture ids; 42 PNGs
replaced under their existing ids; content, rules, generation and saves
untouched.

## Scope (plan section M119; the owner's item 6)

"The background art (except from the newly updated cut-scenes) could use
some redesign. They are a bit underwhelming. The background should not be
too intrusive." Owner decision (planning interview): all three surfaces —
the six service interiors, the title screen, and the battle backdrops. The
M113 cutscene stages are the quality bar and stay as they are. Within the
approved art direction: the same ramps, the same restraint rules.

## What was built

- **The six service interiors** (`tools/asset_gen/generate_textures.ps1`,
  the M27 block redrawn in place under its own reseed; the M113 scene helpers
  `GPoly`/`Stars`/`FloorSpeckle` hoisted above it): painted rooms on the M113
  stage recipe — the Inn a timbered common room (beams, a lamplit shuttered
  window, a hearth, a plank floor with a rug), the Item Shop shelves of jars
  up both walls with a herb bundle and a hanging sign over a counter edge,
  the Equip Shop the forge (racked arms, chains, the forge mouth, an anvil),
  the Training Hall a slatted paper dojo (corner lanterns, crossed staves, a
  wall target, a mat), the Scoreboard a hall of honour (fluted pillars with
  banners, a crystal glow, flagstones), the Guild a lodge (beams, a pinned
  wall map, notices, a hearth, a rug). The composition weight sits in the
  side margins and the strips above and below the panels — the opaque
  header band and the frames cover the rest — and the caption row is kept
  quiet at the source. The M32 town shades regenerate from the new bases by
  construction (the section re-reads them from disk).
- **Caption backings**: `ui::drawCaptionBacking(x, y, w, h)` — a 72 % canvas
  strip — behind every caption drawn directly on the art (the
  `docs/ui_style_guide.md` §4 rule): the Inn caption, the Equip Shop phase
  hint (shop mode only), the Training Hall phase captions, the Guild caption
  and its "Eternal best" line, the Scoreboard's empty-board text, and the
  title phrase.
- **The title scene** (`bg.title`, 426×240, a new generator section with its
  own reseed): a still lake at night under sparse stars and a small moon, a
  low ridge and a near shore, reeds at the margins, moonlight and a mist
  band; quiet behind the emblem row and the plaque/phrase band.
  `MainMenuState` draws it first through `drawSceneBackground` (canvas fill
  as the fallback); the emblem, plaque, menu frame and footer are opaque.
- **The painted battle stages** (`bg.battle.{keep,mine,forest,castle,goosy}`,
  426×122 — the band): a far layer under the M56 ink silhouettes —
  `render::battleStageTextureId(stage)` and `drawBattleStage(resources,
  stage, band, phase, accents)` draw the texture between the flat band fill
  and `buildBackdrop`'s rects when accents are on; high contrast skips it
  (today's exact look) and so does a missing texture. The float/status
  corridor (x 70..356, y 8..73 of the band) carries only the base gradient:
  the generator draws every motif under a GDI+ clip that excludes it and
  `Assert-CorridorClear` compares the corridor against a plain reference
  before saving; speckle touches the floor strip only. Keep: a far wall with
  arrow slits and a tower, flagstones and rubble; Mine: a crystal drip line,
  side clusters with facet glints, timber supports, rails; Forest: trunk
  columns, a canopy, roots, fireflies; Castle: tall windows, banners, a
  chandelier, the dais edge; Goosy: reeds, mist, a moonlit pond. The M56
  rect geometry and its five tests are untouched. **Superseded 2026-09-16**
  by the corrective pass recorded at the end of this note (the band is now
  426×150, the corridor rule is gone, the silhouettes were re-seated); kept
  here as history.
- **Manifest + credits**: six new ids; the M27 row amended ("redrawn M119"),
  the M32 row noted, rows for the title and the five stages.
- **Tests**: `tests/test_battle_backdrop.cpp` gains the M119 case — Plain has
  no texture, the five ids are distinct `bg.battle.*` entries present in the
  shipped manifest as textures, and `bg.title` is shipped.

## Deviations from the plan

- The planned `151_battle_stage_high_contrast` capture was **dropped**:
  capture scenes share one `AppContext` settings object and none may mutate
  it (a set `highContrast` would leak into every later scene). The
  high-contrast path is the pre-existing M56 code path (`accents == false`);
  the id/manifest pins and matrix row 241 cover it. (Since the corrective
  pass of 2026-09-16 the accents flag follows the live palette, which the
  capture tool does flip per scene, so `51_battle_high_contrast` and
  `73_backdrop_mine_hc` now capture the real fallback; the dropped scene is
  no longer needed.)
- The interiors use the pre-hoisted M113 helpers rather than a second copy —
  a function definition draws nothing, so no earlier bytes moved.

## Compatibility

- Save / settings / content / seeds / scores: no impact.
- Packaged assets: 42 interior PNGs replaced, six PNGs added, six manifest
  ids; a missing background degrades to the flat fill (interiors, title) or,
  since the corrective pass, the procedural ground plane under the M56
  silhouettes (battle stages).

## Automated validation

See the completion report below.

## Manual owner checklist

Matrix rows **239–242**: the interiors in a low and a high town (restraint
and caption legibility), the title scene, the battle stages per theme and
in the castle, the High Contrast fallback, and (row 242, since the
corrective pass) the grounding of every row on every stage.

## Known limitations

- The painted battle layer is authored against the band's fixed 426×150
  geometry (since the corrective pass); a future change to `kPanelH` or the
  band's y would need the textures re-authored (the generator's constants
  mirror `render/BattleBackdrop.hpp`, and the PNG-size test would fail
  first).

## Documentation updated

`docs/milestones.md` (row 119 + section), `docs/art_bible.md` (§6: the
interior rule, the title scene, the painted stages), `docs/asset_pipeline.md`
(the service-background paragraph, the title/battle paragraph),
`docs/game_design.md` (the M56 backdrop bullet), `docs/technical_design.md`
(the M56 B1 bullet, §60), `docs/manual_test_matrix.md` (rows 239–241),
`assets/credits.md`, `docs/milestone_notes/M27_environment_ambience.md` /
`M32_town_ladder.md` / `M56_boss_stagecraft.md` / `M108_rebrand.md`
(pointers), this note.

## Completion report

### 1. Implementation summary

- **Milestone:** M119 — Background art.
- **Slices completed:** B1 the six interiors + caption backings; B2 the
  title scene; B3 the painted battle stages. All completed (one capture
  scene dropped, see deviations).
- **Player-facing changes:** painted service interiors in every town, a
  title scene, painted battle stages under the familiar silhouettes.
- **Engineering changes:** `ui::drawCaptionBacking`,
  `render::battleStageTextureId` / `drawBattleStage`, `BattleState::render`
  call site, `MainMenuState::render`, six state call sites.

### 2. Files changed

- **Source:** `src/ui/UiDraw.{hpp,cpp}`, `src/render/BattleBackdrop.{hpp,cpp}`,
  `src/states/{BattleState,MainMenuState,InnState,EquipShopState,
  TrainingHallState,GuildState,ScoreboardState}.cpp`.
- **Tests:** `tests/test_battle_backdrop.cpp`.
- **Content/data:** `assets/manifest.json` (+6), `assets/credits.md`,
  `assets/textures/backgrounds/{inn,item_shop,equip_shop,training_hall,
  scoreboard,guild}.png` + their 36 `_t2..7` variants (replaced),
  `assets/textures/backgrounds/title.png` and `battle_{keep,mine,forest,
  castle,goosy}.png` (new), `tools/asset_gen/generate_textures.ps1`.
- **Documentation:** as listed above.
- **Build/release configuration:** none.

### 3. Plan deviations

As under "Deviations from the plan".

### 4. Compatibility

As above: manifest ids and replaced PNGs only.

### 5. Automated validation

- **Build command:** `cmake --build --preset debug` (VS 2022 amd64 dev shell).
- **Test command:** `.\build-msvc\crystal_tests.exe "[backdrop],[lint],[cutscene]"`,
  then the program's closing battery.
- **Results:** the generator re-run (with `Assert-CorridorClear` passing for
  all five stages) changed exactly the 42 interior PNGs and added the six
  new scenes — nothing else moved (`git status`: the byte-stability proof);
  Debug build clean; the targeted tags `[backdrop],[lint],[cutscene]`
  **29/29 cases, 88,033 assertions**; `ArePGeese.exe --capture
  docs\screenshots\m119_captures` **150/150 scenes clean**, zero overflow
  events (the title, the six interiors, the scoreboard boards, the battle
  bands in the Keep, the Goosy pond and the castle inspected as images at
  1×). Program closing battery on the final checkout: `ctest --preset debug`
  **897/897 passed** (661.6 s); Release build clean + `ctest --preset
  release` **893/893 passed** (453.9 s; the Debug-only cases are compiled
  out there).
- **Warnings:** none in project code (both presets).
- **Skipped validation and reason:** none. `CrystalForge --canonicalize`
  was not run — no `data/*.json` changed anywhere in the program.

### 6. Manual owner validation

Matrix rows 239–241.

### 7. Known limitations

As above.

### 8. Documentation updated

As above.

### 9. Final status

`complete (approved 2026-09-18)` — the approval covers the corrective pass
recorded below. M23 → M24 follow only after explicit authorization.

## Corrective pass — grounded battle stages (2026-09-16)

Owner brief of 2026-09-16, a correction to this milestone's B3 slice — not
a new milestone; at the time the status stayed `implemented, awaiting manual
approval` (the owner approved M117–M119, this correction included, on
2026-09-18). Everything above is kept as written;
this section is the as-built record of the correction. M117 and M118 were
not touched.

**1. Root cause.** Two things, both in the first cut's rules rather than in
the sprites. (a) Geometry: the band was 426×122 (y 24..146) while the
party's fourth row anchors its feet at y 154 and its MP meter ends at y 165,
and a five-foe field's fifth row reaches y 172 — the lowest row hung below
the painted environment with the band's bottom keyline cutting through its
sprite. (b) The gradient-only corridor: the "float corridor carries only
the base gradient" rule kept x 70..356 × y 32..97 blank, and the painted
floor strip began at band y 74 (screen y 98) — below the feet of rows one
and two (y 52 and 86). The upper rows stood on a wall painting with no
floor under them, and the only horizontal structure under any foot was its
own HP meter, so the eye read the meters as platforms and the actors as
floating.

**2. Battle-stage geometry, before → after.** Band 426×122 at y 24..146 →
**426×150 at y 24..174** (`BattleState::render`: `h - kPanelH - 6 - bandY`;
the panel frame starts at y 176, so a two-pixel canvas seam keeps it
visually separate). The horizon (far/ground transition) sits at band y 12
= screen y 36: the party's feet at 52/86/120/154 and a five-foe field's at
36/70/104/138/172 all lie on or below it; the lowest party meter (y 165)
and the fifth foe's feet (y 172) lie over the ground. The fifth foe's meter
(y 173..178) still dips two pixels under the panel frame exactly as it did
before (the M101 layout, untouched). The formations, the meters, the turn
order and the battle rules did not move.

**3. Art-bible rule changed.** §6's "float/status corridor carries the
base gradient alone" bullet is replaced by the binding three-layer staging
rule: far layer / combat ground / near edge; continuous ground beats
isolated platforms; the action field is quiet, not blank (what may and
what may not sit in it); pixel-authored perspective; the grounding
hierarchy; contact shadows optional, secondary, and not used.

**4. Generator and assertion changed.** `New-Band` / `Clip-Corridor` /
`Assert-CorridorClear` / `SaveBand` → `New-Stage` (per-row far and ground
value ramps), `Clip-Field` (major motifs clipped out of the action field
x 30..408 × y 8..140, mirroring `render::actionField`), `Snapshot` (the
plane reference, taken after the broad ground masses and before the cues)
and `Assert-StageGrounded`, which refuses a stage unless: the far strip
and the ground differ in mean value by ≥ 6; every cue pixel inside the
field stays within 26 luminance of the plane; the cues cover 2–35 % of
the field; no signal colour (cyan, violet, glint, gold, danger, heal,
white1/2) enters it; and the plane stays within 26 luminance of its mean
along every party foot row. Measured on the shipped files — Keep: step
21.9, cues 4.6 %, strongest 20.2; Mine: 22.0 / 7.1 % / 21.5; Forest: 17.5
/ 4.2 % / 21.9; Castle: 17.8 / 4.5 % / 20.7; Goosy: 15.2 / 5.8 % / 10.5.
Two first drafts failed it (a Forest root whose 5 px steps overlapped and
double-blended; a Goosy far pond as light as the bank) and were fixed at
the source. The section is still the file's last and reseeds, so every
earlier PNG stays byte-identical.

**5. Per-theme background changes.** *Keep* — a coursed-masonry far strip;
a broken wall stump with an arrow slit (left) and the tower's edge with
slits (right); one flagstone floor lightening toward the near edge with
courses at growing spacing (12/18/24/30/36 px), staggered joints, two
stepped cracks, rubble stones in the margins; the M56 merlons broken over
the two sprite columns, the rubble piles moved to the margins and the near
strip. *Mine* — the rock ceiling with a quiet crystal drip and a timber
lintel; timber supports and crystal clusters (a facet glint each) in the
margins; a rock floor with two broad ore veins (part of the plane), seams,
then rails and sleepers across the lower third; the M56 clusters moved to
the margins and the near strip. *Forest* — the canopy's underside; trunks
with bark bands (two left, one right) and a firefly each side; leaf-litter
earth with a path widening toward the near edge, faint earth bands,
stepped root arcs, leaf clusters; the M56 trunks now stand in the margins
with a fallen bough at the near edge, the canopy broken over the columns.
*Castle* — a plinth line; pilasters at the outer edges and a pointed
window below each banner; paving whose dais runway widens toward the near
edge (part of the plane, not a ledge), tile courses and joints; the M56
banners hang in the margins and the throne arch sits in the near strip.
*Goosy* — mist over the night-dark far pond; reeds with tufts in the
margins; a muddy bank between that water and the near water's edge, grass
along the far edge, mud bands, puddles that grow toward the near edge with
a ripple each, pebbles and tufts; the M56 cattails in the margins, a low
tussock and the ripple glint at the near edge; nobody stands on open
water. *Plain / fallback* — the procedural plane (`buildGroundPlane`: a
far strip, a horizon line, the ground, seams at 12/17/22/27/32 px; value
steps only) under every stage, and alone in high contrast or when a
texture is missing.

**6. Contact shadows.** Not used. The continuous ground plane solves the
grounding on its own, and the game cannot tell hovering units (geese,
bats, spectres) from grounded ones without new content fields, which the
brief rules out for this purpose.

**7. HP/MP meters.** Unchanged. Before, a meter was the only horizontal
structure under a foot on a flat band, so it read as a platform; after,
the paving, rails, path or bank run behind and around every meter and the
band continues below the lowest one, so the meters read as UI bars resting
on a floor. Re-assessed on `151_stage_keep_patrol.png` and
`31_battle_high_town.png` at 3×: no presentation change was needed, so
none was made.

**8. Files changed.** Source: `src/render/BattleBackdrop.{hpp,cpp}`,
`src/states/BattleState.cpp` (the band height, the accents flag),
`src/capture/CaptureRunner.cpp` (`makePatrolTeam`, scenes 151–155). Tests:
`tests/test_battle_backdrop.cpp` (rewritten to the new contract). Assets:
`tools/asset_gen/generate_textures.ps1` (the battle-stage section),
`assets/textures/backgrounds/battle_{keep,mine,forest,castle,goosy}.png`
(re-authored at 426×150), `assets/credits.md`. Documentation:
`docs/art_bible.md` (§6), `docs/asset_pipeline.md`, `docs/game_design.md`,
`docs/technical_design.md` (the B1 bullet, §60), `docs/manual_test_matrix.md`
(row 241 amended, row 242 added), `docs/milestones.md` (the M119 section
and the program paragraph), `docs/milestone_notes/M56_boss_stagecraft.md`
(the pointer), this note. No manifest, data, schema, save or build change.

**9. Commands run.** In the VS 2022 amd64 dev shell:
`powershell -ExecutionPolicy Bypass -File tools\asset_gen\generate_textures.ps1`
(three runs while iterating, the last one clean); `cmake --build --preset
debug`; `.\build-msvc\crystal_tests.exe "[backdrop]"`;
`.\build-msvc\ArePGeese.exe --capture docs\screenshots\m119c_captures`;
`ctest --preset debug`; `cmake --build --preset release`; `ctest --preset
release`.

**10. Automated results.** the generator ran three times while iterating (a Forest root that
  double-blended and a Goosy far pond as light as the bank were caught by
  `Assert-StageGrounded` and fixed at the source); the final run changed
  exactly the five battle PNGs and nothing else (`git status`: the
  byte-stability proof for every earlier file). Debug build clean (zero
  project-code warnings); `crystal_tests.exe "[backdrop]"` **9/9 cases,
  10,813 assertions**; `ArePGeese.exe --capture docs\screenshots\m119c_captures`
  **155/155 scenes clean**, zero overflow events (five new scenes 151–155);
  `ctest --preset debug` **900/900 passed** (360.10 s); Release build
  clean + `ctest --preset release` **896/896 passed** (171.5 s; the four
  Debug-only cases are compiled out there). Warnings: none in project
  code (both presets). Skipped: nothing; `CrystalForge --canonicalize` was
  not run — no `data/*.json` changed.

**11. Manual capture review** (each at 1× and in 3× nearest-neighbour
crops of the party column, the enemy column and the band's lower edge;
`151_stage_keep_patrol.png` also in grayscale). *Keep* (151, 130): all
four party rows stand on the flagstone courses with the band's edge below
the lowest meters; the three-foe column and the lone Golden Goose stand on
the same paving; the far wall and its stump sit behind the top row's head;
the courses pass behind the numerals at low contrast; in grayscale the
sprites' outlines and bodies are the strongest shapes on screen. *Mine*
(152, 70, 73 in high contrast): the rails and sleepers cross the lower
third behind the third row, the supports and clusters stay in the margins;
under High Contrast the painting is gone and the five foes stand on the
procedural plane with the crystal silhouettes at the margins. *Forest*
(153, 71): the party and the five foes stand on the widening path, the
trunks stay in the margins, the roots and bough at the near edge. *Castle*
(155, 72, 140, 141): the King and his court, a five-foe wave, and the
Dragon with its clone stand on the paving with the dais runway behind them
and the banners in the margins; the 36 px boss footprint and its lifted
upper neighbour both rest on the plane. *Goosy* (120, 121): the pond fowl,
the Pondlord's court and the party stand on the bank with puddles between
the rows, the near water's edge under the band's bottom and the far pond
behind the top row. *Plain* (154, 123, and the stage-less scenes 17, 18,
31, 34; 51 in high contrast): the far strip, the horizon line, the ground
and its seams ground every unit — the five-foe field with statuses, the
Tyrant's court and the King's — with no painting at all. Every row's feet,
every meter, the floor transition and the lower edge were checked in each;
nothing needed a circle or platform.

**12. Remaining visual weaknesses.** (a) A boss court of four or five
minions (the Rush Tyrant, the Abyssal Tyrant, the Deadly Duck) lifts its
top row to y 10..26 for the boss's crown (M101/M115), so that far row's
feet sit at y 26, ten pixels above the horizon — it stands at the wall's
base rather than on the floor; moving the band top or the formation was
outside this brief. (b) The fifth foe's meter still ends two pixels under
the panel frame (pre-existing). (c) Top-row sprites' heads rise above the
band's top keyline into the canvas, as they always have. (d) The Keep and
Castle joints read slightly brick-like in the open middle at 1×; the
growing course spacing is what keeps them a floor. (e) The perspective
courses cross the party numerals and the enemy status column as 1 px
low-contrast lines; both text roles remain unbacked, as before. (f) A
five-foe boss court's sixth row (the summon clone) continues under the
panel, as M75 documented.

**13. Acceptance criteria.** All items of the brief's §12 are met on the
captures reviewed, with (a) above as the one geometry the horizon cannot
reach; the visual judgement itself is the owner's (matrix rows 241–242) —
given with the approval of 2026-09-18.
