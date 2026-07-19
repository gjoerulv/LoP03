---
name: crystal-dungeons
description: Operational workflow for building "Crystal Dungeons", a 16-bit-inspired turn-based JRPG roguelite in C++20 / raylib / CMake. Use when implementing any milestone, building, testing, validating, or making design/architecture decisions for this project.
---

# Crystal Dungeons — Build Workflow Skill

This is the **repeatable workflow helper**, not the contract. The contract is
`CLAUDE.md` at the repo root. When they disagree, `CLAUDE.md` wins. Keep this
skill concise and operational — do not let it become a copy of the docs.

## Vision (one paragraph)

A legally-original, 16-bit-inspired **turn-based JRPG roguelite**. A 4-character
party repeatedly enters seeded, procedurally generated dungeons from a town hub.
Dungeons have **visible** enemy teams (no random encounters), guarded chests, at
least 3 mandatory gate battles, and a boss. The hook is **efficiency**: score is
driven mainly by **fewest battle turns**. Audience: **fans of 16-bit JRPGs and
roguelites who want real tactical depth with a readable UI** (medium difficulty;
score-chasing is rewarding but not mandatory). Not a story JRPG, not an FF clone.

## Source-of-truth map (authority order, highest first)

1. `CLAUDE.md` — operating contract (authoritative).
2. Approved active `docs/milestone_notes/MXX_*.md` — current milestone scope.
3. `docs/milestones.md` — milestone ledger + status (update every milestone).
4. `docs/game_design.md` — what the game is and why.
5. `docs/technical_design.md` — architecture, conventions, build.
6. `docs/completion_roadmap.md` — long-term direction; **never** authorization
   to work ahead.
7. Supporting docs (`README.md`, style/control/asset/test docs,
   `docs/milestone_completion_template.md`). This skill — workflow, gotchas.

At session start: read `CLAUDE.md`, then the docs above, then inspect the repo
(current HEAD + working tree), then identify the current milestone and whether
it is complete.

## Pinned dependencies (do NOT float to master)

| Dep            | Tag        | Acquired via         |
|----------------|------------|----------------------|
| raylib         | `6.0`      | CMake FetchContent   |
| Catch2         | `v3.15.1`  | CMake FetchContent   |
| nlohmann/json  | `v3.12.0`  | CMake FetchContent   |

raylib tags have **no `v` prefix** (`6.0`, not `v6.0`). Catch2/json **do**.
Adding/removing/bumping any dependency requires human approval (see CLAUDE.md).

## Locked foundation decisions (from the interview)

- **Audience:** genre fans, medium depth. Difficulty & systems target tactical
  readability, not punishing opacity nor over-casual.
- **Dependencies:** FetchContent + pinned tags (above). No vcpkg/system libs.
- **Input:** keyboard **and** gamepad from M1, behind an input-mapping layer
  (`src/input/`). Gameplay code must request *actions*, never raw keys.

## Build & test commands

Build with **MSVC** from the Visual Studio developer environment: run
`vcvars64.bat` (or open a "Developer PowerShell/Command Prompt for VS") so `cl`
and the bundled CMake/Ninja are on `PATH`. **Do not use MinGW/GCC.**

Ninja (preferred):

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build-msvc
ctest --test-dir build-msvc --output-on-failure
.\build-msvc\CrystalDungeons.exe
```

Visual Studio generator (alternative; match your installed VS version):

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Debug
ctest --test-dir build-msvc -C Debug --output-on-failure
.\build-msvc\Debug\CrystalDungeons.exe
```

`-DCMAKE_*_COMPILER=cl` forces MSVC so a stray compiler on `PATH` can't be picked
up by mistake (that is what broke the first build attempt). First configure
downloads + compiles raylib/Catch2 — slow once, then cached in `build-msvc/_deps`.
Network is required for the **first** configure only.

CMake options: `-DCRYSTAL_BUILD_TESTS=ON` (default ON),
`-DCRYSTAL_WARNINGS_AS_ERRORS=OFF` (default OFF),
`-DCRYSTAL_ENABLE_DEBUG_OVERLAY=ON` (default ON).

## Gotchas (read these before you waste an hour)

1. **Build with MSVC only.** This project is built with **MSVC / C++20** from a
   Visual Studio developer environment. Do **not** use MinGW/GCC — a stray MinGW
   `g++` on `PATH` is exactly what broke the first build attempt. Always pass
   `-DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl` so the right compiler is used,
   and confirm with `where cl`. No C++17 downgrade, no GCC 8.1 workarounds.
2. **Initialize the VS environment first.** `cl` and the bundled Ninja are not on
   a normal shell's `PATH`; run `vcvars64.bat` (or use a "Developer PowerShell/
   Command Prompt for VS") before CMake, or configure can't find the compiler or
   generator.
3. **raylib + window required for GPU calls.** `LoadTexture`, `LoadFont`,
   `LoadRenderTexture`, audio init etc. need an initialized window/device. Keep
   that logic out of unit tests — tests must run headless. Put pure logic
   (scaling math, state-stack ordering, input resolution, path sanitizing,
   later: danger/score/generation) where it can be tested without raylib.
4. **RAII for every raylib resource.** Never `LoadX` without an owning wrapper
   (`src/render/RaylibRAII.hpp`). No `UnloadX` scattered in game code.
5. **Render path:** draw the world into the 426×240 `RenderTexture2D`, then blit
   that scaled to the window. The source rect height must be **negative**
   (`-240`) because render textures are y-flipped. Easy to get upside-down.
6. **`SetExitKey(KEY_NULL)`** — otherwise raylib quits on ESC behind your back.
   Handle ESC through the input map.
7. **Don't mutate the state stack mid-iteration.** State transitions are queued
   and applied between frames (`StateStack` pending commands). A state that pops
   itself during `update` must not touch the vector directly.
8. **No per-frame heap in hot paths.** Reuse buffers; avoid allocating in
   `update`/`render`. `std::function` in the input layer is fine (called a few
   times per frame, not a hot loop).
9. **Data/saves are JSON with a version field, validated, never trusted.**
   Malformed input → readable error + safe fallback, never a crash. No path
   traversal: sanitize all relative paths (`paths::sanitizeRelative`).
10. **High warnings on project code only**, not on `_deps`. Don't "fix" warnings
    inside dependencies.
11. **Driving the game for screenshots:** never use focus-dependent
    `SendKeys` — it leaks keystrokes into whatever app has focus (it
    happened). Use `PostMessage` WM_KEYDOWN/WM_KEYUP addressed to the game's
    HWND (arrows need the extended-key bit, ~70ms between down/up so GLFW
    sees both) plus `SetWindowPos` topmost+`SWP_NOACTIVATE` so
    `CopyFromScreen` actually sees the game. Proper capture tooling is M23.

## Architecture rules (enforce in review)

- Separate: simulation / rendering / input / resources / data / save / UI /
  battle / dungeon-gen. One cohesive responsibility per file; keep files small.
- No global mutable state, no raw owning pointers, no unchecked indexing, no UB.
- Deterministic & seeded where it matters (dungeon gen, danger, scoring).
- Wrap external libs behind project-owned abstractions where it pays off.
- Debug overlays only behind a flag.

## Milestone workflow

Statuses (only these): `planned` · `in progress` · `implemented, awaiting
manual approval` · `complete (approved)` · `blocked`. **Only the owner sets
`complete (approved)`**, after manual testing; Claude's terminal state is
`implemented, awaiting manual approval`. Approval of one milestone is not
authorization to start the next.

1. Read contract + docs + repo (HEAD, working tree). Identify current
   milestone; confirm the prior one is `complete (approved)`.
2. **Re-audit the milestone note against the current checkout** before
   planning; refresh it if stale. Post a concrete plan **before** coding; get
   owner authorization to begin.
3. Implement **only** the approved slices. Routine engineering decisions are
   autonomous; escalate per the CLAUDE.md mandatory-escalation list.
4. Build + run tests (or give exact unverified commands + expected output).
5. Update all affected docs — documentation is part of the implementation.
6. Report using `docs/milestone_completion_template.md`, set the status to
   `implemented, awaiting manual approval`, and **stop** for owner approval.

**Git:** never commit, push, amend, rebase, merge, tag, or force-update —
inspection only. The owner handles all commits and pushes.

Milestones: M1–M10 complete (approved; M10 on 2026-07-19). Post-M10 program:
M11 Baseline audit · M12 UI/text safety · M13 Input/settings · M14 Asset
manifest · M15 Art vertical slice · M16 Compact rooms · M17 Exploration
visuals · M18 Battle presentation · M19 Progression/score integrity ·
M20 Content variety · M21 Final audio · M22 Onboarding/accessibility ·
M23 Validation/playtesting/balance · M24 Release packaging. Details:
`docs/milestones.md` + one note per milestone under `docs/milestone_notes/`.

## Verification checklist (before claiming done)

- [ ] From a VS developer shell: `cmake -S . -B build-msvc -G Ninja
      -DCMAKE_CXX_COMPILER=cl ...` configured without error.
- [ ] `cmake --build build-msvc` compiled with no project warnings (deps may warn).
- [ ] `ctest --test-dir build-msvc --output-on-failure` — all green.
- [ ] App launches, window opens at correct aspect, scales cleanly, no crash on
      exit. (Human-validated if I can't see it.)
- [ ] Milestone's listed deliverables all present.
- [ ] Docs updated; known issues listed.
- [ ] Did NOT claim build/test "passes" unless the command actually ran green.
      If unrun: state "not verified", why, exact commands, expected output.

## Common failure modes to avoid

- Claiming a build/test passed without running it.
- Implementing beyond the approved milestone.
- Marking a milestone `complete (approved)` without the owner's manual
  approval, or treating one approval as authorization for the next milestone.
- Committing or pushing (the owner owns Git history).
- Implementing from a stale milestone note without re-auditing it first.
- Hand-authoring danger labels instead of deriving from stats (M6).
- Scoring that rewards farming/stalling instead of fewest turns.
- Copyrighted names/assets sneaking in. Everything original.
- Letting a malformed JSON crash the game.
- Y-flipped or letterbox-wrong virtual screen.

## When to ask the human (vs. just decide)

Ask (with a recommended answer + consequences) for: product direction,
player-experience design, architecture pivots, dependency changes, save-format
or public-schema changes, milestone-scope changes, anything expensive to reverse.
Human validation is mandatory for: feel, readability, fun, balance, audio,
"does this milestone satisfy the intent". For minor implementation details: make
a conservative, reversible choice, document the assumption, keep moving.
`"I agree 100%"` = approval of all recommended answers.
