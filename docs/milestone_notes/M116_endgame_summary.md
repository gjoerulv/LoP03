# M116 — The End-game Summary

**Status:** implemented, awaiting manual approval
**Program:** M109–M116 (owner-authorized 2026-09-02 via the approved plan).
**Save schema stays v1:** `Party.summaryShown` is an additive optional bool
(old saves → false). No other version motion.

## Scope (plan section M116; owner sections 23–27)

A tabbed End-game Summary that unlocks only once the finale's keepsake
choice is recorded, shows itself once automatically, and is revisitable
through THE STRANGER "P" (Talk / End-game Summary); the victory stinger
then the Result loop on its first showing; six pages — Overview, Heroes,
Combat, World, Bestiary, Patrols — with nothing crammed; the game never
ends and the counters keep updating.

## What was built

- **Pure side** (`src/game/Summary.hpp`): `summaryUnlocked` (the finale's
  recorded choice — never the King's fall alone), `shouldAutoShowSummary`,
  `SummaryPage` / `summaryPageAt` / `summaryPageName`, `SummaryRow`,
  `summaryRows(page, party, db)` for all six pages from the M109 ledger
  (Heroes reads the live name, class, level and `allKnownSkills`; the
  Bestiary page walks the BestiaryState order and marks nothing), the
  migration note as a leading header row on the Overview, `formatCount`
  (thousands grouping), `formatPlayTime` (`Hh MMm`).
- **`EndgameSummaryState`**: a header band, the page name with `n/6`, an
  Inset list of `drawMenuScrolled` rows (label + right-aligned gold value
  at the body size, headers as disabled rows), thirteen visible rows at a
  13 px pitch, CyclePrev/CycleNext page cycling (the scoreboard idiom),
  Up/Down scrolling, Cancel; the footer hints inside the budget; the play
  clock pauses on it; `onEnter` plays `setMusicThen(Victory, Result)` on
  the first showing and `setMusic(Result)` on revisits.
- **Unlock flow** (`TownState::onResume`, before `pushAchievementToasts`):
  once `shouldAutoShowSummary`, `summaryShown = true` and the state is
  pushed with `firstShow = true`. **Revisit**: the roadside block, once
  the finale is done, pushes a two-row `EventChoiceState` ("Talk" /
  "End-game Summary"; its Cancel steps away with nothing spent); Talk is
  the extracted `playNextStrangerBeat()` (the M100 joke cycle, verbatim —
  `strangerJokesTold` untouched), the summary opens with `firstShow =
  false`.
- **Audio** (`AudioManager::setMusicThen`, `pendingAfterJingle_`): a
  non-jingle first argument plays the loop outright; headless records the
  loop; the stinger path (no jingle file) starts the loop at once; else
  the finished-jingle branch of `update()` starts the pending loop; a
  plain `setMusic()` clears any pending chain.
- **Save** (`SaveSystem`): `summaryShown` written and read as an optional
  bool.
- **Captures:** `142_summary_overview` … `147_summary_patrols` (every
  field at seven digits, the longest names, every boss known, the
  migration note) and `148_stranger_choice`.

## Deviations from the plan

- The list keeps the label at the body size and the value at the body
  size too (the plan left the pitch to be pinned once the frame inset was
  chosen: 13 px, thirteen rows).
- The header band carries no gold badge; the page ordinal sits in the
  page line beneath it.

## Tests

`tests/test_summary.cpp` [summary]: the unlock predicate (a seen finale
is not enough; the recorded choice is), auto-show once (the flag), the
flag surviving a save/load and a fresh party unshown, the formatters, the
page cycle wrapping both ways with six distinct names, every page's rows
pure and inside the label/value budgets at maximal content (seven-digit
counts, the longest name; the Overview naming the biggest hitter; the
migration note only for a migrated ledger; Heroes = four headers + 4×13
rows), the Bestiary page never marking an unmet foe (exact known/unknown
split, the counts), and the jingle-then-loop chain headless (the chain
lands on its loop; a plain `setMusic` clears it; a loop as the "jingle"
plays the next track outright).

## Compatibility

Save v1 with one additive optional field; nothing else moves.

## Automated validation

See the completion report below.

## Manual owner checklist

Matrix rows **229–231**: the once-only auto-show after the finale (the
stinger then the loop, a toast dismissing first, a reload never
re-showing), P's Talk / End-game Summary pick, the six pages (names, n/6,
wrapping, scrolling, grouping, the live Heroes rows, the Bestiary
matching the Bestiary screen and marking nothing), the counters moving
between visits, and a pre-ledger save's migration note. **Judge whether
the summary is fun and whether anything is missing.**

## Documentation updated

`docs/milestones.md` (rows + section + the program close), `docs/game_design.md`
(the end-game summary passage), `docs/technical_design.md` (§57),
`docs/control_standard.md` (the cycle pair's uses), `docs/asset_pipeline.md`
(the jingle chain), `docs/manual_test_matrix.md` (rows 229–231), this note.

## Completion report

### 1. Implementation summary

- **Milestone:** M116 — The End-game Summary.
- **Slices completed:** the pure rows and predicates; the state; the
  unlock and revisit flows; the audio chain; the save field; captures;
  tests; docs. All completed.
- **Player-facing changes:** the summary after the finale; P's two-way
  pick at the roadside.
- **Engineering changes:** `game/Summary.hpp` (new),
  `states/EndgameSummaryState` (new), `TownState::playNextStrangerBeat`,
  `AudioManager::setMusicThen`, `Party.summaryShown`.

### 2. Files changed

- **Source:** `src/game/Summary.hpp` (new), `src/states/EndgameSummaryState.{hpp,cpp}`
  (new), `src/states/{TownState.hpp,TownState.cpp}`, `src/audio/{AudioManager.hpp,
  AudioManager.cpp}`, `src/game/Party.hpp`, `src/save/SaveSystem.cpp`,
  `src/capture/CaptureRunner.cpp`, `CMakeLists.txt`.
- **Tests:** `tests/test_summary.cpp` (new), `tests/CMakeLists.txt`.
- **Content/data:** none.
- **Documentation:** as listed above.
- **Build/release configuration:** `CMakeLists.txt` (one new source).

### 3. Plan deviations

See "Deviations from the plan" — routine.

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

- **Build:** `cmake --build --preset debug` — clean, zero project-code
  warnings (VS 2022 developer shell).
- **Targeted tests:** `crystal_tests.exe "[summary],[save],[audio],[lint],
  [cutscene],[town]"` — **74 cases, 86 463 assertions green** (the seven
  new [summary] cases among them).
- **Capture lint:** `ArePGeese.exe --capture <dir>` — exit 0, **148/148
  scenes clean** (`142`–`148` new; the six pages at seven-digit counts and
  the longest names inspected).
- **Full suite / Release:** the program's closing battery (recorded in
  `docs/milestones.md` under the program section).

### 6. Manual owner validation

Matrix rows 229–231.

### 7. Known limitations

- The summary's "fun" is the owner's judgment; the rows are complete by
  the ledger's definition, and a missing statistic is a ledger addition
  first.

### 8. Documentation updated

As listed above.

### 9. Final status

`implemented, awaiting manual approval`
