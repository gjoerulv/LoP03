# Crystal Dungeons — Milestones

> Work in order. **Stop after each milestone and wait for human approval.**
> Status legend: ☐ not started · ◐ in progress · ☑ complete (human-approved).

| #  | Milestone                         | Status |
|----|-----------------------------------|--------|
| 1  | Project foundation                | ☑ complete (approved) |
| 2  | Core data model                   | ◐ implemented, awaiting approval |
| 3  | Town hub shell                    | ☐ |
| 4  | Dungeon generator                 | ☐ |
| 5  | Battle system MVP                 | ☐ |
| 6  | Danger rating & scoring           | ☐ |
| 7  | Content pass                      | ☐ |
| 8  | Presentation pass                 | ☐ |
| 9  | Balancing & validation pass       | ☐ |
| 10 | Release packaging pass            | ☐ |

## M1 — Project foundation

Deliverables: CMake project; raylib window; basic game loop; internal
resolution/scaling (426×240); state stack; resource-manager foundation; minimal
Catch2 test setup; basic README/build instructions.

Implemented:
- FetchContent for raylib `6.0`, Catch2 `v3.15.1`, nlohmann/json `v3.12.0`
  (pinned), built with MSVC / C++20, high warnings on project code only.
- `cd::Application` game loop + window + 426×240 virtual screen with
  aspect-preserved scaling and y-flip correction.
- `StateStack` with queued transitions and transparent states; `TitleState` +
  `MenuOverlayState` demo (push/pop, transparent overlay).
- `InputMap` (keyboard + gamepad bindings) with pure, injectable resolution;
  raylib query factory for production.
- `ResourceManager` foundation: cached, RAII-owned textures/fonts; generated
  placeholder on missing/failed load.
- `paths::userDataDir` / `paths::sanitizeRelative`.
- Catch2 tests: viewport math, state-stack transitions, input resolution, path
  sanitizing.
- README with build/run instructions; debug overlay behind a flag.

Out of scope (deferred): real assets, JSON content (M2), gameplay systems.

## M2 — Core data model

Deliverables: JSON loaders; content schemas/validators; data structures for
classes/enemies/skills/items; tests for validators and malformed data.

Implemented (`src/content/`, raylib-free and headless-testable):
- Data structures: `StatBlock`/`StatGrowth`, `SkillDef`, `ClassDef`, `EnemyDef`,
  `ItemDef`, and content enums with string<->enum mapping (`Enums`).
- `ContentDatabase`: id→def maps, lookups, duplicate-id detection (JSON-free).
- `ObjectReader` (`JsonValidation`): typed required/optional field reads with
  type/range/enum checks that append readable, contextual errors to a
  `LoadReport` instead of throwing.
- `ContentLoader`: in-memory `parse*` (unit-testable) + file `loadAll` (catches
  JSON syntax errors) + `validateReferences` (skill ids in classes/enemies/
  scrolls). Each file is `{ "version": 1, "<plural>": [...] }` (schema v1).
- Seed content: `data/{skills,classes,enemies,items}.json` — 10 skills, the 6
  fixed classes, 7 enemies, 10 items. Minimal but valid; full rosters are M7.
- Integration: `Application` loads `data/` at startup into the `AppContext`,
  logs a summary, and degrades gracefully on errors; CMake copies `data/` next
  to the exe; the title screen shows the loaded counts.
- Tests: field/type/range/enum/duplicate/malformed-wrapper validation,
  reference checks, file IO (missing/malformed), and a test that loads the real
  shipped JSON with zero errors. 42 tests total, all passing.

Out of scope (deferred to owning milestones): `bosses.json` (M7) and
`dungeon_themes.json` (M4); leveling/economy logic; equipping/using items.

## M3 — Town hub shell
New game/party creation; class selection + renaming; town navigation;
inn/shop/guild/save/scoreboard placeholders; save/load MVP.

## M4 — Dungeon generator
Seeded generation; room graph/grid; start/boss/main-path/side rooms; visible
enemy teams; chests; ≥3 mandatory gates before boss; exit/retreat flow.

## M5 — Battle system MVP
Side-view screen; turn order; Attack/Skill/Item/Guard/Escape; damage/healing;
victory/defeat/escape; KO/revive; return to dungeon.

## M6 — Danger rating & scoring
Stat-derived danger formula + labels; battle-turn tracking; dungeon score;
scoreboard; tests for danger and scoring.

## M7 — Content pass
6 classes; enemies/elites/bosses; items/equipment/relics; skills/spells; 3 themes;
balance pass for playable runs.

## M8 — Presentation pass
UI polish; animations; transitions; placeholder audio/music/SFX; better feedback
text; controls/help screen.

## M9 — Balancing & validation pass
Difficulty curves; score sanity; malformed-data testing; save compatibility;
performance cleanup; bug fixing.

## M10 — Release packaging pass
Final README; build/run/controls; known limitations; smoke tests; clean
structure; final playable build target.
