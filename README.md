# Crystal Dungeons

A 16-bit-inspired, turn-based **JRPG roguelite** about clearing seeded dungeons
**efficiently**. Take a party of four from a town hub into procedurally generated
dungeons full of *visible* enemy teams and guarded chests, beat the boss, and
score on how few battle turns you spent. Original work — not a clone of any
existing game.

> **Status: Milestone 1 (Project Foundation).** This builds a window with a
> 426×240 virtual screen, a state stack, an input-mapping layer (keyboard +
> gamepad), and a resource-manager foundation. Gameplay arrives in later
> milestones. See [`docs/milestones.md`](docs/milestones.md).

## Requirements

- **Visual Studio 2022 or newer** with the **"Desktop development with C++"**
  workload (provides the MSVC compiler plus a bundled CMake and Ninja). This
  project is built with **MSVC / C++20**; MinGW and other GCC toolchains are not
  supported.
- **CMake ≥ 3.20** (the Visual Studio–bundled CMake is fine).
- **Internet access on the first configure only** — dependencies are downloaded
  and built once, then cached under `build-msvc/_deps`.

Dependencies (fetched and pinned automatically by CMake; nothing to install
manually):

| Library        | Version  |
|----------------|----------|
| raylib         | `6.0`    |
| nlohmann/json  | `v3.12.0`|
| Catch2         | `v3.15.1`|

## Build & run

Build from the **Visual Studio developer environment** so the MSVC compiler
(`cl`) and the bundled CMake/Ninja are on `PATH`. Either open a **"Developer
PowerShell for VS"** / **"Developer Command Prompt for VS"**, or run
`vcvars64.bat` in a normal shell first (adjust the edition in the path):

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

### Ninja (recommended)

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build-msvc
ctest --test-dir build-msvc --output-on-failure
.\build-msvc\CrystalDungeons.exe
```

`-DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl` forces MSVC, so no other compiler
that happens to be on `PATH` can be picked up by mistake.

### Visual Studio generator (alternative)

Use the generator that matches your installed Visual Studio (e.g.
`"Visual Studio 17 2022"`, or a newer version):

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Debug
ctest --test-dir build-msvc -C Debug --output-on-failure
.\build-msvc\Debug\CrystalDungeons.exe
```

The first configure compiles the dependencies once (slower), then caches them in
`build-msvc/_deps` for fast subsequent builds.

## Controls (Milestone 1 demo)

| Action            | Keyboard            | Gamepad            |
|-------------------|---------------------|--------------------|
| Open menu overlay | `Enter` / `Space` / `Tab` | A / Start    |
| Back / resume     | `Esc` / `Backspace` | B / Start          |
| Quit (from title) | `Esc`               | B                  |
| Toggle debug overlay | `F1`             | —                  |
| Move (reserved)   | Arrows / `WASD`     | D-pad              |

The window is resizable; the 426×240 image always scales to fit with
aspect-preserving letterbox/pillarbox bars.

## Build options

| Option                          | Default | Effect                               |
|---------------------------------|---------|--------------------------------------|
| `CRYSTAL_BUILD_TESTS`           | `ON`    | Build the Catch2 tests + register CTest |
| `CRYSTAL_WARNINGS_AS_ERRORS`    | `OFF`   | Treat project warnings as errors     |
| `CRYSTAL_ENABLE_DEBUG_OVERLAY`  | `ON`    | Compile the runtime debug overlay (F1) |

Example: `cmake -S . -B build-msvc -G Ninja -DCMAKE_CXX_COMPILER=cl -DCRYSTAL_WARNINGS_AS_ERRORS=ON`

## Project layout

```
src/
  core/      Application, config, logging, shared context
  render/    Viewport (pure math), VirtualScreen, raylib RAII wrappers
  states/    GameState interface, StateStack, demo states
  input/     InputAction, InputMap (pure resolution), raylib query, Input facade
  resource/  ResourceManager (cached, RAII, graceful fallback)
  platform/  Paths (user data dir, path sanitizing)
tests/       Catch2 unit tests (headless: pure logic only)
data/        JSON content (Milestone 2+)
assets/      textures/audio/fonts (later milestones)
docs/        design + technical + milestone docs (source of truth)
```

## Documentation

- [`docs/game_design.md`](docs/game_design.md) — what the game is and why.
- [`docs/technical_design.md`](docs/technical_design.md) — architecture & conventions.
- [`docs/milestones.md`](docs/milestones.md) — roadmap and status.
- `CLAUDE.md` — the project's operating contract.
