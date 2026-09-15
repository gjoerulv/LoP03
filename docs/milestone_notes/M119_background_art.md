# M119 — Background art

**Status:** implemented, awaiting manual approval (implemented 2026-09-14)
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
  rect geometry and its five tests are untouched.
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
  the id/manifest pins and matrix row 241 cover it.
- The interiors use the pre-hoisted M113 helpers rather than a second copy —
  a function definition draws nothing, so no earlier bytes moved.

## Compatibility

- Save / settings / content / seeds / scores: no impact.
- Packaged assets: 42 interior PNGs replaced, six PNGs added, six manifest
  ids; a missing background degrades to the flat fill (interiors, title) or
  the M56 look (battle stages).

## Automated validation

See the completion report below.

## Manual owner checklist

Matrix rows **239–241**: the interiors in a low and a high town (restraint
and caption legibility), the title scene, the battle stages per theme and
in the castle, and the High Contrast fallback.

## Known limitations

- The painted battle layer is authored against the band's fixed 426×122
  geometry; a future change to `kPanelH` or the band's y would need the
  textures re-authored (the generator's constants sit beside the M56 ones).

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

`implemented, awaiting manual approval`

Then stop and wait for the owner's manual acceptance decision. M23 → M24
follow only after explicit authorization.
