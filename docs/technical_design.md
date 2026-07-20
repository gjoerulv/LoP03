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
assets/                   # manifest.json + generated textures/audio + credits.md
src/
  main.cpp
  core/                   # Application, AppContext, GameConfig, Log, Geometry, FadeController
  audio/                  # AudioManager + AudioRoles: manifest-driven audio, synth fallback
  render/                 # VirtualScreen, Viewport (pure), RaylibRAII
  states/                 # GameState, StateStack, concrete states (menu/town/...)
  input/                  # InputAction, InputMap (+ raylib query factory)
  resource/               # ResourceManager (texture/font/sound cache, RAII)
  platform/            # Paths + AtomicFile (user-data dir, safe persistence)
  content/                # JSON content model: defs, enums, loaders, validators
  game/                   # runtime model: Character, Party, Inventory, derivation
  save/                   # SaveSystem: versioned JSON slots + autosave (defensive)
  town/                   # Tilemap, Movement, TownData (pure)
  dungeon/                # Rng, DungeonModel, DungeonGenerator (pure, seeded)
  battle/                 # Battle: deterministic turn-based combat (pure)
  danger/                 # DangerRating: stat-derived danger tiers (pure)
  score/                  # Scoring + persistent Scoreboard
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

`InputAction` is the gameplay-facing enum (with stable serialization names and
display names). `InputMap` holds key/gamepad bindings per action; `InputQuery`
is a set of injected callbacks (keys, buttons, axes, text codepoints, pressed-
key queue) so everything below tests headlessly with fakes; production uses
`makeRaylibInputQuery()`.

The `Input` facade (owned by `Application`) computes a per-frame action model
in `update(dt)` (M13): unified key/button/left-stick down states (stick with
0.5-enter/0.35-release hysteresis), its own press/release edges, hold-repeat
navigation (`navPressed`, 0.35s delay / 90ms interval — menus use it,
Confirm/Cancel never repeat), suppression of held input across state
transitions and remap listens (`suppressUntilRelease`, triggered by
`Application` on stack-top changes), and active-device tracking that drives
prompt labels. Supporting pure modules: `input/PromptLabels` (binding →
"[Tab] Pause"-style labels; all prompts and the generated Controls page use
them), `input/Remap` (swap-with-donor conflict policy, Esc reserved,
never-unbound invariants).

### Settings (M13)

`settings::SettingsStore` persists a versioned `settings.json` (v1) in the
user data dir: master/music/SFX volumes (applied via
`AudioManager::setVolumes`), borderless-window mode (applied self-healing per
frame), battle speed (scales the battle resolve pause), message speed (scales
transient message durations), and both devices' remappable bindings. Loading
is defensive (ObjectReader/LoadReport): missing file = silent defaults;
malformed/foreign-version = reported defaults; a binding set that would
strand the keyboard restores that action's defaults. Parse/serialize are
string-based and headless-tested.

### Resources & the asset manifest (M14)

Presentation assets resolve through `assets/manifest.json` (schema v1
owner-approved at M14; v2 animation entries owner-approved at M17; the loader
accepts both. Parsing/validation is raylib-free in `src/assets/AssetManifest`
and headless-tested, including a test that validates the shipped manifest).
States request **stable logical IDs** — never file paths. `ResourceManager`
caches by id and owns raylib handles via RAII wrappers (`RaylibRAII.hpp`);
unknown ids / missing files / failed loads log a warning and return a
generated **placeholder** (magenta checker) or the default font — never crash.
`AudioManager` resolves its fixed roles (`sfx.*`, `music.*`, `ambience.*`;
tables in `src/audio/AudioRoles.hpp` since M21) against the same catalog:
manifest file → synthesized tone → silence (owner-approved order);
file music uses raylib music streams. Reload model (owner-approved): callers
cache nothing and re-fetch by id; the debug-only F5 `ReloadAssets` action
re-resolves the catalog and restarts the current track. CMake copies `assets/`
beside the executable like `data/`; provenance lives in `assets/credits.md`.
Authoring guide: `docs/asset_pipeline.md`.

M15 ships the first real catalog content: deterministic generators under
`tools/asset_gen/` produce the slice's tiles/sprites/UI/music per
`docs/art_bible.md`. Render paths key off the catalog per element (town and
themed dungeon tiles via `tiles.<theme>.*` — `Dungeon.themeId` carries the
content id — player/battle sprites with specific-id-then-generic lookup,
`ui::drawFramedPanel` nine-patch) and keep the pre-asset colored-shape
drawing as the fallback for roles without art, so themes and enemies gain
art incrementally without code changes.

### Animation & exploration presentation (M17)

Manifest v2 adds `"animation"` entries: a horizontal frame strip inside a
catalog texture (`texture`, `row`, `frameCount`, `frameWidth`/`Height`,
`frameTime`, `loop`). Validation drops malformed entries and any animation
whose strip texture is missing (`dropDanglingAnimations`, re-run after file
checks); a shipped-manifest test also parses PNG headers to prove every
strip fits its texture. Playback is pure and allocation-free
(`render/Animation.hpp`: `frameAt` wraps or holds, `frameRect` indexes the
strip); states keep their own clocks (walk clock resets while standing so
frame 0 is the stand pose; a free-running world clock drives indicator
pulses). `render/SpriteDraw` draws animation frames or static textures
**centered on the collision-rect center**, so visual bounds are independent
of collision bounds (art can change size freely; collision never reads
texture dimensions).

Draw-order rule (town + dungeon): tiles (with deterministic sparse accent
variants) → world sprites (markers, then the player) → overlay pass (danger
labels, the pulsing facing-brackets indicator) → HUD/minimap/footer. World
sprites stay tile-bounded, overlays always draw above them — no depth
sorting is needed or performed. Team markers select a silhouette by
stat-derived danger tier (normal / horned elite for Dangerous+Deadly /
crowned boss), with the M15 prop sprites and colored glyph rectangles as
fallbacks; the tier is also always shown as the text label, so shape+text
carry the information without color.

### Composition, events & boss mechanics (M20)

- **Roles & constraints:** `EnemyDef.role` is required content
  (`enemies.json`); `data/composition.json` (v1, validated with repair-or-
  report semantics, defaults = the shipped curves) externalizes team-size
  and elite-chance depth curves, the support cap, the damage minimum, boss
  minion bounds, and depth stat scaling. `makeTeam` fills slots from
  constraint-filtered candidate lists (sorted pools, so composition is
  deterministic per seed); an elite pool that cannot satisfy a rule falls
  back to the normal pool rather than violating it. `EnemyTeam.statScalePct`
  carries the depth multiplier into both `buildBattle` and
  `danger::teamThreat`, so displayed danger always matches the fight.
  `kGenerationVersion` = 4.
- **Events:** `RoomType::Event` dead-end side rooms (2–3 per dungeon,
  kinds unique per dungeon) carry a `RoomEvent`
  (shrine/spring/merchant/challenge/wager) realized as an `EventChamber`
  layout archetype with a solid event anchor; unguarded chests may be
  `trapped` (extra gold, party wounds on claim). `DungeonState` shows each
  event's full trade-off in the footer before Confirm and applies exactly
  what it stated; elite challenges run through the normal battle flow with
  an `EncounterKind::Challenge` that credits danger twice. The wager flows
  through `RunSummary.wagerAccepted` → `ScoreBreakdown.wager` (+150
  no-death / −100 otherwise, completed runs only) and the result panel
  grows a line for it.
- **Boss mechanics** (deterministic, in the pure sim, mirrored identically
  by play and simulator because both share `tickStatuses` at turn start):
  Brute enrage ×1.5 below half HP with a once-only announcement; Sorcerer
  +25% magic per fallen ally (logged as "empowered"); Commander revives
  fallen minions at half HP once, below half HP; Rush doubles its first
  action's damage. Enemy AI also casts Support skills whose status the
  target lacks — making buffer/debuffer enemies (and Commander kits) real.

### Presets, versioning & packaging (M24)

`CMakePresets.json` defines `msvc-debug` (dev: debug overlay + capture
CLI) and `msvc-release` (shipping: `CMAKE_MSVC_RUNTIME_LIBRARY=
MultiThreaded` with `CMP0091` forced NEW so raylib matches — the exe's
import table carries only OS DLLs and runs without the VC++
redistributable; verified via dumpbin). The version lives once in
`project(VERSION)` and is configured into `generated/core/Version.hpp`
(title-screen stamp) and `packaging/CrystalDungeons.rc.in` (Windows
VERSIONINFO + icon; the multi-size `packaging/crystal.ico` is generated
from the approved emblem by `tools/asset_gen/generate_icon.ps1`).
`tools/package.ps1` is the one-command release path: preset build →
stage exe + `data/` + `assets/` + player README + LICENSES → validate
(required files, every manifest path resolves inside the package, no
debug artifacts, capture strings absent from the exe, exe
ProductVersion matches) → zip `dist/CrystalDungeons-<version>-win64.zip`.
Release builds cap raylib's log level at warnings (`NDEBUG`). User data
stays in `paths::userDataDir()` for dev and packaged builds alike.

### Validation tooling (M23)

Three layers, all deterministic. **Capture:** `CrystalDungeons --capture
<outdir>` (compiled only when `CRYSTAL_ENABLE_CAPTURE` is ON and the build
is not Release) renders 22 scenario states — every screen family, all
three themes, five-enemy and boss battles, worst-case 12-char names,
maximal score breakdowns, the tutorial/Details overlays, High Contrast —
to the real 426×240 virtual screen in a hidden window, exports native-res
PNGs (`VirtualScreen::exportImage`), and **fails (nonzero exit) if any
scene raises a text-overflow diagnostic** (`ui::overflowEvents()`, a
running counter behind the M12 fitted/wrapped helpers). All mutable state
lives in a scratch temp dir; player data is never touched; reruns are
byte-identical (hash-verified). **Lint:** `tests/test_presentation_lint.cpp`
resolves every convention-derived texture id (per-theme tiles, per-class
battle actors, tier fallbacks, markers, props) against the shipped
manifest, checks name/description/telegraph budgets with a conservative
measure (it caught two over-budget boss telegraphs on first run), and
validates 2000+ generated rooms against the full M16 invariant set.
**Reports:** `crystal_tests "[sim-report]" -s` emits reproducible JSON —
progression bands per depth and outlier encounters (teams needing > 2× the
median rounds at the dungeon's clearing level). The M23 tuning
(composition.json elite pressure; `kGenerationVersion` 4) moved the
clearing curve from 1/1/1/1/3/5/9/11 to 1/1/1/2/3/7/9/11 (depths
1/2/3/4/5/6/8/10).

### Onboarding & accessibility (M22)

Tutorial: `src/tutorial/Tutorial.*` is raylib-free — a constexpr beat table
(9 beats, stable ids), `Progress {enabled, seen}` with defensive
parse/serialize (malformed or foreign-version `tutorial.json` → fresh
state, reported, never a crash; unknown seen ids survive round trips), and
`TutorialStore::takeBeat` which marks-and-saves on first fire so a prompt
shows at most once even across crashes. `TutorialPromptState` and the M22
`DetailsOverlayState` are transparent overlay states (`rendersBelow()`
only): the scene below freezes and dims, one Confirm dismisses. Beats are
triggered from `onEnter`/`onResume`/`update` — never constructors, whose
queued pushes would order the overlay underneath the state being built.
`Details` is a new remappable `InputAction` (keyboard C, gamepad Y;
additive to the bindings schema — old settings files keep the default).
The UI palette moved behind `style::palette()` (see `ui_style_guide.md`
§4): standard table byte-identical to the old constants, high-contrast
table selected by `settings.highContrast` (optional field, absent = off).
Save-overwrite and quit-to-title arm on first Confirm and execute on the
second. Tests: `tests/test_tutorial.cpp` (round-trip, malformed, take-once
store semantics on a temp file, beat-text wrap lint with a conservative
measure) and `tests/test_accessibility.cpp` (palette equivalence/switch,
settings round-trip, Details action schema + defaults).

### Audio architecture (M21)

The full soundscape ships through the M14 catalog — 30 original WAVs (11
music, 4 ambience, 15 SFX) produced by the deterministic
`tools/asset_gen/generate_audio.ps1` (byte-identical reruns; provenance in
`assets/credits.md`). The stable contract is `src/audio/AudioRoles.hpp`
(raylib-free): `Sfx`/`MusicTrack`/`AmbienceTrack` enums, role-id tables,
the synth-fallback map, per-role SFX rate-limit intervals, and the fade
curve — all headless-tested against the shipped manifest
(`tests/test_audio.cpp`, including RIFF-header validation of every file).
`AudioManager` gains a second streamed channel for ambience (file-or-
silence, governed by the music volume slider), a 0.25 s crossfade on music
changes, one-shot victory/defeat jingles (`loop:false`; when the file is
missing the matching stinger SFX plays so battle end is never silent), and
rate limiting so held movement keys yield a step cadence and rapid menu
scrolling stays clean. Scene mapping: title/town/guild tracks, per-theme
dungeon music + ambience (owner decision; `themeMusic`/`themeAmbience` in
`DungeonState`), boss battles use `music.boss` via the existing
`bossBattle_` flag, and the result screen plays `music.result`. Battle SFX
are typed at the presentation layer only (`stageNumbers` picks
physical/magic/status codes from the resolved `SkillDef`) — the sim is
untouched. No gameplay information is sound-only; the muted game plays
identically.

### Score comparability & economy audit (M19)

`ScoreEntry` carries two optional comparability tags, both loaded with
absent-defaults and written without a scoreboard format bump:
`generationVersion` (M16) and `partyLevel` (M19; highest party level at
completion, 0 = legacy shown as "-"). Ranking (`ranksAbove`) never reads
them — they make comparison conditions visible instead of normalizing.
The economy evidence battery lives in `tests/test_economy.cpp`: pure
score-incentive guards (fewer turns always better; stalling and escapes
always cost; unfinished runs score 0 regardless of loot) plus data-driven
sims (depth-by-level clearing curve, income vs Training Hall costs, class
identity at levels 1–50). An on-demand analysis table prints via
`crystal_tests "[economy-report]" -s`. The M19 audit found no exploit
loops and left all data values unchanged; the depth>6 difficulty plateau
it exposed is deferred to M20's composition constraints (changing team
composition would change published seed meaning).

### Battle presentation sequencer (M18)

The simulation stays synchronous and deterministic; `render/BattleSequencer`
(pure, time-injected, headless-tested) only paces how an already-final
result becomes visible: **Windup** (acting unit leans toward the foe) →
**Impact** (hit units brighten, small screen shake, floating numbers +
SFX appear, and the *displayed* HP commits) → **Settle** (the message pause,
`resolveSeconds`). `BattleState` keeps a `displayHp_` mirror so bars, KO
tags, and enemy fade-outs update at the impact beat rather than instantly;
fallen enemies sink away over 0.4s while party members stay visible for
revives. Battle speed scales the staging (Fast halves it; Instant skips
windup/impact entirely) and Confirm always skips the whole sequence — the
commit still fires exactly once, so no information is ever lost by
skipping. `effectFlash`/`effectShake` (M18 settings, full/reduced/off)
gate the impact effects; flash is a decaying brighten pulse, never a
strobe. The sim/presentation boundary is enforced by tests: every
pre-existing battle and simulator test passes unmodified, and the
sequencer's own tests cover stage ordering, speed scaling, skip semantics,
one-shot commits, and settings honoring.

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
`skills.json`, `classes.json`, `enemies.json`, `items.json`, `bosses.json`,
`dungeon_themes.json`. Bosses carry an `archetype`, `skills`, `minions`, and a
`telegraph`; themes list `normalEnemies`/`eliteEnemies`/`bosses` id pools; skills
may carry an optional `statusEffect`/`statusMagnitude`/`statusDuration`. All ids
are reference-checked across files.

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

`MainMenuState` (New Game / Continue / Controls / Quit) → `PartyCreationState` →
`TownState`. Town locations push real sub-states (`InnState`, `ItemShopState`,
`EquipShopState`, `TrainingHallState`, `GuildState`, `ScoreboardState`,
`SlotMenuState`); `TownMenuState` is a transparent pause overlay.
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

### Compact room realization (Milestone 16)

Topology and room realization are separate systems. `dungeon::generate` keeps
deciding connectivity, gates, teams, chests, and the boss exactly as above;
`dungeon/RoomLayout` (pure, unit-tested) turns each graph room into a compact
layout:

- **Archetypes** classified from immutable topology facts (`classifyRoom`):
  Entry, Corridor, Crossroads, GateChamber, TreasureAlcove, TreasureVault,
  BossAntechamber, BossArena. Owner-approved dimension ranges: common
  chambers 9–15 × 7–11 tiles, corridors 9–13 long × 5–7 wide (long axis along
  the door pair), boss arena 15–19 × 11–13; global bounds 5..19 × 5..13.
- **Derived room-local seeds:** `roomLocalSeed(dungeonSeed,
  kGenerationVersion, roomIndex, archetype)` (splitmix64-style mixing) feeds
  a per-room `Rng`. Realization **never draws from the topology RNG**, so
  presentation changes cannot alter what a published seed means.
  `kGenerationVersion` (currently 4; 1 = the pre-M16 fixed 26×15 rooms) is
  folded into the hash and recorded on new score entries as an optional
  `generationVersion` field — no scoreboard format bump; absent = pre-M16
  (owner decision 2026-07-19).
- **Layout contents:** closed border walls, centered two-tile door gaps per
  wall (`doorGap`/`interiorGap`), sparse landmark pillars filtered by a
  keep-clear mask (door lanes, anchors, spawns), and anchors for chest,
  chest guard, and boss. RNG draw order depends only on immutable topology,
  so a cleared gate or defeated guard never reshapes a room.
- **Validation:** `validateLayout` BFS-checks every configuration a player
  can experience (fully open, pristine gates-closed, and each cleared-gate
  entry): doors, chest, and spawns reachable; gate/guard/boss blocks
  face-able. Mass tests validate thousands of rooms across seeds, depths,
  and themes; a safety net drops obstacles rather than ship an invalid room.

### Walkable presentation & flow

`DungeonState` realizes all layouts once at dungeon entry (from pristine
state) and composes each room's `town::Tilemap` from the layout base plus
live overlays (door gaps carved unless gated; gate/guard/boss marker blocks
while their encounters stand). Rooms draw centered in the exploration
viewport (origin offset; render-only — movement and interaction stay in
room-local space, reusing `resolveMove`). Entering through a door spawns you
on that door's interior tile; the start room uses the layout's center spawn.
Entry: Guild → seed → autosave → dungeon; pause → Retreat to town.

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
  town); escape leaves the gate intact.

## 11. Danger & scoring (Milestone 6)

Both modules are pure and unit-tested.

### Danger (`danger/`)

`teamThreat(team, db)` = per-enemy stat threat (weighted HP/Attack/Magic/Defense/
Speed) + per-skill threat (damaging skills weighted full, support/heal half),
scaled by a synergy factor (more enemies and a healer raise it). `tierFor(threat,
depth, isBoss)` compares the threat to a depth baseline (`50 + 25·(depth−1)`) and
returns **Trivial/Easy/Fair/Dangerous/Deadly**; boss teams are always **Boss**.
The tier is therefore derived solely from stats and abilities — **never
hand-authored** — and is shown on the map (colored label) and the fight prompt.
Weights/thresholds are explicit constants, tuned in M9.

### Scoring (`score/`)

A run is summarized as `RunSummary` (completed, battleTurns = total rounds,
dangerDefeated, chestsOpened, treasureGold, noDeath, escapes). `scoreBreakdown`
yields each component and the clamped total: base + boss − **per-round turn
penalty** + chest + danger-defeated + treasure + no-death − escape. An
**unfinished run scores 0**, and the turn penalty is large relative to the
bonuses so *fewest battle turns* dominates — and because each team/chest is
finite, there is nothing to farm. The `Scoreboard` persists ranked entries as
versioned JSON in the user data dir (defensive load; missing file = empty board).

### Run flow

`BattleState` reports a `BattleResult` (outcome, rounds, party-KO). `DungeonState`
accumulates run stats across battles; a boss victory computes the score, records
a `ScoreEntry`, and shows `DungeonResultState` before returning to town. The town
`ScoreboardState` lists the board.

## 12. Content, status & equipment (Milestone 7)

### Status effects (`battle/`)

A `Combatant` carries `StatusInstance`s (Poison + Attack/Defense up/down).
Skills/items apply them to their targets; `Battle::tickStatuses` (called at each
unit's turn start) deals poison and ages durations. Buffs/debuffs scale the
effective attack/defense used in the damage formulas; `Cure`/antidotes strip
poison and debuffs. A **Brute** boss `enrages` (×1.5 attack below half HP) and
shows a telegraph line; **Commander/Rush** bosses field a fixed minion team
(dynamic summons / true waves are deferred). The base (no-status, non-boss)
formulas are unchanged, so prior battle tests still hold.

### Equipment (`game/`)

`Character` has `weapon`/`armor`/`accessory` slots. `refreshCharacter(c, db)`
re-derives stats from the class **and** the equipped items' `statBonus`, then
clamps hp/mp — called on equip changes and on load. The Equip Shop
(`EquipShopState`) buys gear with gold and equips/unequips it. Equipment is saved
as optional per-member fields (save version unchanged at `1`; unknown ids are
dropped on load).

### Theme/depth generation

`generate(seed, depth, db, themeId)` draws enemy and boss pools from the chosen
`DungeonThemeDef` (falling back to all content for an unknown id), and scales path
length, team size, elite chance, gate count, and rewards with depth. The Guild
chooses theme + depth, giving infinite seeded runs. Bosses are built from
`BossDef` (the boss plus its minions as one `EnemyTeam` with a `bossId`).

## 13. Presentation (Milestone 8)

- **Audio (`audio/AudioManager`):** SFX and looping music are **synthesized at
  runtime** into raylib `Sound`s (RAII-wrapped) — no asset files required. An
  internal device guard owns `InitAudioDevice`/`CloseAudioDevice` (so sounds
  unload before the device closes). Every call is guarded by the device state, so
  a failed init or generation is a silent no-op, never a crash. Music loops by
  re-playing when the current track stops. Real files dropped under
  `assets/audio/` are the intended later replacement.
- **Transitions (`core/FadeController`):** a pure fade-in timer; `Application`
  draws a black overlay at `coverAlpha()` inside the 426×240 target so it scales
  with the game. Scene states call `fade.start()` (and set music) in their
  `onEnter`/`onResume`/ctor.
- **Animation:** `BattleState` spawns floating damage/heal numbers (computed from
  per-unit HP deltas around each action) that rise and fade.
- **Shared services:** `AppContext` now also carries `AudioManager&` and
  `FadeController&`. The `HelpState` (from the main menu) documents controls.

Visuals and audio are validated by a human (not unit-tested); only pure timing
(`FadeController`) is covered by tests.

## 14. Balancing & validation (Milestone 9)

`battle/Simulator` deterministically auto-resolves a `Battle` with simple AI for
both sides (party: heal a badly hurt ally if able, else the strongest affordable
damaging skill on the weakest enemy). It is pure and used by the balance tests to
make difficulty/score claims objective: deeper dungeons rate more dangerous, the
same party fares worse at greater depth, a starting party clears a depth-1 gate,
a geared level-1 party can down a depth-1 boss, and a simulated full clear scores
positively. No coefficients needed tuning. Malformed-data and save-compatibility
coverage was extended (bosses/themes parsers; minimal/old saves; dropped gear).

## 15. UI layout & text safety (Milestone 12)

Text placement is measured, never guessed (M12). The pure layer is headless-
tested; raylib adapters live in `ui/UiDraw`.

- **`ui/TextLayout`** (pure): injectable `TextMeasure`; `wrapText` does greedy
  word wrap with long-token breaking (UTF-8-boundary safe) and preserves
  explicit newlines; `lineHeight(fontSize)` is the single line-spacing rule.
- **`ui/ScrollWindow`** (pure, header-only): scrolling window over vertical
  lists — `follow` keeps a cursor visible (menus), `scrollBy` free-scrolls
  (scoreboard); `moreAbove/moreBelow` drive the arrow indicators.
- **`ui/UiStyle.hpp`**: named font roles (TitleHero 22 / ScreenTitle 16 /
  Heading 14 / MenuLarge 14 / Menu 11 / Body 10 / Small 8), spacing, shared
  colors. States use roles, not ad-hoc numbers, as they are migrated.
- **`ui/UiDraw` additions**: `measureText`/`raylibMeasure()`;
  `drawTextRight`; `drawTextFitted` (fits-or-clips + logs); `drawTextWrapped`
  (bounded lines + logs); `drawMenuScrolled` (window slice + arrows).
- **Overflow diagnostics:** any bounded draw that cannot fit reports
  `[ui-overflow] site: 'text' Wpx > Mpx` once per site/text via `cd::log` and
  scissor-clips instead of spilling. Silent truncation is prohibited; the log
  line is the regression tripwire until M23 automates capture.
- **Migrated in M12:** Help; battle bottom panel (command menu now fits — it
  previously drew its last row off-screen; skill/item lists scroll in 4 rows
  with the selected entry's description in the right column; 5-enemy columns
  start higher and enemy statuses sit beside the unit); equip/item shops
  (scrolling lists + selected-item detail line); scoreboard (right-aligned
  numeric columns, depth column, free scrolling); town/dungeon HUD backdrops
  sized to measured text; dungeon fight prompt includes the team name; title
  menu centered on measured labels; result-panel pitch; Inn/Training Hall
  drop `%-N s` pseudo-columns. Debug overlay now starts hidden (F1 toggles).
- **Not yet migrated** (fit today, tracked in the audit register): party
  creation, save slots, Guild, pause menus. Hard-coded control-prompt strings
  everywhere are M13 scope (binding-derived labels).

## 16. Leveling, shops & packaging (Milestone 10)

- **XP/leveling (`game/Party`):** `xpToNext(level)` defines the curve;
  `grantXp`/`grantPartyXp` add XP, level up (capped at `kMaxLevel`), recompute
  stats via `refreshCharacter`, and heal the max-HP gained. Battle victories in
  the dungeon award the party XP and gold from the defeated team's enemies and
  boss (`xpReward`/`goldReward`). `level`/`xp` persist in the save.
- **Shops/services:** `ItemShopState` buys consumables; `EquipShopState` buys and
  equips gear; `TrainingHallState` pays gold to level a character up by one
  (gold→level progression alongside battle XP). Parties start with a little gold.
- **Packaging:** the final `README.md` documents what the game is, the MSVC build/
  run, controls, the play loop, project layout, the smoke test (the 125-test
  suite, which loads content, generates dungeons, and simulates a clear), and
  known limitations. The deliverable is `CrystalDungeons.exe` plus the `data/`
  folder copied beside it.

## Release-hardening corrections (post-356619d audit)

- `CRYSTAL_DEBUG_OVERLAY` and `CRYSTAL_CAPTURE` are configuration-aware generator-expression definitions and are never compiled for any Release configuration, including Visual Studio multi-config builds. `CRYSTAL_SHIPPING_BUILD` adds a compile-time guard against accidental linkage.
- The `msvc-release` preset explicitly disables both development surfaces. Packaging independently scans the staged executable for debug/capture markers and verifies the PE machine type is AMD64.
- Save slots, settings, tutorial progress, and scoreboard data use sibling-temp atomic replacement. Save slots retain one `.bak` generation. A failed write leaves the prior destination intact.
- Scoreboard loading is transactional: malformed data does not replace already-valid in-memory entries.
- Packaged builds write timestamped persistent logs under the user-data `logs` directory; fatal startup errors show a GUI dialog with the log path.
- Dungeon generation compatibility is currently version 4.

