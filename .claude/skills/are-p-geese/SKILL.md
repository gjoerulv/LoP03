---
name: are-p-geese
description: Operational workflow for building "Are P Geese" (formerly Crystal Dungeons), a 16-bit-inspired turn-based JRPG roguelite in C++20 / raylib / CMake (MSVC). Use when implementing any milestone, building, testing, validating, or making design/architecture decisions for this project.
---

# Are P Geese — Build Workflow Skill

The **repeatable workflow helper**, not the contract. `CLAUDE.md` at the repo
root is the contract and wins every disagreement. This file carries ONLY what
no other document owns: operational cribs and hard-won gotchas. It must never
restate status, rules, or numbers that live elsewhere — every restated fact
here has eventually drifted and lied.

## Where truth lives (pointers, not copies)

- Session start, document authority, milestone workflow, escalation, Git
  rules, honesty rules: `CLAUDE.md` (read it first, follow its order).
- Milestone status: `docs/milestones.md` — the ONLY status source. Never
  trust or write a status restatement, including in this file.
- Build/run/test commands and dependency pins: `README.md` +
  `cmake/Dependencies.cmake`. Version constants: the history comments in
  `src/dungeon/RoomLayout.hpp` (generation) and `src/battle/Battle.hpp`
  (battle rules) are the authorities — never quote their values in docs.
- Game behavior: `docs/game_design.md`. Architecture:
  `docs/technical_design.md`. UI contract: `docs/ui_style_guide.md`.
  Completion report shape: `docs/milestone_completion_template.md`.
- Cross-session working memory (fix-round history, owner decisions):
  Claude's memory directory — check `MEMORY.md` before re-deriving history.

## Command crib (only what README does not carry)

Every build/test runs from a **VS 2022 developer shell** (README §Build & run
has the entry points; MinGW/GCC unsupported). Beyond README:

```powershell
.\build-msvc\ArePGeese.exe --capture <outdir>   # all scenes, native 426x240; nonzero exit on ANY text overflow
.\build-msvc\crystal_tests.exe "[tag]"          # targeted tags BEFORE the full suite (full ctest ~4-8 min; background it)
.\build-msvc\crystal_tests.exe "<name>" -s      # -s prints INFO tables (sweeps, batteries)
.\build-msvc\crystal_tests.exe "[economy-report]" -s   # balance battery; also [sim-report], [goosy-report], [eternal-report]
.\build-msvc\CrystalForge.exe --canonicalize    # ALWAYS after hand-editing data/*.json
powershell -ExecutionPolicy Bypass -File tools\package.ps1   # stage+validate+zip -> dist\
```

The test binary is `build-msvc\crystal_tests.exe` (NOT `tests\...`). Asset
generators live in `tools\asset_gen\` (`generate_textures.ps1`,
`generate_audio.ps1` fed by `music_data.ps1`, `generate_font.ps1`,
`generate_icon.ps1`, `preview.ps1` — the sprite-review gate).

## Gotchas — build & environment

1. **Initialize the VS environment or nothing works.** `where.exe cl` /
   `where.exe ninja` before configuring. Always pass
   `-DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl` when configuring without
   presets so a stray MinGW on PATH can't be picked (it broke build #1).
2. **`LNK1168: cannot open ArePGeese.exe`** = a running (or crashed) game
   instance holds the file — usually the owner's play session. Say so and
   wait; do not fight the linker.
3. **Never include `<windows.h>` in a TU that also sees raylib** (Rectangle/
   DrawText/CloseWindow collide; `min`/`max` macros break `std::max`;
   `WIN32_LEAN_AND_MEAN` does NOT save you). Win32 code goes in its own
   `.cpp` under `src/platform/` behind a standard-types header (see
   `platform/FatalDialog.*`). Never add an explicit `user32` link
   (`LNK2005 CloseWindow`).
4. **Build the exe AND the tests** — building only `crystal_tests` hides
   link and `main.cpp` breakage. Release must still build too: the
   multi-config gating (`CRYSTAL_CAPTURE`/`CRYSTAL_DEBUG_OVERLAY` absent
   from Release) only breaks there.
5. **High warnings on project code only** — never "fix" warnings inside
   `_deps`.

## Gotchas — engine & determinism

6. **GPU calls need the window.** Tests run headless — keep pure logic
   (generation, danger, scoring, battle sim, text layout) raylib-free.
   RAII-wrap every raylib resource (`src/render/RaylibRAII.hpp`).
7. **The 426×240 render texture blits with NEGATIVE source height** (-240;
   render textures are y-flipped). `SetExitKey(KEY_NULL)` or ESC quits the
   game behind your back.
8. **Never mutate the state stack mid-iteration** — transitions are queued
   (`StateStack` pending commands). No per-frame heap in hot paths.
9. **Anything that changes what a seed produces bumps
   `dungeon::kGenerationVersion`** — generator code OR data curves
   (including item prices: the merchant derives offers from them).
   Owner-gated; add a history line. Battle-rule changes bump
   `battle::kBattleRulesVersion` the same way.
10. **Event/resolution randomness is PURE HASH, never an rng-stream draw**
    (`dungeon::themeEventHash(seed, roomIndex, salt)`, fresh salt pair per
    use — appearance and pick separate). A stream draw reshuffles every
    seed and breaks reload-honesty. New encounter events are ONE ROW in
    `encounterRegistry()` (`dungeon/ThemeEvents`) — equal weight, no
    per-event chance constant, no chain position.
11. **A shared battle rule lives in `battle::` code** called by
    `BattleState`, the `Simulator`, AND the AI choosers (see
    `forcedActionFor`/`forcedChoice`, `uncontrolledChoice`). Enforcing one
    in a single state silently desyncs live play from the sim.

## Gotchas — tests, data & assets

12. **Never edit `data/*.json` while the suite runs** (tests read source
    `data/` live — phantom failures). Canonicalize after hand edits.
13. **Catch2 test names: ASCII only** — em dashes in names break ctest
    filters.
14. **Generator discipline:** append-only sections with a fresh
    `$script:rng` reseed; hand-placed ASCII grids (bosses hard-capped
    36×36); after any run, `git status` must show ONLY intended new files
    (byte-stability proof); review sprites via contact/silhouette sheets
    BEFORE accepting (`preview.ps1`, or an upscaled sheet read back as an
    image); every asset gets an `assets/credits.md` row; grid palette keys
    are the sanctioned `$GRIDC` table (art_bible §2 census binds hex
    literals).
15. **Screenshots come from the capture tool, never window automation**
    (`--capture`; focus-dependent SendKeys leaks keystrokes — it
    happened). Register a deterministic capture scene for every new UI
    surface — the overflow lint is the layout referee, and it has caught
    real overflows mid-round. Animated panels need a fixed-clock capture
    hook so the frame is byte-exact.
16. **PowerShell console output mangles UTF-8** (em dashes render as
    mojibake) — for exact file text always trust the Read/Grep tools, and
    never paste console-rendered text back into files. README.md and this
    file were once double-encoded by exactly that loop.

## Gotchas — text & UI

17. **Every text container uses one of the five overflow policies**
    (`docs/ui_style_guide.md` §7 — A fitted, B preview, C viewport,
    D scrolled list, E ellipsized). `[ui-overflow]` in a debug build is a
    real defect and fails capture. Authored `\n` is paragraph semantics
    only; glyphs are bound by `src/ui/GlyphCoverage.hpp` (a test enforces
    it). Gear names in prose follow §13 (`ui::drawGearNameTag`).
18. **Footer hint strips are width-budgeted** (~418px) — a fifth hint or a
    long label overflows; shorten labels, and let a long dynamic value
    (e.g. a target name) HIDE a hint rather than collide with it.

## Milestone/fix-round ritual (the operational half of CLAUDE.md's rules)

- Re-audit the milestone note against the current checkout before
  implementing; statuses and scope claims in notes go stale.
- A fix or adjustment to an APPROVED milestone gets a **dated section in
  that milestone's note** (the canonical record lives in ONE note; other
  affected notes get one-line pointers), a manual-test-matrix row, ledger
  remark if status-adjacent, and a memory update. Never leave a doc
  describing superseded behavior.
- Insert new matrix rows in NUMERIC ORDER (anchoring an insert on the
  row above, not below — this slipped twice).
- Verification battery before claiming done: debug build → targeted test
  tags → full ctest → `--capture` lint → release build. Report actual
  results; anything unrun is stated as unrun with exact commands.
- Git: inspection only. The owner commits.

## When to ask vs. decide

`CLAUDE.md` §Mandatory escalation is the list; recommended-answer +
consequences format; `"I agree 100%"` approves all recommendations. Feel,
fun, readability, balance, and audio judgments are owner-only — matrix rows
carry them.
