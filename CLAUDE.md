# CLAUDE.md

Operating contract for this repository — highest authority. The skill at
`.claude/skills/crystal-dungeons/SKILL.md` is the repeatable workflow helper,
not a substitute for this file.

## Project identity

`Crystal Dungeons` — a 16-bit-inspired turn-based JRPG roguelite in C++20 with
raylib. A focused dungeon-score game, not a broad story JRPG.

Core loop: town hub → prepare party → seeded dungeon → visible enemy teams
guarding gates and chests → boss → score driven mainly by fewest battle turns →
upgrade → repeat.

**Originality is a hard constraint.** The game is inspired by the clarity,
readable UI, side-view battles, party identity, and atmosphere of 16-bit JRPGs.
It must not copy Final Fantasy or any other protected game: no copyrighted
names, characters, music, sprites, fonts, maps, monsters, spells, UI replicas,
story elements, or asset layouts. This binds every milestone.

Authoritative game behavior lives in `docs/game_design.md`; architecture in
`docs/technical_design.md`. This file does not restate either.

## Document authority

Highest first:

1. `CLAUDE.md` — this contract.
2. The active `docs/milestone_notes/MXX_*.md` — current implementation scope.
3. `docs/milestones.md` — milestone ledger and status.
4. `docs/game_design.md` — game behavior.
5. `docs/technical_design.md` — architecture.
6. `docs/completion_roadmap.md` — strategic direction only; never authorization
   to work ahead.
7. Supporting authoring, control, asset, validation, and test documents.

Where documents conflict: name the conflict explicitly, resolve by this order,
escalate material product or architecture conflicts to the owner, then update
the stale document. Never silently choose the convenient reading.

This file must not duplicate milestone status, game rules, architecture, build
commands, or content specs. Restated facts drift out of date; point instead.

## Session start

1. Read this file.
2. Read `docs/milestones.md` — it is the single source for which milestone is
   current and what its status is.
3. Read the active milestone note, plus `docs/game_design.md` and
   `docs/technical_design.md` as relevant to the work.
4. Inspect HEAD and the working tree before proposing changes.
5. Do not restart historical milestones or act on stale task text.

If this file and the ledger ever disagree about project status, the ledger is
correct and this file is the defect.

## Milestone workflow

Work milestone by milestone. Do not begin the next one until the owner
explicitly authorizes it.

Statuses — exactly these:
`planned` · `in progress` · `implemented, awaiting manual approval` ·
`complete (approved)` · `blocked`

**Only the owner may set `complete (approved)`**, after manual testing. Builds,
tests, screenshots, simulations, and self-review never justify it. The terminal
state a Claude session may set is `implemented, awaiting manual approval`.
Approval of one milestone is not authorization to start the next.

Re-audit a milestone note against the current checkout before implementing it —
a note describes the repository as last reviewed, not as it is now.

Once the owner approves a plan, execute the approved slices without asking about
routine implementation details. Decide autonomously where the choice stays in
scope, changes no player-facing rule, adds no dependency, alters no public
schema, breaks no save compatibility, invalidates no deterministic behavior, and
commits to no new architecture.

## Mandatory escalation

Stop and ask the owner before:

* changing approved player experience, or combat, scoring, progression, or
  dungeon rules outside approved scope;
* adding or replacing a dependency;
* a major architectural rewrite;
* changing public JSON schemas beyond the approved plan;
* changing save compatibility or deterministic seed behavior;
* changing the virtual resolution or core rendering assumptions;
* adopting a final art direction that is not already approved;
* deleting or replacing substantial historical documentation;
* expanding into a later milestone.

When asking: only questions that materially affect the project, always with a
recommended answer and the brief consequence of each option. `I agree 100%`
approves all recommended answers.

For minor details: choose conservatively, keep it reversible, document the
assumption, and do not block progress.

## Honesty and verification

Never claim a build passes unless the build command ran and succeeded. Never
claim tests pass unless they ran and passed. When something cannot be verified,
report what was not verified, why, the exact command the owner should run, the
expected result, and what to send back on failure.

Automated tests never prove visual quality, control intuitiveness, balance,
enjoyment, accessibility in practice, or release readiness.

Owner validation is required for: visual feel and readability, controller and
keyboard feel, fun factor, difficulty balance, audio acceptability, whether a
milestone satisfies its intended player experience, and any check that requires
actually playing the game.

## Documentation responsibility

Documentation is part of the implementation, not optional cleanup. Before
declaring a milestone implemented: update its note with what was actually built,
record deviations from the plan, update every affected design, architecture,
content, control, asset, save, and validation document, update
`docs/milestones.md`, set `implemented, awaiting manual approval`, give the
owner a manual test checklist, and report build and test results using
`docs/milestone_completion_template.md`. Then stop.

Never leave documentation knowingly describing superseded behavior. Prefer a
pointer over a copy.

After the owner approves: set `complete (approved)`, record the approval date in
the ledger, and stop unless also authorized to begin the next milestone.

## Git

Never commit, push, amend, rebase, merge, tag, or force-update a branch.
Inspecting status, diffs, history, and HEAD is fine. The owner handles all Git
operations.

## Working tree

Preserve unrelated owner changes. No destructive resets, no broad formatting
churn, no editing of unrelated files. Report pre-existing modifications before
touching overlapping files. Keep changes reviewable.

## Engineering constraints

Build, run, and test commands live in `README.md`. Do not restate them here or
in any other document.

* C++20, CMake, MSVC (Visual Studio 2022 or newer). MinGW/GCC is unsupported.
* Dependencies are pinned by tag in `cmake/Dependencies.cmake` and acquired via
  FetchContent. Never float to a branch. Adding, removing, or bumping one
  requires owner approval.
* Windows is the primary target. Isolate Windows-only code behind a platform
  abstraction in `src/platform/`.
* No network features, no shell execution from game code, no arbitrary path
  loading from untrusted input. Sanitize save and data paths, validate JSON,
  handle malformed saves and content without crashing, and never write outside
  the intended save/config directories.
* Placeholder assets are temporary fallbacks, never the final target.

## Definition of done

A milestone is done only when it stayed within scope; the project builds and
tests pass (or exact unverified commands are provided); affected docs are
updated; known issues and manual-validation needs are listed; no unrelated
refactors are included; and no next-milestone work was smuggled in.
