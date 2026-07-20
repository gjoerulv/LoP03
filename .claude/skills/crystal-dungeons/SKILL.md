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

`README.md` owns the authoritative build instructions — read it rather than
trusting a copy. The operational essentials:

Build with **MSVC** from a Visual Studio developer environment. **Open
"Developer PowerShell for VS 2022"** — `& "...\vcvars64.bat"` does *not*
configure a PowerShell session (the batch file sets variables in a child `cmd`
that exits immediately). To build from an already-open PowerShell, either run
`Common7\Tools\Launch-VsDevShell.ps1 -Arch amd64`, or wrap each command:
`cmd /c "call ""...\vcvars64.bat"" >nul && cmake ..."`. **Do not use MinGW/GCC.**

Presets (M24, preferred):

```powershell
cmake --preset msvc-debug          # -> build-msvc (overlay + capture CLI)
cmake --build --preset debug
ctest --preset debug               # or: build-msvc\crystal_tests.exe
.\build-msvc\CrystalDungeons.exe

cmake --preset msvc-release        # -> build-msvc-rel (static CRT, no capture)
cmake --build --preset release
```

Raw Ninja (equivalent to msvc-debug):

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build-msvc
ctest --test-dir build-msvc --output-on-failure
```

`-DCMAKE_*_COMPILER=cl` forces MSVC so a stray compiler on `PATH` can't be picked
up by mistake (that is what broke the first build attempt). First configure
downloads + compiles raylib/Catch2 — slow once, then cached per build dir.
Network is required for the **first** configure only.

CMake options: `-DCRYSTAL_BUILD_TESTS=ON` (default ON),
`-DCRYSTAL_WARNINGS_AS_ERRORS=OFF`, `-DCRYSTAL_ENABLE_DEBUG_OVERLAY=ON`,
`-DCRYSTAL_ENABLE_CAPTURE=ON` (capture CLI; always excluded from Release).

Validation & release tooling (M23/M24):

```powershell
.\build-msvc\CrystalDungeons.exe --capture out\dir   # 23 native-res scenes; fails on text overflow
build-msvc\crystal_tests.exe "[economy-report]" -s   # balance battery table
build-msvc\crystal_tests.exe "[sim-report]" -s       # machine-readable JSON report
powershell -ExecutionPolicy Bypass -File tools\package.ps1  # stage+validate+zip -> dist\
```

Asset generators (deterministic; reruns byte-identical):
`tools\asset_gen\generate_textures.ps1`, `generate_audio.ps1`,
`generate_icon.ps1`. Every asset needs a row in `assets/credits.md`.

## Gotchas (read these before you waste an hour)

1. **MSVC only, and initialize the VS environment first.** `cl` and the bundled
   Ninja are not on a normal shell's `PATH` — without a developer environment,
   configure fails with *"CMake was unable to find a build program corresponding
   to Ninja"*. Verify with `where.exe cl` / `where.exe ninja` before configuring.
   Always pass `-DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl` so a stray MinGW
   `g++` on `PATH` can't be picked up (that broke the very first build). No C++17
   downgrade, no GCC workarounds.
2. **Never include `<windows.h>` in a TU that also sees raylib.** This broke the
   build once already (a fatal-error `MessageBox` added to `main.cpp`). Win32
   `wingdi.h`/`winuser.h` declare `Rectangle`, `DrawText`, and `CloseWindow`,
   which collide with raylib's, and the `min`/`max` macros break `std::max`
   (e.g. in `core/FadeController.hpp`). `WIN32_LEAN_AND_MEAN` does **not** help.
   Put Win32 code in its own `.cpp` under `src/platform/` behind a header that
   exposes only standard types — see `platform/FatalDialog.*` and
   `platform/AtomicFile.cpp`. Also **do not** add an explicit `user32` link:
   that puts `user32.lib` ahead of `raylib.lib` and yields
   `LNK2005: CloseWindow already defined`; MSVC's default libs already cover it,
   in the right order.
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
11. **Screenshots: use the capture tool, not window automation.**
    `CrystalDungeons.exe --capture <dir>` (debug builds) renders 23
    deterministic scenes at native 426×240 and fails on text overflow —
    always prefer it. If live input driving is unavoidable, never use
    focus-dependent `SendKeys` (it leaks keystrokes — it happened); use
    `PostMessage` WM_KEYDOWN/UP to the game's HWND (extended-key bit for
    arrows, ~70ms between down/up) + `SetWindowPos` topmost+NOACTIVATE.
12. **Generation changes need a version bump.** Anything that alters what a
    seed produces (generator code OR composition/data curves) bumps
    `dungeon::kGenerationVersion` (currently 6) — the scoreboard tags it
    for comparability. Owner-gated.

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

Milestones: **M1–M29 `complete (approved)`**; **M30** (economy: paid rest &
rest-token event) `implemented, awaiting manual approval`. The M25–M30 polish
program — font/UI corrections, per-enemy art, environment/ambience identity,
enmity AI + control skills, content expansion + class learnsets, and the paid
inn — is delivered. **M23** (validation/playtesting/balance) and **M24**
(release packaging) are deferred to run **after** M30: their tooling and
packaging are built (v0.9.0 RC in `dist/`), awaiting owner-run external
playtests (`docs/playtest_protocol.md`) and a clean-machine sign-off; version
bumps to 1.0.0 after playtests pass. Details: `docs/milestones.md` + one note
per milestone under `docs/milestone_notes/`.

## Verification checklist (before claiming done)

- [ ] From a VS developer shell: `cmake --preset msvc-debug` configured without
      error.
- [ ] `cmake --build --preset debug` compiled the **executable and the tests**
      with no project warnings (deps may warn). Building only `crystal_tests`
      hides link and `main.cpp` breakage — see gotcha 2.
- [ ] `ctest --preset debug` — all green.
- [ ] Release still builds: `cmake --preset msvc-release` +
      `cmake --build --preset release`. Multi-config gating (`CRYSTAL_CAPTURE`,
      `CRYSTAL_DEBUG_OVERLAY` excluded from Release) only breaks here.
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
