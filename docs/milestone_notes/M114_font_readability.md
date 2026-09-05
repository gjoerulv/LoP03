# M114 — Font readability redesign

**Status:** implemented, awaiting manual approval
**Program:** M109–M116 (owner-authorized 2026-09-02 via the approved plan).
**No version motion:** rules 19, generation 24, saves v1, content v1,
manifest v2 (the font ids and paths are unchanged; the files' contents
are new).

## Scope (plan section M114; owner section 18)

The original bitmap typeface, redrawn for readability on a **≈6×9 master
cell** with a **9 px floor for meaningful text**, the 161-glyph Latin
coverage kept, no second font, overflow answered by reflow (never by
shrinking), a capture-only specimen under the overflow lint.

## What was built

- **The alphabet** (`tools/asset_gen/generate_font.ps1`, every glyph a
  hand-placed grid emitted by hand-authored rows — nothing traced or
  rasterized): a 9-row cell — cap height 7 (rows 0–6), x-height 5
  (rows 2–6), two descender rows (7–8), accents in rows 0–1 (the M87
  convention; accented capitals keep 5-row compressed bodies). Capitals
  and digits 5 wide; most lowercase 4 wide (m, v, w 5); i, l, j, t, f
  narrow. The owner's forms: a two-storey `a`, an open `c` beside a closed
  `e`, distinct `r`/`n`/`m` arches, a pointed `v` beside a round `u`, a
  serifed `I`, a tailed `l`, a flagged `1` with a base, a dotted `0` beside
  `O`, a barred `G`, cleaner punctuation, paired quotes, a comma and a
  semicolon that descend, `¿` and `¡` that hang below the line, a cedilla
  in the descender rows (no more compressed `ç`/`Ç`). The 161 codepoints
  are unchanged (`GlyphCoverage.hpp` untouched; `test_glyph_coverage`
  green).
- **The descriptors:** `font_small.fnt` lineHeight **9** (base 7),
  `font_main.fnt` 10 (base 7), `font_title.fnt` 20 (base 14, the 2× atlas)
  — 9, 10 and 20 all render 1:1; `activeFont` (≤ 9 → small, ≤ 15 → main,
  else title) is unchanged. Wrapped text keeps `lineHeight(size) = size
  + 3` (body 10 → 13 px pitch against a 9-row cell: 4 px clear).
- **Size policy:** `style::kFontSmall` 8 → **9**; every literal 8 and 9
  font size in the states migrated to the constant (36 sites across
  BattleState, CastleState, CelebrationState, DungeonState,
  GooseTownState, HelpState, MapsState, PartyState, ScoreboardState,
  TownState — HP/MP numerals, the KO tag, the status column at a 9 px
  step, the spoils lines, the dungeon footer prompt and minimap chips,
  town labels and messages, the party sheet's stats/gear/passives/
  milestones/skills, the Maps hints, the scoreboard chip). **No 8 px text
  survives**: the small base is 9 rows, so an 8 would render at 0.89 —
  worse than the floor. Town labels moved up one pixel to keep their
  clearance over the sprites.
- **The specimen** (`FontSpecimenState`, capture-only, body under
  `CRYSTAL_CAPTURE`; scene `139_font_specimen`): the confusable groups,
  paired quotes and punctuation, the pangrams, the accented sets and the
  M87 Latin pangram at 9 / 10 / 20, every line through a fitted draw so
  the lint referees it.
- **Reflow pass:** `--capture` under the new font — **139/139 scenes
  clean, zero overflow events**. The redesign's lowercase averages 4.9 px
  of advance (the old 5.7) with capitals unchanged at 5.9, so no site
  grew wider; the taller cell and the 9 px captions cost height only, and
  every fixed pitch (the battle status column at 9 px, the party sheet's
  rows, the HUD numerals) was re-checked in the captures. No budget
  needed widening; no text was shrunk.

## Deviations from the plan

- The plan estimated a +15 % width from a 6-wide cell and budgeted a
  reflow of the footer strip, the scoreboard columns and the party sheet.
  The authored alphabet keeps most lowercase 4 wide (readability came from
  the taller cell, the descenders and the letterforms, not from width),
  so the capture lint found nothing to reflow. The scoreboard's
  hard-coded column edges and the party sheet's column math are untouched
  and verified by the captures.
- The M87 accented-capital convention (compressed 5-row bodies under
  the accent) is kept as the plan asked; the cedilla moved into the
  descender rows because they now exist.

## Fixes during manual testing

- **2026-09-03 — the period read as a colon.** The `.` grid carried a
  stray ink pixel in row 0 above its baseline dot (row 6), so the glyph
  rendered as two stacked dots at every size. Fixed at the source
  (`generate_font.ps1`, glyph 46: row 0 cleared; the only glyph with an
  unpaired top-row pixel — the dots on `i` and `j` are intended), the font
  regenerated (byte-stable on a second run; the `.fnt` descriptors are
  unchanged since the advance stayed 1 px), `[glyphs],[lint]` green (16
  cases), `--capture` 148/148 clean, and the specimen re-inspected: a
  single baseline dot at 9 / 10 / 20. Note for anyone re-running the
  capture after a generator change: the executable loads the
  `build-msvc/assets` copy made by the POST_BUILD step, which only
  refreshes on a relink — copy `assets/fonts/*` beside the binary (or
  rebuild) before capturing, or the lint runs against the stale atlas.

## Tests

`test_glyph_coverage` (the 161 codepoints against both `.fnt` files and
all shipped text), `test_text_layout` (the wrap and pitch pins,
unchanged), the `[ui]`/`[lint]`/`[content]` suites — all green. The
generator re-run twice produces byte-identical files (deterministic by
construction: pure `SetPixel` over the grids).

## Compatibility

No version motion. The font files change bytes; ids and paths do not.

## Automated validation

See the completion report below.

## Manual owner checklist

Matrix rows **225–226**: readability at 1× and under CRT across the
caption-heavy screens (town labels, the battle HUD, the party sheet, the
scoreboard, dungeon footers, shops, Help, a cutscene); the confusable
groups tell apart; nothing 8 px remains; nothing overlaps; nothing new
is clipped. **Report any screen that reads worse than before.**

## Documentation updated

`docs/milestones.md` (rows + section), `docs/ui_style_guide.md` (§2 the
font paragraph, the role table, the rules), `docs/asset_pipeline.md`
(the font entry), `docs/art_bible.md` (§3 type), the M87 note (a
superseded-in-part pointer), `docs/manual_test_matrix.md` (rows
225–226), this note.

## Completion report

### 1. Implementation summary

- **Milestone:** M114 — Font readability redesign.
- **Slices completed:** the alphabet; the descriptors; the size policy and
  the migration; the specimen; the reflow pass; tests; docs. All
  completed.
- **Player-facing changes:** every piece of text in the game is the
  redrawn typeface; captions are 9 px.
- **Engineering changes:** `generate_font.ps1` (9-row cell, new data),
  `style::kFontSmall = 9`, 36 migrated sites, `FontSpecimenState` (new).

### 2. Files changed

- **Source:** `src/ui/UiStyle.hpp`, `src/states/{BattleState,CastleState,
  CelebrationState,DungeonState,GooseTownState,HelpState,MapsState,
  PartyState,ScoreboardState,TownState}.cpp`,
  `src/states/FontSpecimenState.{hpp,cpp}` (new),
  `src/capture/CaptureRunner.cpp`, `CMakeLists.txt`.
- **Tests:** none added (the existing pins bind; the capture is the lint).
- **Content/data:** `assets/fonts/{font_atlas.png,font_atlas_2x.png,
  font_small.fnt,font_main.fnt,font_title.fnt}` (regenerated),
  `tools/asset_gen/generate_font.ps1`.
- **Documentation:** as listed above.
- **Build/release configuration:** `CMakeLists.txt` (one new source).

### 3. Plan deviations

See "Deviations from the plan".

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

- **Build:** `cmake --build --preset debug` — clean, zero project-code
  warnings (VS 2022 developer shell).
- **Font generation:** `generate_font.ps1` — 161 glyphs, atlas 832×9;
  `git status` shows the five font files changed and nothing else.
- **Targeted tests:** `crystal_tests.exe "[glyphs],[layout],[text],[ui],
  [lint],[content]"` — **73 cases, 93 149 assertions green** (the 161
  codepoints still bound to both `.fnt` files and all shipped text; the
  wrap and pitch pins unchanged).
- **Capture lint:** `ArePGeese.exe --capture <dir>` — exit 0, **139/139
  scenes clean, zero `[ui-overflow]` events** under the new font
  (`139_font_specimen` new). The party sheet, scoreboard, town, dungeon
  HUD, battle targeting and spoils captures were inspected for vertical
  collisions at the 9 px captions: none.
- **Full suite / Release:** ride the program's closing battery.
- **Manual-test fix (2026-09-03):** see *Fixes during manual testing* —
  `[glyphs],[lint]` 16 cases green, `--capture` 148/148 clean after the
  period fix.

### 6. Manual owner validation

Matrix rows 225–226.

### 7. Known limitations

- Readability is the owner's judgment; the specimen and the captures are
  the evidence, not the verdict.

### 8. Documentation updated

As listed above.

### 9. Final status

`implemented, awaiting manual approval`
