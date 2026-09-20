# M124 — The Iron Man's fall: send-off scene, summary & the Hall of Shame

**Status:** complete (approved 2026-09-20)
**Program:** M120–M125 (owner-authorized 2026-09-18 via the approved plan;
branch `oyb12`, baseline `d789f31`).
**No version motion:** rules, generation, save, settings, content and manifest
versions unchanged; no schema change; no new asset (existing sprites,
procedural motion). One new user-data file, `fallen_runs.json` (its own
version 1).

## Scope (plan section M124; the owner's ruling of 2026-09-18)

When an Iron Man run ends: "a humiliating animation (with a random punchline)
of the party getting beat up by geese and ducks. The king laughs in the
background. All this is similar to the victory screen after a dungeon run. An
end game summary is shown, then back to title screen. A permadeath should be
recorded and inspected in the title screen with run details, in a similar
manner as end-game summary are shown. This summary should also include where
the team was beaten, and by who."

## What was built

### The send-off (`FallenState`, `FallenPhrases.hpp`)

- The `CelebrationState`'s unkind twin, reached through M123's one door
  (`beginIronManFall`). A "Defeat!" plaque; **Fell at** and **Beaten by**
  lines; one dry line drawn from `kFallenPhrases` (16 original lines, the
  TitlePhrases / CelebrationPhrases idiom — `GetRandomValue`, so captures
  stay reproducible).
- The party lies where it fell (the celebration's KO pose), fanned either
  side of the **Hollow King**, who stands in the back shaking with laughter
  while a gold "Ha!" / "Ha ha!" pops above his crown. Four tormentors — two
  Evil Geese, a Pond Drake and a Mallard Marauder — hop on the members, each
  to its own beat; every landing squashes and flashes the member and throws
  two small stars. Three more waterfowl jog a lap of honour across the
  foreground (one of them the Golden Goose), and plucked feathers drift down
  in place of confetti.
- Presentation only: existing sprites, every position a pure function of the
  state's own clock; it reads the party and writes nothing. The defeat music
  plays. Confirm (or Cancel) moves on to the summary.

### The fallen run's summary (`EndgameSummaryState`, `game/Summary.hpp`)

- `EndgameSummaryState` gained a second, **fallen form**: the same six pages
  built by `summaryRows`, but over a party **snapshot** instead of the live
  party. The header band reads "Fallen Run" (danger accent), the defeat music
  plays instead of the fanfare, and the Overview leads with
  `fallenSummaryRows`: **Fell at** / the place, **Beaten by** / the foes (each
  on its own full-width row — a place line is far wider than the value
  column), **Mode: Iron Man**, then the lifetime rows.
- From the send-off, leaving (Cancel **or** Confirm; the hint reads "To the
  title") starts the title over. From the Hall of Shame it simply goes back.

### The record (`game/FallenRuns.{hpp,cpp}`, `fallen_runs.json`)

- `FallenRun{place, foes, leader, highestLevel, playSeconds, party}`. `party`
  is the run's party in **exactly the slot codec's format**. For that the
  codec was lifted out of the slot I/O: `SaveSystem::serialize(party)` is the
  text `save` writes and `SaveSystem::parseText` the validation `load`
  applies (behavior-neutral — a test compares `serialize` with the file
  `save` wrote). `serialize` has no Iron Man refusal; that stays `save`'s.
- The store follows the AchievementStore pattern: versioned JSON in the
  user-data directory, atomic write, **newest 20 kept**, a missing file is an
  empty hall, malformed or foreign-version text is an empty hall plus a log
  line, one bad record is skipped and the rest kept, a snapshot that is not a
  JSON object is dropped rather than poisoning the file.
- `beginIronManFall` writes the record **once, before anything is shown**, so
  quitting during the send-off cannot lose it. `AppContext::fallenRuns` is
  the new store reference; `Application` loads it at start-up.

### The Hall of Shame (`HallOfShameState`, the title menu)

- The title gains a **Hall of Shame** row between Continue and Credits —
  **only while records exist** (six rows tighten the pitch to 17 px and lift
  the frame 5 px so it still ends above the footer rows; five rows are
  unchanged). Title rows are now action ids, not fixed indices.
- The list: newest first, six rows a window with an `n / total` chip beyond
  that. A row shows "<Leader>'s party - Lv.N", the save slots' play-time
  clock (greyed hour digits and all), and a small where/who line (policy E:
  ellipsized, the full text being one Confirm away). Confirm parses the
  snapshot and opens the fallen summary; a snapshot that can no longer be
  read (newer build, changed content) keeps its row and answers "This run's
  details can no longer be read." Read-only: nothing can be continued,
  deleted or changed.

## Deviations from the plan

- M123's plain notice (`IronManFallState`) is **removed**, not kept beside
  the scene — the send-off replaces it, as M123's note announced.
- The plan's list columns were "party, place, foes, play time"; place and
  foes share one small line so a row stays two lines tall.
- No deletion UI for records (not asked for; the cap of 20 bounds the file).

## Tests

- `tests/test_fallen_runs.cpp` (new, `[m124]`, 8 cases): the codec round
  trip with a party snapshot (and that the very same party is still refused
  by `save`); `serialize` == the file `save` wrote, `parseText` == `load`, a
  failed parse never touches the target; the newest-20 cap in memory and on
  load; malformed / wrong-shape / foreign-version text; bad records skipped
  and non-object snapshots dropped; the store (missing file, record, reload,
  recovery from a corrupt file); the fallen summary's lead rows; the phrase
  pool (non-empty, ASCII, genre-free, fits the screen).
- The `[save]` suites pin the codec extraction.
- Captures: `174_iron_man_fall` (now the send-off, clock pinned at 2.35 s),
  `175_fallen_run_summary`, `176_hall_of_shame`, `177_title_hall_of_shame`.

## Compatibility

- **Saves / settings / scores / seeds / rules / schemas:** untouched. Slot
  files are byte-identical (the codec moved, it did not change).
- **New file:** `fallen_runs.json` in the user-data directory. Deleting it
  empties the hall and hides the title row; nothing else reads it.
- `AppContext` gained one member (`fallenRuns`) — both construction sites
  (the application and the capture runner) were updated.

## Known limitations

- The scene uses the battle sprites as they are: no bespoke "pecking" frames,
  no sound effects beyond the defeat music. Its humour, pacing and
  readability are the owner's to judge.
- Records carry no date (nothing else in the game reads the wall clock).
- A record whose snapshot no longer validates shows its row but not its
  pages.
- Quitting the game mid-run is still not recorded as a fall (M123).

## Documentation updated

`docs/milestones.md`, this note, `docs/game_design.md` (§12 Iron Man: the
send-off and the Hall of Shame), `docs/technical_design.md` (§65, the title
flow line, the live scene count), `docs/control_standard.md`,
`docs/manual_test_matrix.md` (rows 263–266, row 261 re-pointed at the
send-off), `docs/playtest_protocol.md` (the clean-slate file list),
`README.md`, a pointer in `M123_playtime_iron_man.md`, a pointer
in `M116`'s note for the summary's second form.

## Completion report

### 1. Implementation summary

**M124 — The Iron Man's fall.** Complete. A wiped Iron Man run is recorded,
sent off with the geese-and-ducks scene under the laughing King, summarized
with where it fell and to whom, and returned to the title — where the Hall of
Shame lists every such run and reopens its summary.

### 2. Files changed

- **Source (new):** `src/game/FallenRuns.{hpp,cpp}`,
  `src/states/FallenState.{hpp,cpp}`, `src/states/FallenPhrases.hpp`,
  `src/states/HallOfShameState.{hpp,cpp}`.
- **Source (changed):** `src/save/SaveSystem.{hpp,cpp}`,
  `src/game/Summary.hpp`, `src/states/EndgameSummaryState.{hpp,cpp}`,
  `src/states/IronManFall.{hpp,cpp}`, `src/states/MainMenuState.{hpp,cpp}`,
  `src/core/AppContext.hpp`, `src/core/Application.{hpp,cpp}`,
  `src/capture/CaptureRunner.cpp`, `CMakeLists.txt`.
- **Tests:** `tests/test_fallen_runs.cpp` (new), `tests/CMakeLists.txt`.
- **Docs:** see "Documentation updated".

### 3. Plan deviations

See "Deviations from the plan".

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

All run 2026-09-19 from the VS 2022 developer shell (amd64):

- `cmake --build --preset debug` - **succeeded** (game, CrystalForge, tests).
- `crystal_tests.exe "[m124],[m123],[save],[summary],[options]"` - **60 test
  cases, 2823 assertions, all passed** (the `[save]` tags pin the
  behavior-neutral codec extraction).
- `ArePGeese.exe --capture <dir>` - **177/177 scenes clean**. The first run
  caught a real overflow (the Hall of Shame's where/who line, 483 px in a
  334 px row); it became a policy-E ellipsized line, the specimen kept. All
  four new scenes were read back by eye (the send-off at 3x); two polish
  passes followed (brighter fallen sprites; the six-row title frame lifted
  clear of the footer rows).
- `cmake --build --preset release` - **succeeded** (the first attempt failed
  linking `CrystalForge.exe` at the manifest step - `mt.exe` could not open
  the output file, a transient lock; the immediate retry linked cleanly with
  no source change).
- `ctest --preset debug` - **944/944 passed** (2757 s).
- `ctest --preset release` - **940/940 passed** (669 s).

No data file changed in this milestone, so no canonicalize run was needed.

### 6. Manual owner validation

Matrix rows **263–266** (and the re-pointed **261**): whether the send-off is
funny and readable (the pacing of the hops, the King's laugh, the punchlines);
that the summary shows where and who and leads to the title; that the record
appears in the Hall of Shame with the right clock, level, place and foes, and
reopens the same pages; that the title row appears only once somebody has
fallen and the six-row menu still reads well.

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`complete (approved 2026-09-20)`
