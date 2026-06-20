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
  core/                   # Application, AppContext, GameConfig, Log, Geometry (pure)
  render/                 # VirtualScreen, Viewport (pure), RaylibRAII
  states/                 # GameState, StateStack, concrete states (menu/town/...)
  input/                  # InputAction, InputMap (+ raylib query factory)
  resource/               # ResourceManager (texture/font/sound cache, RAII)
  platform/               # Paths (user-data dir, path sanitizing)
  content/                # JSON content model: defs, enums, loaders, validators
  game/                   # runtime model: Character, Party, Inventory, derivation
  save/                   # SaveSystem: versioned JSON slots + autosave (defensive)
  town/                   # Tilemap, Movement, TownData (pure)
  dungeon/                # Rng, DungeonModel, DungeonGenerator (pure, seeded)
  battle/                 # Battle: deterministic turn-based combat (pure)
  ui/                     # Menu, TextInput (pure) + UiDraw (raylib helpers)
tests/                    # Catch2 unit tests (headless: pure logic + filesystem)
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
stack ordering/transitions, input resolution, path sanitizing, content
validation/loading — and in later milestones danger, scoring, generation, save
round-trips. Anything needing raylib is validated by running the app
(human-in-the-loop where visual/feel is involved). Tests get the shipped content
path via the `CRYSTAL_TEST_DATA_DIR` compile definition so they validate the
real JSON.

## 7. Content data model (Milestone 2)

Content lives in JSON under `data/` and is loaded by `src/content/` (raylib-free,
so it is unit-testable headlessly). At startup `Application` loads it into a
`ContentDatabase` stored in `AppContext`; CMake copies `data/` next to the exe.

### File format

Each file is a versioned wrapper around a named array:

```json
{ "version": 1, "skills": [ { "id": "strike", "name": "Strike", ... } ] }
```

`version` must equal the supported schema version (currently `1`). Files:
`skills.json`, `classes.json`, `enemies.json`, `items.json`. (`bosses.json` and
`dungeon_themes.json` arrive with their owning milestones — M7 and M4.)

| Type   | Required fields | Notable optional fields |
|--------|-----------------|-------------------------|
| skill  | `id`, `name`, `category`, `target` | `element`, `power`≥0, `mpCost`≥0, `description` |
| class  | `id`, `name`, `baseStats`{hp≥1,attack,magic,defense,speed} | `role`, `growth`{floats}, `startingSkills`[skill ids] |
| enemy  | `id`, `name`, `stats`{…} | `tier`(normal/elite), `tags`[fast/magic/armored/poison], `skills`[ids], `xpReward`, `goldReward` |
| item   | `id`, `name`, `type`(consumable/equipment/relic/scroll) | `slot`, `rarity`, `value`, `effect`, `effectAmount`, `statBonus`, `grantsSkill`, `description` |

Enums (string values): `Element`, `SkillCategory`, `SkillTarget`, `EnemyTag`,
`EnemyTier`, `ItemType`, `EquipSlot`, `Rarity`, `ConsumableEffect`.

### Validation rules

- Loading **never throws to the caller and never crashes**; problems are
  appended to a `LoadReport` (`{source, context, message}`), and the game runs
  in a degraded state if content is missing.
- Per field: presence, JSON type, numeric range, and known enum value.
- Per entry: a bad entry is skipped (its errors reported) but does not discard
  the valid entries around it. Duplicate ids are rejected.
- Semantic rules: scrolls must set `grantsSkill`; equipment/relics need a real
  `slot`.
- Cross-references: skill ids in `startingSkills`, enemy `skills`, and scroll
  `grantsSkill` must exist.
- Bumping `version` or changing field meaning is a **public-schema change** —
  requires human approval (see `CLAUDE.md`).

## 8. Town, party, and saves (Milestone 3)

### Flow & states

`MainMenuState` (New Game / Continue / Quit) → `PartyCreationState` →
`TownState`. Locations push sub-states (`InnState`, `SlotMenuState`,
`PlaceholderLocationState`); `TownMenuState` is a transparent pause overlay.
`AppContext` now also carries `save::SaveSystem& saves` and the active
`Party& party`.

### Town (walkable)

A fixed single-screen **26×15** tilemap (16px tiles) built by `town::buildTown()`.
`Tilemap` answers solidity and pixel-rect collision; `town::resolveMove` does
axis-separated collision (natural wall-sliding). Both are pure and unit-tested.
Seven `Building`s each expose a walkable **Door** interact tile; standing on one
and pressing Confirm enters that location. Rendering uses code-generated colored
tiles (placeholder, no external art).

### Party model

`Character` persists `classId/name/level/xp` and current `hp/mp`; `stats`,
`maxHp`, `maxMp` are **derived** from the `ClassDef` (base + per-level growth,
truncated for determinism) on creation/load, so they never go stale. MP is a
provisional `magic`-scaled pool until the combat milestone.

### Save format & rules

Versioned JSON (`version` must equal `kSaveVersion`, currently `1`) under the
user data dir (`%APPDATA%/CrystalDungeons/saves` on Windows). **Slots:** three
manual (`save_slot1..3.json`) plus an autosave (`save_auto.json`).

```json
{ "version": 1, "gold": 150,
  "party": [ { "classId": "knight", "name": "Rolan", "level": 1, "xp": 0, "hp": 120, "mp": 4 } ] }
```

- Loading reuses the M2 validator (`ObjectReader`/`LoadReport`): malformed,
  foreign-version, or unknown-class saves are reported and **leave the target
  party untouched** — never a crash, never a partial load.
- Manual saves happen at the town Save Point. **Autosave** writes on dungeon
  entry (trigger wired in M4); loading any slot — including autosave — always
  starts the party in **town**, never inside a dungeon (no save-scumming a run).
- Save-format changes require human approval (see `CLAUDE.md`).

## 9. Dungeon generation (Milestone 4)

`dungeon/` is pure (no raylib) and seeded, so generation is fully deterministic
and unit-tested. `dungeon::generate(seed, depth, db)` returns a `Dungeon`:

- **Layout:** a randomized-DFS simple **main path** from a Start room to a Boss
  room on a 7×7 grid; dead-end **side rooms** branch off path rooms and hold
  chests. Rooms connect via `Door`s (N/E/S/W) referencing neighbor indices.
- **Gates:** at least **3** path transitions are marked `gated` and assigned an
  `EnemyTeam`. Because the path is the unique route to the boss and side rooms are
  dead-ends, the boss is reachable **only** by passing every gate (a tested
  invariant: BFS ignoring gated doors cannot reach the boss).
- **Teams & chests:** enemy teams are composed from the content database (normal/
  elite by depth) with aggregated tags and an original generated name; chest
  rewards (gold + optional item) come from the item data. At least one chest is
  guarded. The `Rng` is xorshift64*, so the same seed yields an identical dungeon
  on every platform.

**Determinism rule:** to keep generation reproducible, iterate the (unordered)
content DB into **sorted** id pools before any RNG-driven selection.

### Walkable presentation & flow

`DungeonState` renders one room at a time as a single-screen tilemap (reusing
`town::Tilemap`/`resolveMove`): walls with door gaps, walk room-to-room. **Gated
doors are solid** (blocked by a team marker) until the battle milestone, so M4
exercises generation, exploration, team/chest inspection, and the retreat flow —
not combat. Entry: Guild → seed → autosave → dungeon; pause → Retreat to town.
Danger tiers and dungeon score are intentionally absent until M6 (no
hand-authored danger).

## 10. Combat (Milestone 5)

`battle/` is pure and **deterministic** (no randomness), so battles are fully
reproducible and unit-tested; `BattleState` is the side-view UI driving it.

- **Model:** `Battle` holds `Combatant`s (built from party `Character`s and the
  encounter's `EnemyTeam`). `turnOrder` sorts living units by speed (desc),
  tie-broken party-first then index, recomputed each round. Guard is cleared at
  the start of a unit's turn.
- **Resolution (deterministic formulas):** physical `max(1, attack + power −
  def/2)`, magic `max(1, magic + power − def/4)`, both halved while guarding;
  heal `power + magic/2` (never resurrects); items apply Heal/RestoreMp/Revive/
  Cure; revive restores a percentage of max HP. Skills spend MP. The enemy AI is
  deterministic (heal a hurt ally if able, else damage the lowest-HP party
  member). Variance/crits and status ailments are deferred to M7/M9.
- **Commands:** Attack, Skill, Item, Guard, Escape. Escape always succeeds (every
  encounter is escapable); escaping forfeits the gate/chest.
- **Inventory:** `game/Inventory` (item id → count, insertion-ordered). Persisted
  as an **optional** `inventory` array in the save — old saves without it still
  load (empty), so the save version stays `1`.
- **Dungeon loop:** facing a team and pressing Confirm starts a battle whose
  `Outcome` is written to a slot the `DungeonState` reads on resume. Victory
  un-gates the door / removes a chest guard; a boss victory ends the run
  (return to town); defeat triggers game over (heal, lose half gold, return to
  town); escape leaves the gate intact. Dungeon **score** for these outcomes is
  M6.
