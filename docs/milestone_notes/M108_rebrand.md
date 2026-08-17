# M108 — Rebrand: "Are P Geese"

**Status:** implemented, awaiting manual approval
**Program:** M98–M108 "Are P Geese" (owner-authorized 2026-08-16) — the
program's last milestone, run last on purpose: one identity flip over a
stable build. No battle-rules, generation, or save-FORMAT change; the save
LOCATION moves (owner decision: FULL rebrand) with a one-time, loss-proof
migration.

## Scope (owner item + interview ruling)

"New working Title is 'Are P Geese'. The title screen, icons, game icon
and everything related should be remade accordingly." Interview: the FULL
option — exe/CMake target and the save folder included, with auto-copy
migration that never deletes the old folder.

## What was built

- **Display identity**: window title, title screen (both render
  `config::kWindowTitle`), fatal-error dialogs, startup/shutdown log
  lines, the tutorial welcome (constexpr + data), and the Windows
  VERSIONINFO (ProductName / FileDescription / OriginalFilename /
  InternalName) all say **Are P Geese**.
- **Build identity**: CMake `project(ArePGeese)`, the game target/exe is
  **ArePGeese.exe**, the rc template is `packaging/ArePGeese.rc.in`
  (old template removed), `package.ps1` stages and zips
  `ArePGeese-<version>-win64`. Build dirs and presets keep their names
  (local, invisible); CrystalForge keeps its name (a tool, not the game).
  A stale `CrystalDungeons.exe` may linger in existing build dirs until a
  clean configure — harmless, noted for the owner.
- **The save home**: `%APPDATA%\ArePGeese` (`paths::userDataDir`; the
  same env-derivation on every platform). **Migration**
  ([platform/Migration](../../src/platform/Migration.hpp), called first
  thing in `main`): if the new home is absent or empty and the legacy
  `CrystalDungeons` folder exists, everything copies across recursively —
  skip-on-error, idempotent, and the legacy folder is NEVER modified or
  deleted, so it remains the recovery source even mid-interruption. A
  populated new home always wins.
- **Docs identity**: CLAUDE.md's project-identity clause (renamed with
  the owner's authority recorded), README (title + run/package commands),
  game_design (the name and its joke: THE STRANGER "P" may be a goose;
  we ask), the workflow skill's exe references. Historical documents
  (milestone notes, the ledger's past entries) deliberately keep their
  original wording — they are records, not descriptions of the present.
- **Deliberately interim**: the icon ART is still the crystal
  (`packaging/crystal.ico` via `generate_icon.ps1`) and the title-screen
  one-liner pool is unchanged — both flagged for an owner-directed
  identity-art pass; the rc comment says so.

## Verification

- New tests ([migration][m108]): nested copy-once, legacy untouched,
  populated-home wins, fresh-install/missing-source no-ops, empty-home
  acceptance. Full suite + capture + a release build recorded in the
  completion report.

## Deviations from the plan

- The icon art and one-liners stayed (see "deliberately interim") — the
  rename is complete, the art pass is the owner's call.
- The rc TEMPLATE was renamed; the skill FOLDER `crystal-dungeons` was
  not (tooling references it; content updated).

## Manual owner checklist

Matrix row **204**: the window/tab says Are P Geese; Explorer file
properties on ArePGeese.exe read the new names; with an existing
CrystalDungeons save folder and no new one, first launch migrates
everything (saves, settings, scoreboard, profile) and the old folder is
intact; a second launch copies nothing; packaging stages the new zip
name. **Judge whether the icon and title-liners should follow now.**

## Documentation updated

CLAUDE.md identity, README, game_design intro, workflow skill, ledger
row, matrix row 204, credits (none — no new art), this note.
