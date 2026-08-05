# M86 — CrystalForge catch-up & version 0.6.0

Authorized 2026-08-05 as the closing milestone of the M75–M86 expansion
program (see the program section in `docs/milestones.md`). Re-audit this
note against the then-current checkout before implementation begins.

## A. Status

☐ planned

## B. Goal (owner brief)

CrystalForge learns everything the program added, and the game's version
is deliberately renumbered to **0.6.0** (owner decision 2026-08-05: the
0.9.0 label overstated completeness; the README's release claim follows).

## C. Scope (planned)

### CrystalForge

- FieldDescriptor coverage for every field the program added:
  `initialStatuses`, `triggers`, `mpDamagePct`, `elementResist`,
  `iconCategory`, the AI/immunity flags, any `maxHeld`/punchline fields —
  the M59 descriptor-completeness sweep is the enforcement.
- New editable categories: `data/event_flavor.json` (M80) and
  `data/curio_lore.json` (M85) — browse/edit/save/validate like the other
  nine, through the real loaders.
- Enum ids (the three new statuses, etc.) flow automatically via the
  shared `*Ids()` tables — verified, not assumed.
- `--canonicalize` round-trip proven over the full post-M85 data tree
  (every authored entry byte-canonical); the sim lab and test runner still
  work against the v15 battle rules.

### Version 0.6.0

- `CMakeLists.txt`: `project(CrystalDungeons VERSION 0.6.0)`; regenerated
  `core/Version.hpp`; the CMake comment and the README's
  "0.9.0 until M23, then 1.0.0" claim rewritten to the new numbering.
- No save/score migration — their schema versions are independent (v1
  stays v1).

### Docs sweep

- Final consistency pass across `docs/game_design.md`,
  `docs/technical_design.md`, control/asset/validation docs and README
  for everything M75–M85 changed (each milestone updates its own docs;
  this is the safety net, not the first write).
- Ledger execution-order note updated: **M23 → M24 run after M86**,
  re-audited against the then-current checkout (capture set, batteries
  and packaging manifest have all grown again).

## D. Schema, save & version implications

- No game-behavior change at all; the version constant and editor only.

## E. Out of scope

Any gameplay/content change. M23/M24 themselves.

## F. Dependencies

M75–M85 all implemented (their fields must exist to describe).

## G. Acceptance criteria

- Every new field is editable in CrystalForge with validation; the
  editor's descriptor-completeness battery passes; both new categories
  round-trip byte-canonically.
- The game and window title report 0.6.0; README makes no stale claim.
- The docs sweep finds (and fixes) nothing describing superseded behavior.

## H. Automated validation

[editor] battery over the new descriptors/categories; canonicalize
round-trip; full suite green in Debug and Release.

## I. Owner manual validation

Open CrystalForge, edit one trigger, one flavor line and one curio lore
entry, save, and see each in-game; confirm the version string everywhere
it appears.
