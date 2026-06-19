# Crystal Dungeons — Technical Design

> Living document. Update when architecture, conventions, or build change.

## 1. Stack

- **C++20**, **CMake** (out-of-source builds).
- **raylib `6.0`**, **Catch2 `v3.15.1`**, **nlohmann/json `v3.12.0`**, all via
  **CMake FetchContent** with pinned tags (no floating branches, no vcpkg/system).
- Windows is the primary target; keep platform-specific code isolated.

### Compiler baseline

Built with **MSVC / C++20** (Visual Studio 2022 or newer) as the supported
toolchain — see the README for the exact build routine. Code avoids
bleeding-edge features (`<format>`, modules, heavy ranges/concepts) and stays
free of Windows-only APIs, so it remains portable to other modern C++20
compilers (recent GCC/Clang) should a non-Windows target ever be added — but
**MSVC is the only supported build path**. MinGW and GCC 8.x toolchains are not
supported and must not be used. `std::filesystem`, `std::optional`,
`std::string_view`, structured bindings, etc. are used freely.

## 2. Repository layout

```
CLAUDE.md                 # operating contract
README.md                 # human build/run instructions
CMakeLists.txt            # root build
cmake/                    # Dependencies.cmake, CompilerWarnings.cmake
docs/                     # design + technical + milestones (source of truth)
data/                     # JSON content (populated M2+)
assets/                   # textures/audio/fonts placeholders (M8); .gitkeep for now
src/
  main.cpp
  core/                   # Application, GameConfig, Log
  render/                 # VirtualScreen, Viewport (pure), RaylibRAII
  states/                 # GameState, StateStack, concrete states
  input/                  # InputAction, InputMap (+ raylib query factory)
  resource/               # ResourceManager (texture/font/sound cache, RAII)
  platform/               # Paths (user-data dir, path sanitizing)
tests/                    # Catch2 unit tests (headless: pure logic only)
.claude/skills/crystal-dungeons/SKILL.md
```

One responsibility per file; prefer small cohesive files over monoliths.

## 3. Architecture boundaries

Keep these separable and one-directional where possible:

- **Simulation** (game logic) — no raylib calls; testable.
- **Rendering** — raylib draw calls; reads simulation state.
- **Input** — hardware → game *actions* (`InputAction`); gameplay never reads raw
  keys. Keyboard + gamepad both bound.
- **Resources** — RAII-wrapped, cached textures/fonts/sounds; graceful failure.
- **Data/content** (M2+) — JSON load + validate; never trust input.
- **Save/load** (M3+) — versioned JSON; defensive.
- **UI / battle / dungeon-gen** — added in later milestones.

### State management

`StateStack` owns a vector of `GameState` (unique_ptr). States can be transparent
(let the state below render and/or update). Transitions (push/pop/replace/clear)
are **queued** and applied between frames so a state never mutates the stack
mid-iteration. The app exits when the stack empties or a state requests quit.

### Rendering pipeline

1. `BeginTextureMode(virtualScreen)` → clear → `stack.render()` draws in
   **426×240** logical space → `EndTextureMode`.
2. `BeginDrawing` → clear letterbox black → blit the render texture scaled to the
   computed viewport (aspect-preserved, nearest-neighbor; source height negative
   to correct y-flip) → optional debug overlay → `EndDrawing`.

`Viewport::compute(windowW, windowH, internalW, internalH)` is a **pure
function** (no raylib) returning scale + destination rect, and is unit-tested.

### Input layer

`InputAction` is the gameplay-facing enum. `InputMap` holds key/gamepad bindings
per action. Resolution is **pure**: `InputMap::isDown/isPressed/isReleased(action,
const InputQuery&)`, where `InputQuery` is a set of callbacks. Production uses
`makeRaylibInputQuery()` (wraps raylib polling); tests inject fakes. The `Input`
facade (owned by `Application`) refreshes the query each frame and is passed to
states as `const Input&`.

### Resources

`ResourceManager` caches by key and owns raylib handles via RAII wrappers
(`RaylibRAII.hpp`). Missing/failed loads log a warning and return a generated
**placeholder** (e.g., magenta checkerboard texture) — never crash. Requires an
initialized window, so it is exercised at runtime, not in unit tests.

### Paths & safety

`paths::userDataDir()` resolves a per-user writable dir (`%APPDATA%/CrystalDungeons`
on Windows, `$XDG_DATA_HOME`/`~/.local/share` fallback elsewhere) using env vars
only — **no shell execution**. `paths::sanitizeRelative()` rejects absolute paths,
drive letters, and `..` traversal; all data/save file access goes through it.

## 4. Conventions

- Namespaces: project code under `cd::` (sub-namespaces optional, e.g.
  `cd::input`). Free functions for pure helpers.
- Ownership: `std::unique_ptr` for owned heap objects; references/`*` for
  non-owning views; **no raw owning pointers**.
- Error handling: validate inputs, return `std::optional`/status + readable log;
  reserve exceptions for truly exceptional/programmer errors. Never crash on bad
  external data.
- Determinism: dungeon gen, danger, and scoring take explicit seeds/inputs and
  are unit-tested.
- No per-frame heap allocation in hot loops; reuse buffers.
- Warnings: high (`/W4` MSVC, `-Wall -Wextra -Wpedantic` GCC/Clang) on **project
  targets only**; `-DCRYSTAL_WARNINGS_AS_ERRORS=ON` available, default OFF.

## 5. Build options

| Option                          | Default        | Effect                              |
|---------------------------------|----------------|-------------------------------------|
| `CRYSTAL_BUILD_TESTS`           | ON             | Build Catch2 tests + register CTest |
| `CRYSTAL_WARNINGS_AS_ERRORS`    | OFF            | Treat project warnings as errors    |
| `CRYSTAL_ENABLE_DEBUG_OVERLAY`  | ON (Debug)     | Compile the runtime debug overlay   |

## 6. Testing strategy

Unit tests cover **pure** logic only (no window/GPU/audio): viewport math, state
stack ordering/transitions, input resolution, path sanitizing — and in later
milestones danger, scoring, generation, JSON validators, save round-trips.
Anything needing raylib is validated by running the app (human-in-the-loop where
visual/feel is involved).
