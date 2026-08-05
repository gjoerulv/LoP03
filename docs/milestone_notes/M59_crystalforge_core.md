# M59 — CrystalForge editor core (browse / edit / save / validate / quick checks)

Authorized 2026-07-24 with the M59–M60 CrystalForge program (plan approved by
the owner; Q&A decisions recorded in `docs/milestones.md`). Implemented
2026-07-25 on base checkout `7c8f32d` (post-M58; baseline re-verified 532/532
Debug tests before work began). This note is the milestone's record; the
designer-facing manual is `docs/editor_guide.md` and the architecture section
is `docs/technical_design.md` §17.

## A. Status

**☑ complete (approved by the owner 2026-08-05)** — implemented 2026-07-25. Evidence in §F.
Only the owner may set `complete (approved)` after manually driving the tool.

## B. Goal

A designer-facing content editor, **CrystalForge**, as a separate executable
in this repo: browse/edit/save every `data/` category (skills, classes,
enemies, bosses, items, passives, themes, composition, story), validate every
edit through the game's real loader with jump-to-entity errors, and run a
seconds-fast quick-sim sanity battery on save — with zero new dependencies,
zero game-behavior change, and no version bumps.

## C. Owner decisions (2026-07-24, planning Q&A)

- UI = the game's own M46 kit in a separate raylib window; **Dear ImGui
  rejected** (new dependency).
- Tests = in-process quick checks + (M60) spawned `crystal_tests.exe` runs.
- Sim reports (M60) = aggregates + observer telemetry.
- M57/M58 recorded approved; the editor split as M59 core + M60 sim lab.
- (2026-07-25, follow-up) M53–M56 recorded approved alongside M57/M58.

## D. As implemented

### Slice E1 — target, shell, window

- `CMakeLists.txt`: `CRYSTAL_ENABLE_EDITOR` (default ON) gates the
  **CrystalForge** exe; the pure core is an always-built static lib
  **`crystal_editor_core`** (CanonicalJson, FieldDescriptor,
  CategoryDescriptors, EditorDocs, EditorValidation) so the suite tests it
  regardless of the option. `tools/package.ps1` was verified to stage an
  explicit file list — neither target can ship.
- `src/editor/EditorMain.cpp`: window init (1280×720), the shipped `.fnt`
  fonts installed via the injectable `ui::setFonts`, a 640×360 point-filtered
  render texture blitted 2× (pixel glyphs stay crisp; every M46 kit metric
  works unchanged), `SetExitKey(0)` so Esc is a UI key and quitting always
  passes the unsaved-changes guard. Data dir: `--data <dir>` →
  compile-time `CRYSTAL_EDITOR_DATA_DIR` (the **source tree's `data/`**, so
  saves land where the owner commits from) → `cwd/data`; refuses to start
  without content. **`--canonicalize`** runs headless: canonical-rewrites
  every file, then proves values unchanged by re-validating through the real
  loader (used for the one-time normalization below; safe to re-run).
- `src/editor/EditorShell.{hpp,cpp}`: a standalone update/render loop (the
  game's StateStack stays untouched); four panes (categories / entries /
  fields / messages) + modals (text edit, filterable picker, list editor,
  entry-table editor, delete/quit confirms); keyboard-first (Tab, arrows,
  Enter, Esc, Ctrl+S, F5, N/D/Del) with an editor-level mouse layer
  (click/wheel) — `src/ui/` gained no mouse code.

### Slice E3 — documents, descriptors, canonical writer

- **`ordered_json` documents are the source of truth** (`EditorDocs`); edits
  mutate by key; unknown keys survive and render dimmed; Def structs are
  never serialized. Entity ops: add (skeleton of required fields, unique
  id), duplicate (`_copy` uniquified), delete (guarded by a whole-corpus
  reference count), id-rename reference warning.
- **Canonical writer** (`CanonicalJson`): one formatter for every save —
  2-space root; compact one-entity-per-line for `skills`/`enemies`/`items`/
  `passives`; block entities for `classes`/`bosses`/`dungeon_themes`/
  `story`/`composition`; nested values inline; arrays-of-objects one per
  line; scalars via nlohmann's own dump (string escaping, float
  round-trip — class growth `12.0` stays `12.0`). Writes are atomic
  (`platform::AtomicFile`).
- **One-time normalization executed:** `CrystalForge --canonicalize`
  rewrote all 9 files — the working-tree diff is only: blank group-separator
  lines removed (`skills`/`enemies`/`items`), `composition.json`'s two
  inline sections expanded to the block shape, and CRLF→LF (the writer emits
  `\n` like `SaveSystem`; comparisons are line-ending tolerant so git
  autocrlf checkouts cannot re-churn). The tool printed
  `9 file(s) rewritten, 0 content error(s) before and after`, and the
  round-trip test now pins byte-stability forever.
- **Descriptor tables** (`FieldDescriptor` + `CategoryDescriptors`): one
  `FieldDesc` per key the loader reads, driving both the form and the
  omit-when-default write policy. Descriptor keys are the **loader's** keys —
  implementation corrected three mismatches the tests caught: a skill's
  control effect is authored as `control` (not the Def field name), foe
  affinities are FLAT `weaknesses`/`immunities` arrays (no `affinity`
  wrapper), and `bosses.archetype` / `items.type` are loader-required
  (`reqEnum`) so they are always written.
- Additive `src/content/` helpers: `elementIds()`-style value lists for all
  15 enums, built from the same tables `parse*` reads (one source of truth),
  plus header declarations for the already-external `parseStory` /
  `parseComposition` so the editor can validate in-memory.
- Additive `ui::TextInput` `TextFilter::Printable` mode for
  descriptions/story text; the game's name entry is untouched (default
  filter unchanged).

### Slice E4 — validation + quick checks

- `EditorValidation`: serializes the documents and runs the **real** parsers
  + `validateReferences` into a scratch `ContentDatabase`; the messages pane
  lists every `LoadError` (source/context/message) and Enter jumps to the
  entity and field (id extracted from the error context). Saving is never
  blocked by errors — the game's startup validation (same code, same
  messages) is the enforcement point; the guide says so.
- **Quick checks** (3 fixed seeded battles via `battle::simulate`): fresh
  naked L1 party vs the first theme's opening pair; L20 median-geared party
  vs the first theme's first boss + court at 150%; L99 best-geared party vs
  the Boss Rush opener at castle scale. Gear picks are deterministic and
  class-aware (a mage gets the best rod, not a spear). Informational only.

## E. Plan deviations

- **Quick check 3 is the Boss Rush opener, not "the King still falls".**
  The plan sketched a King check; the first run showed a maxed sim party
  LOSING to the King — which is the owner-approved M54 balance (he defeats
  any party without Royal-Relic counterplay, and the simulator's AI uses no
  items). A King check would test the sim's limitations, not the curve.
  Routine substitution within scope; flagged here.
- **Canonical entity styles are per-file, not one shape.** The plan's writer
  sketch predated reading the shipped files, which use two deliberate shapes
  (compact rows vs block entities). The writer preserves both; the
  normalization diff shrank from "whole files" to a handful of lines.
- **Descriptor key corrections** (`control`, flat affinity,
  required `archetype`/`type`) — the completeness/loader tests caught all
  three before any file was written; recorded so the schema tables' authority
  (the loader, not the Def structs) is remembered.
- No other deviations.

## F. Automated validation (all run in this session, 2026-07-25)

- Configure/build: `cmake --build --preset debug` (VS2022 dev shell) —
  clean, no project-code warnings.
- Data normalization: `.\build-msvc\CrystalForge.exe --canonicalize` →
  `9 file(s) rewritten, 0 content error(s) before and after`; working-tree
  diff reviewed (separator lines + composition block shape + line endings
  only).
- Editor battery: `crystal_tests.exe "[editor]"` — **12 cases / 2913
  assertions, all pass** (canonicalization value-preservation across every
  shipped file, writer idempotence + byte-stability, loader parity of the
  scratch database, quick checks pass on shipped content, unknown-key
  survival, descriptor completeness sweep over every shipped entity incl.
  nested objects/arrays, omit-at-default policy, enum-list completeness).
- Full suites: see the M59/M60 completion report in `docs/milestones.md`
  status entries (Debug and Release counts recorded there after the final
  verification pass of the program).
- Smoke: `CrystalForge.exe` launched windowed, ran startup validation +
  quick sims, alive after 6 s, clean exit on kill. (The tool cannot run
  headless; building in both configs + the pure-core battery is its CI.)

## G. Manual owner checklist

1. `cmake --build --preset debug`, then run `.\build-msvc\CrystalForge.exe`.
2. Walk all nine categories; spot-check entries against the raw JSON.
3. Edit: step an enemy's ATK (Left/Right), rename an item (Enter → type →
   Enter), toggle a bool, cycle an enum, open a skill picker (Enter on an
   enemy's Skills list, `N`, type to filter, Enter), edit a class learnset
   entry, and change a composition number.
4. `Ctrl+S`; confirm `git diff data/` shows exactly your edits (plus nothing).
5. Break something on purpose (point an enemy at a deleted skill id via
   Del on the skill list + save): the Messages pane goes red; Enter on the
   error jumps to the enemy; fix it; F5 goes green.
6. Watch the three quick checks report; then sanity-run the game with your
   edited content (rebuild or restart so it recopies/loads `data/`).
7. `N` a new skill, `D` a duplicate, `Del` it again — confirm the reference
   warning appears when deleting something in use.
8. Quit with unsaved changes (Esc) — the guard must offer save/discard/
   cancel; the window's X offers save/discard.
9. Mouse: click rows in each pane, wheel-scroll the entity list.
10. Optional: re-run `CrystalForge --canonicalize` — it must report 0 files
    rewritten.
11. On failure: send the console output, the `git diff`, and what you did.

## H. Known limitations

- Keyboard repeat is raylib's default; very long lists scroll by PgUp/PgDn
  or mouse wheel.
- Entry-table editing (learnsets, status riders) cycles reference ids with
  Left/Right rather than opening the filterable picker (kept one modal
  deep); lists themselves have the full picker.
- The editor does not hot-reload the running game (restart to play edits).
- Story/composition rows have no add/delete (fixed shapes: 8 beats, one
  rule block) — by design.
- A file saved with content errors is accepted on disk (the game refuses it
  at startup with identical messages); the guide documents this contract.

## I. Documentation updated

- `docs/milestones.md` — M59 section + row (this milestone), M53–M58
  approvals recorded, narrative updated.
- `docs/editor_guide.md` — **new** designer manual.
- `docs/technical_design.md` — new §17 (editor architecture).
- `README.md` — "Development tools" section + project layout row.
- `docs/completion_roadmap.md` — §20 program direction (at planning).
- `docs/game_design.md`, `docs/manual_test_matrix.md` — checked,
  intentionally unchanged (the editor is a development tool, not player
  behavior).

## J. Final status

`complete (approved 2026-08-05)` — M60 (sim lab, battle observer,
test runner) continues next under the same approved plan.
