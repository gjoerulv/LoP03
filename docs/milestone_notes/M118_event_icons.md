# M118 — Event icons

**Status:** implemented, awaiting manual approval (implemented 2026-09-14)
**Program:** M117–M119 (owner-authorized 2026-09-14 via the approved plan;
branch `oyb11`, baseline `72f4c3e`).
**No version motion:** manifest v2 with fifteen new texture ids; content,
rules, generation and saves untouched.

## Scope (plan section M118; the owner's item 5)

"Each event should have its own icon. As-is, they are rather inconspicuous."
The audit behind the plan found the cause: of the 22 event kinds only seven
had a `prop.event.*` sprite, six drew a colour box with a letter, and nine
(goose polymorph, sacrifice, level altar, Stranger story, token exchange,
patrol reset, reels, blackjack, goosy flock) had no case in the marker
switch at all — a zero-alpha box under a near-black "?". Owner decisions:
one bespoke icon per kind, a subtle glint on unresolved events, the icon in
the flavor panel.

## What was built

- **The table** (`src/dungeon/ThemeEvents.hpp`): `eventMarkerSpriteId(kind)`
  beside `eventFlavorId` — the seven shipped ids keep their names, the
  fifteen new kinds map to `prop.event.<flavor id>`; `kAllRoomEventKinds`
  (22, enum order) is the shared list. `static_assert`s bind it to the enum's
  last member and to the flavor vocabulary.
- **The marker** (`DungeonState` render loop): the sprite id comes from the
  table; the colour + glyph switch covers every kind and is only the
  missing-texture fallback; an unresolved event draws a crystal pip
  (`ui::drawCrystalPip`, the shared two-frame motion glint) centred one pixel
  above the icon. Markers exist only while unresolved, so the pip leaves with
  them. The M80 flavor panel draws the icon at a fixed inset left of its
  centred title.
- **Art**: fifteen hand-placed 12×12 props in a RNG-free section appended
  last in `tools/asset_gen/generate_textures.ps1` (the M20 idiom: `FR`/`P` +
  `Outline`): a spectral helm (armory ghost), an ore sack with a pick
  (miner's cache), a root knot with a sprout (elder root), a hooded peddler
  with a sickly duck aboard (duck peddler), a parchment with a red pin
  (surveyor), a scaled dragon snout with horns and a slit eye (dragonform),
  a goose with a wand spark (goose polymorph), an anvil with a broken blade
  (sacrifice), a stepped altar with a rising spark (level altar), the hooded
  Stranger (Stranger story), a balance with a gold coin and a violet token
  (token exchange), an hourglass (patrol reset), a three-window machine with
  a lever (reels), two fanned cards (blackjack), five geese in a V (goosy
  flock). Manifest rows, the credits row amended in place. Byte-stability:
  the section is RNG-free and sits after the last reseed — a full generator
  re-run adds exactly the fifteen PNGs.
- **Review**: `tools/asset_gen/preview_events.ps1` (cloned from the M81 gear
  icon sheet: props directory, `event_*.png`, 12×12 cells, dark + light
  rows → `docs/sprite_review/events_contact.png`), read back as an image
  before acceptance.
- **Specimen capture** `150_event_icons`: `EventMarkerSpecimenState`
  (capture-only, the M114 `FontSpecimenState` precedent) — two columns of
  eleven, icon at 1× and flavor title; a missing icon is a loud red block.
- **Tests**: `tests/test_event_marker.cpp` (new, `[events][marker][lint]`):
  every kind maps to a distinct `prop.event.*` id present in the shipped
  manifest as a texture, None → null, the seven legacy names, the fifteen
  flavor-derived ids; the flavor lockstep test asserts the shared list
  matches its own kind for kind; the presentation lint walks the table.

## Deviations from the plan

None.

## Compatibility

- Save / settings / content / seeds / scores: no impact.
- Packaged assets: fifteen new PNGs under `assets/textures/props/`, fifteen
  manifest ids; a missing texture degrades to the coloured box.

## Automated validation

See the completion report below.

## Manual owner checklist

Matrix rows **237–238**: every icon in a real room (and the nine formerly
invisible kinds), the flavor panel's icon, the glint's restraint.

## Known limitations

- `ui::motionPhase()` reads the live clock (the title pulse, the menu
  chevrons and the M56 backdrop glint share it), so the pip's frame in a
  capture is whichever the clock gives — the accepted, pre-existing
  property; no test byte-diffs capture PNGs.

## Documentation updated

`docs/milestones.md` (row 118 + section), `docs/art_bible.md` (§3 markers),
`docs/asset_pipeline.md` (the event-marker paragraph),
`docs/technical_design.md` (the M55 marker sentence, §59),
`docs/manual_test_matrix.md` (rows 237–238), `assets/credits.md`,
`docs/milestone_notes/M55_theme_rites.md` / `M103_six_events.md` /
`M104_gambling_den.md` (pointers), this note.

## Completion report

### 1. Implementation summary

- **Milestone:** M118 — Event icons.
- **Slices completed:** the table + shared list; the marker (every kind,
  the pip); the panel icon; fifteen sprites + manifest + credits; the review
  tool; the specimen capture; the tests; the docs. All completed.
- **Player-facing changes:** every dungeon event shows its own icon with a
  glint while unresolved; the flavor panel shows the icon.
- **Engineering changes:** `dungeon::eventMarkerSpriteId`,
  `dungeon::kAllRoomEventKinds`, `EventMarkerSpecimenState`, capture scene
  150, `preview_events.ps1`.

### 2. Files changed

- **Source:** `src/dungeon/ThemeEvents.hpp`, `src/states/DungeonState.cpp`,
  `src/states/EventMarkerSpecimenState.{hpp,cpp}` (new),
  `src/capture/CaptureRunner.cpp`, `CMakeLists.txt`.
- **Tests:** `tests/test_event_marker.cpp` (new), `tests/test_event_flavor.cpp`,
  `tests/test_presentation_lint.cpp`, `tests/CMakeLists.txt`.
- **Content/data:** `assets/manifest.json` (+15), `assets/credits.md`,
  `assets/textures/props/event_*.png` (15 new),
  `tools/asset_gen/generate_textures.ps1`, `tools/asset_gen/preview_events.ps1`
  (new), `docs/sprite_review/events_contact.png` (review artefact).
- **Documentation:** as listed above.
- **Build/release configuration:** `CMakeLists.txt` (one source),
  `tests/CMakeLists.txt` (one test file).

### 3. Plan deviations

None.

### 4. Compatibility

As above: manifest ids only.

### 5. Automated validation

- **Build command:** `cmake --build --preset debug` (VS 2022 amd64 dev shell).
- **Test command:** `.\build-msvc\crystal_tests.exe "[events],[lint],[flavor]"`,
  then the program's closing battery.
- **Results:** the generator re-run wrote exactly the fifteen new PNGs and
  changed nothing else (`git status`: the byte-stability proof); Debug build
  clean (the specimen's first compile missed the resource-manager include —
  fixed at once); the targeted tags `[events],[lint],[flavor],[marker]`
  **29/29 cases, 110,245 assertions**; `ArePGeese.exe --capture
  docs\screenshots\m118_captures` **150/150 scenes clean**, zero overflow
  events (new `150_event_icons` — all 22 icons beside their titles;
  `86_event_flavor` shows the panel icon; the rite rooms show their icons and
  the pip); `docs/sprite_review/events_contact.png` inspected at 8× on the
  dark and light rows — every silhouette distinct from every other. The full
  Debug and Release suites ride the program's closing battery (see the M119
  note's report and the ledger's program section).
- **Warnings:** none in project code.
- **Skipped validation and reason:** the full `ctest` suites and the Release
  build run once at the program close-out, not per milestone (the M109–M116
  precedent); `CrystalForge --canonicalize` was not run — no `data/*.json`
  changed.

### 6. Manual owner validation

Matrix rows 237–238.

### 7. Known limitations

As above.

### 8. Documentation updated

As above.

### 9. Final status

`implemented, awaiting manual approval`

Then stop and wait for the owner's manual acceptance decision (the program
continues with M119 first, as authorized).
