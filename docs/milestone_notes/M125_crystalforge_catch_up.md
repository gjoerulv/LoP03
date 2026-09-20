# M125 — CrystalForge catch-up

**Status:** complete (approved 2026-09-20)
**Program:** M120–M125 (owner-authorized 2026-09-18 via the approved plan;
branch `oyb12`, baseline `d789f31`). The program's last milestone.
**No version motion:** no rule, schema, save, data or asset change. Editor,
tests and documentation only.

## Scope (plan section M125; the owner's point 12)

"CrystalForge should be updated so it works with the current version of the
game." The owner chose *catch-up + hardening* (2026-09-18): cover every field
the program added, audit the editor against everything the loader can read,
sanity-check the Sim Lab with the Elixir's MP rider, stamp the version on the
window, refresh the guide, extend the editor tests.

## Re-audit finding

CrystalForge was **already working with the current version**: it loads,
validates and canonicalizes all fifteen data files with zero errors, its
descriptors cover every key in the shipped data (the M59 sweep), it preserves
unknown keys, and the two fields this program added (`mpAmount`, M120;
`skillTexts`, M121) were given descriptors in their own milestones precisely
so the editor never fell behind. M125 therefore found nothing to repair. What
it adds is the guard that was missing, the round-trip proof for the new
fields, and the stamp and guide the plan named.

## What was built

### The loader-key audit (`tests/test_editor_loader_audit.cpp`)

- The M59 sweep only sees keys that appear in the **shipped** data, so an
  optional loader key nobody has authored yet — inert by default, which is
  how most fields are born — could exist with no editor field and no failing
  test. The audit closes that: it reads `src/content/ContentLoader.cpp`
  itself (new test definition `CRYSTAL_TEST_SOURCE_DIR`), splits it into its
  top-level functions, maps parser ↔ data file from `loadAll`'s own calls,
  and collects every key a parser can read: `req*`/`opt*` readers (templated
  ones included), raw `.find("...")`s, keys handed to the shared `read*`
  helpers, and the helpers' own keys, closed transitively. Every such key
  must have a field descriptor in that file's category, at any depth; every
  editor category must be a loaded file, and every loaded file an editor
  category.
- Result today: **237 loader keys across 15 files, all described** — no gap.
  (Spot-checked that the scan really sees them: `mpAmount`, `skillTexts`,
  `kingMpAmount`, `resistPct`, `script`, `triggers` ×3, `minTown` ×5, ...)

### Round-trip proof for the program's fields

- `tests/test_editor_descriptors.cpp`: `mpAmount` is an optional number,
  absent = 0, written when set, removed at zero, and the shipped Elixir reads
  50 through the descriptor; `skillTexts` is an object array of `skill` +
  `description`, and every authored rewrite in the shipped data is fully
  described (at least the five M121 shipped).
- `tests/test_editor_canonical.cpp`: an edited rider (75) builds a clean
  scratch database through the **real loader** and the loaded item carries
  it; the same rider on a non-healing item is reported by validation;
  `skillTexts` survives the canonical writer exactly
  (array, order, text); a duplicate rewrite and a rewrite naming an unknown
  skill are validation errors; removing them makes the documents clean again.

### Sim Lab sanity (`tests/test_editor_simlab.cpp`)

- The simulator has **no item command** — simulated parties never open the
  bag — so the Elixir's rider cannot move a sweep. The test builds two
  scratch databases from the editor's documents (rider 0 and 50, asserting
  the loaded value each time) and requires identical sweeps (wins, average
  and median turns, KOs, HP fraction, telemetry count). It is pinned so a
  future "the sim uses items" change has to come back to the editor's
  guidance; the guide now says plainly that items are judged by play.

### The version stamp (`src/editor/EditorTitle.hpp`)

- `cd::editor::windowTitle()` → "CrystalForge - Are P Geese v<version>" from
  `version::kString` (the stamp the game's title screen shows); `EditorMain`
  passes it to `InitWindow`. Pure and pinned by a test.

### The guide (`docs/editor_guide.md`)

- The versioned window title; a "Fields added by the M120–M125 program"
  section (MP Amount, Skill Texts, why skill kinds/icons have no field, that
  Iron Man added no content); what the Sim Lab does not model; and a
  developer section on the two staleness guards.

## Deviations from the plan

- None in scope. The plan expected possible descriptor gaps from the audit;
  there were none, so `CategoryDescriptors.cpp` is unchanged in this
  milestone.

## Compatibility

Nothing the game reads or writes changed. The game executable is unchanged
by this milestone apart from being rebuilt.

## Known limitations

- The audit is a source scan, deliberately simple: it understands the
  loader's present idioms (`req*`/`opt*`, `.find("key")`, `read*` helpers).
  A key read some other way (a computed key, a new helper family not named
  `read*`) would be invisible to it — the M59 shipped-data sweep still
  catches such a key the moment content uses it.
- It checks that a descriptor **exists**, not that its kind, bounds or
  default mirror the loader's.
- The editor's UI itself has no capture scenes (it never had; it is a
  separate executable) — its screens are owner-validated.

## Documentation updated

`docs/milestones.md`, this note, `docs/editor_guide.md`,
`docs/technical_design.md` (§66), `docs/manual_test_matrix.md` (rows
267–268).

## Completion report

### 1. Implementation summary

**M125 — CrystalForge catch-up.** Complete. The editor already handled the
current content; it now also fails the suite if the loader ever learns a key
it cannot edit, proves the program's two new fields edit, validate and
round-trip, states what the Sim Lab does not model, and names its build in
the window title.

### 2. Files changed

- **Source:** `src/editor/EditorTitle.hpp` (new), `src/editor/EditorMain.cpp`.
- **Tests:** `tests/test_editor_loader_audit.cpp` (new),
  `tests/test_editor_descriptors.cpp`, `tests/test_editor_canonical.cpp`,
  `tests/test_editor_simlab.cpp`, `tests/CMakeLists.txt`.
- **Docs:** see "Documentation updated".

### 3. Plan deviations

None.

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

All run 2026-09-19 from the VS 2022 developer shell (amd64):

- `cmake --build --preset debug` - **succeeded** (game, CrystalForge, tests).
- `crystal_tests.exe "[m125]"` - the loader audit alone: **270 assertions,
  passed** (237 key checks; `-s` output spot-checked for the keys named
  above). `crystal_tests.exe "[m125],[editor]"` - **29 test cases, 6975
  assertions, all passed**.
- `CrystalForge.exe --canonicalize` - **0 files rewritten, 0 content errors
  before and after**.
- `ArePGeese.exe --capture <dir>` - **177/177 scenes clean** (no game screen
  changed; run as a regression check).
- `cmake --build --preset release` - **succeeded**.
- `ctest --preset debug` - **951/951 passed** (396 s).
- `ctest --preset release` - **947/947 passed** (380 s).
- Scripted docs pass: every capture name cited for scenes 150+ across
  `docs/` and `README.md` exists in the scene list (the one miss, in the
  M119 note, is that note's own statement that the scene was dropped).

### 6. Manual owner validation

Matrix rows **267–268**: open CrystalForge and confirm the window title;
edit the Elixir's MP Amount and a milestone's Skill Texts, validate, save,
inspect the diff, and see the change in the game; try the two invalid edits
and confirm validation reports them.

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`complete (approved 2026-09-20)`
