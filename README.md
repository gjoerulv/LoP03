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

- **CMake ≥ 3.20**
- A **C++20** compiler: MSVC (Visual Studio 2022), GCC 11+, or Clang 13+.
  - ⚠️ GCC 8.x (e.g. the compiler bundled with older CodeBlocks) is from 2018,
    predates C++20, and **cannot reliably build this project**. Install a modern
    toolchain — see [Toolchain notes](#toolchain-notes).
- **Internet access on the first configure only** — dependencies are downloaded
  and built once, then cached under `build/_deps`.

Dependencies (fetched and pinned automatically by CMake; nothing to install
manually):

| Library        | Version  |
|----------------|----------|
| raylib         | `6.0`    |
| nlohmann/json  | `v3.12.0`|
| Catch2         | `v3.15.1`|

## Build & run

Pick the generator that matches your toolchain.

### Visual Studio 2022 (MSVC) — recommended on Windows

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\CrystalDungeons.exe
```

### Ninja

```powershell
cmake -S . -B build -G "Ninja"
cmake --build build
ctest --test-dir build --output-on-failure
.\build\CrystalDungeons.exe
```

### MinGW Makefiles (GCC)

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
.\build\CrystalDungeons.exe
```

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

Example: `cmake -S . -B build -G "Ninja" -DCRYSTAL_WARNINGS_AS_ERRORS=ON`

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

## Toolchain notes

If `cmake --build` fails with errors about `-std=c++2a`, `std::filesystem`, or
unsupported C++20 features, your compiler is too old. Options on Windows:

- **Visual Studio 2022 Build Tools** (free): install the *Desktop development
  with C++* workload, then use the `Visual Studio 17 2022` generator above.
- **Modern MinGW-w64**: [w64devkit](https://github.com/skeeto/w64devkit) or
  MSYS2 (`pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake
  mingw-w64-ucrt-x86_64-ninja`), GCC 13+.
- **LLVM/Clang** 13+ with the Ninja generator.

Verify your compiler version with `g++ --version` / `cl`.

## Documentation

- [`docs/game_design.md`](docs/game_design.md) — what the game is and why.
- [`docs/technical_design.md`](docs/technical_design.md) — architecture & conventions.
- [`docs/milestones.md`](docs/milestones.md) — roadmap and status.
- `CLAUDE.md` — the project's operating contract.
