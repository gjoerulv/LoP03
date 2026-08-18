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

- **Display identity**: window title, fatal-error dialogs,
  startup/shutdown log lines, the tutorial welcome (constexpr + data),
  and the Windows VERSIONINFO (ProductName / FileDescription /
  OriginalFilename / InternalName) all say **Are P Geese**. (This bullet
  originally also claimed the title screen — wrongly: the M51 plaque
  draws a hardcoded literal the sweep missed. Fixed in the 2026-08-17
  fix round below.)
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
- **Deliberately interim (superseded)**: the icon ART shipped as the
  crystal and the one-liner pool unchanged, flagged for owner judgment.
  The owner ruled on 2026-08-17: the icon is now the goose emblem (fix
  round below); the one-liners stay.

## Verification

- New tests ([migration][m108]): nested copy-once, legacy untouched,
  populated-home wins, fresh-install/missing-source no-ops, empty-home
  acceptance. Full suite + capture + a release build recorded in the
  completion report.

## Fix round (2026-08-17 — owner manual-pass verdict: title screen + icons REJECTED)

The owner's screenshot showed the title screen still reading CRYSTAL
DUNGEONS with the crystal emblem, and the crystal exe icon. Root cause of
the first: this note's original claim that "the title screen renders
`config::kWindowTitle`" was **wrong** — the M51 title plaque draws a
hardcoded literal in `MainMenuState.cpp`, which the M108 sweep missed.
Delivered:

- **Title plaque** now reads **ARE P GEESE**; the title screen fronts the
  new 32×32 **goose emblem** (`ui.emblem.goose`; a goose riding still
  water, one cyan glint), with the crystal emblem as load-failure
  fallback.
- **Exe icon**: `generate_icon.ps1` now builds `packaging/arepgeese.ico`
  from the goose emblem; CMake references it; `crystal.ico` deleted.
- **Packaging texts**: `README-player.txt` and `LICENSES.txt` headers
  (and the run-instructions exe name) rebranded — both had been missed.
- **Version 0.6.0 → 0.7.0** (owner direction): `project(VERSION)`, which
  flows to the title-screen stamp, the VERSIONINFO, and the package name.
- The title-screen ONE-LINER pool stays unchanged (the owner's feedback
  named the title and icons only).

## Deviations from the plan

- ~~The icon art stayed~~ — resolved 2026-08-17 (fix round above); the
  one-liners remain, per the owner's feedback scope.
- The rc TEMPLATE was renamed; the skill FOLDER `crystal-dungeons` was
  not (tooling references it; content updated).

## Manual owner checklist

Matrix row **204** (re-test after the fix round): the window/tab says Are
P Geese; the TITLE SCREEN plaque reads ARE P GEESE under the goose
emblem; ArePGeese.exe shows the goose icon in Explorer (icon caches can
lag — check file properties if the shell shows a stale glyph) and its
properties read the new names + v0.7.0; with an existing CrystalDungeons
save folder and no new one, first launch migrates everything and the old
folder is intact; a second launch copies nothing; packaging stages the
new zip name.

## Documentation updated

CLAUDE.md identity, README, game_design intro, workflow skill,
technical_design (icon/rc paths), art_bible §7 (the goose emblem),
assets/credits.md (emblem row), ledger row, matrix row 204, this note.
