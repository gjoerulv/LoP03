# CrystalForge — the content editor (designer guide)

CrystalForge is Crystal Dungeons' content editor: a separate desktop tool for
balancing and authoring the JSON content in `data/` — skills, classes,
enemies, bosses, items and equipment, passives, dungeon themes, the team
composition rules, and the story beats. It is built from the same code the
game runs: the **real content loader** validates your edits and (M60) the
**real battle simulator** plays them out, so the editor can never accept
something the game rejects, and its sims can never behave differently from
live play.

Build commands live in `README.md` ("Development tools"). CrystalForge is a
development tool: it is built with the game (`CRYSTAL_ENABLE_EDITOR`, on by
default) and is never part of a shipped package.

## Starting it

```powershell
.\build-msvc\CrystalForge.exe
```

By default it opens the **source-tree `data/`** — the files the repository
tracks — so saved edits are exactly what lands in version control. Pass
`--data <dir>` to edit another content folder. The game loads content at
startup: restart the game (or rebuild, which recopies `data/` next to the
exe) to play your changes.

Version control is the safety net: the editor writes atomically and only on
save, and `git diff` shows every change it made. Save deliberately.

## The screen

Four panes, left to right:

1. **Categories** — the nine content files. A `*` marks unsaved changes.
2. **Entries** — the entities of the selected category (id + display name).
3. **Fields** — the selected entity's fields, one row each, driven by the
   game's own schema. Unrecognized keys (future schema) are shown dimmed and
   survive saves untouched.
4. **Messages** — validation results: loader errors (press Enter on one to
   jump to the offending entity and field) and the quick-check verdicts.

The footer always shows the keys that work right now.

## Keys

| Key | Where | Does |
|---|---|---|
| Tab / Shift+Tab | anywhere | next / previous pane |
| Arrows | anywhere | navigate; Left/Right also steps a focused value |
| Enter | lists | drill in / activate / choose |
| Esc | anywhere | back out; at the top level, quit (guarded when dirty) |
| Ctrl+S | anywhere | save every dirty file, then re-validate |
| F5 | anywhere | re-validate without saving |
| N / D / Del | entry list | new entry / duplicate entry / delete entry |
| type text | pickers | filter the candidate list |

The mouse works for the basics: click a row to select it, wheel to scroll the
pane under the cursor.

## Editing fields

- **Numbers** step with Left/Right (each field has sane bounds and step
  sizes; class growth steps in tenths).
- **Toggles** flip with Left/Right or Enter.
- **Choices** (enums like a skill's target, and references like an enemy's
  skills) cycle with Left/Right, or press Enter for a searchable picker.
  References always offer the ids that exist *right now* — including entries
  you just added.
- **Text** (names, descriptions, story bodies) opens an edit line on Enter.
  Renaming an **id** warns when other content still references the old id —
  validate and fix the references it lists.
- **Lists** (an enemy's skills, a theme's rosters) open on Enter: `N` adds
  from a picker (already-present ids are hidden), `Del` removes,
  `Ctrl+Up/Down` reorders.
- **Entry tables** (a class's learnset, an item's status riders) open on
  Enter: pick an entry to edit its fields inline, `N`/`Del` to add/remove.
- Optional fields at their default are **omitted from the file** — exactly
  how the shipped content is authored, so diffs stay clean.

## Validation and quick checks

Every save (and F5) rebuilds the whole content set through the game's loader
and cross-reference checks. Errors list in the Messages pane; Enter jumps to
the culprit. **A file is saved even when it has errors** — the safety net is
that the game refuses invalid content at startup with these same messages, so
fix everything red before playing.

Three **quick checks** then simulate fixed battles through the real
simulator, as a seconds-fast smoke alarm for the difficulty curve:

1. *Fresh party wins the first fight* — naked level-1 party vs the first
   theme's opening pair.
2. *Mid-ladder boss is beatable* — level-20, median-geared party vs the
   first theme's first boss (with court) at 150%.
3. *Endgame gear clears the Boss Rush opener* — level-99, best-geared party
   vs the first rush boss at its castle scale. (Deliberately not the King:
   by approved balance he defeats any party that brings no Royal-Relic
   counterplay, and the simulator uses no items.)

A red check does not block saving — it means *look at what you just changed*.
The full balance batteries (`ctest`) remain the real referee.

## Canonical formatting

Saves always go through one canonical writer (2-space indent; one line per
entry in `skills`/`enemies`/`items`/`passives`, block entries elsewhere), so
formatting can never drift between saves and diffs stay minimal. The one-time
normalization of the shipped files happened at M59. If a file is ever
hand-edited into a different shape, `CrystalForge --canonicalize` restores
the canonical form headlessly and proves the values unchanged.

## The Sim Lab (M60)

The two bottom sidebar entries open the deeper tools. **Sim Lab** runs seed
sweeps of any matchup through the game's real simulator:

- **Party:** level, up to four members (class / weapon / armor / accessory /
  passive, each with a searchable picker), or one keypress presets the whole
  party (bare / median gear / best gear, class-aware).
- **Opponent:** a hand-built enemy team at any stat scale, a boss with its
  authored court, a Boss Rush fight, an Endless wave, or the King himself.
- **RUN SWEEP** resolves 10 / 100 / 1000 seeded battles and reports win
  rate, turn statistics (avg/median/min/max), average party HP, party KOs,
  the opponent's danger tier — and, from the record-only battle observer,
  per-action usage/damage/healing and per-combatant damage dealt/taken.
- Run, edit content or config, run again: the results pane shows **deltas
  against the previous run** — the core balancing loop.
- **Export** writes a markdown or CSV report into the git-ignored `reports/`
  folder (timestamped filenames).

## The Test Runner (M60)

**Test Runner** executes the real Catch2 suite (`crystal_tests.exe`, built
beside the editor) per category — enemies & bosses, skills & status, items &
economy, classes & passives, content validation, or everything — streaming
the output live into the pane, with a pass/fail verdict from the exit code.
`Del` stops a run. Edit content, save, run the matching category: minutes of
terminal round-trips become one keypress.

## What the editor will not do

- It never touches battle rules, code, saves, or assets — content JSON only.
- It does not hot-reload the running game.
- It is not a balance oracle: quick checks catch broken curves and the sim
  lab measures the consequences, but the owner's playtesting outranks both.
