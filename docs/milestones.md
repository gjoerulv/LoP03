# Crystal Dungeons — Milestones

> Work in order. **Stop after each milestone and wait for human approval.**
> Status legend: ☐ not started · ◐ in progress · ☑ complete (human-approved).

| #  | Milestone                         | Status |
|----|-----------------------------------|--------|
| 1  | Project foundation                | ☑ complete (approved) |
| 2  | Core data model                   | ☑ complete (approved) |
| 3  | Town hub shell                    | ☑ complete (approved) |
| 4  | Dungeon generator                 | ☑ complete (approved) |
| 5  | Battle system MVP                 | ◐ implemented, awaiting approval |
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

Deliverables: new game/party creation; class selection + renaming; town
navigation; inn/shop/guild/save/scoreboard placeholders; save/load MVP.

Implemented:
- Flow: `MainMenu` (New Game / Continue / Quit) → `PartyCreation` (4 slots, class
  cycle + modal name editing) → **walkable `TownState`** → enter buildings → back.
- Walkable town (chosen over a menu hub): a single-screen 26×15 tilemap
  (`town/`), free player movement with axis-separated tile collision, 7 buildings
  with door-entry prompts. Pure tilemap/movement logic is unit-tested.
- Locations: **Inn** (real full heal), **Save Point** (save to a manual slot),
  Training Hall (lists classes), and Guild/Item Shop/Equip Shop/Scoreboard
  placeholders. Pause overlay (Menu/Cancel) → Resume / Quit to Title.
- Game model (`game/`): `Character`/`Party`; `createCharacter` derives stats from
  a `ClassDef` (base + per-level growth); `healFull`.
- Save system (`save/`): versioned JSON in the user data dir; **3 manual slots +
  an autosave slot**; defensive loading (reuses the M2 validator) that never
  corrupts the target party; `autosave()` capability ready for the dungeon-entry
  trigger. Loading any slot always returns the party to **town**.
- Reusable UI (`ui/`): pure `Menu` (cursor nav) and `TextInput` (name buffer),
  plus raylib `UiDraw` helpers. Pure geometry in `core/Geometry.hpp`.
- Retired the M1 demo states (`TitleState`/`MenuOverlayState`), replaced by the
  real menu/town flow.
- Tests: menu nav, text input, character derivation, save round-trip/malformed/
  version/unknown-class/slots/autosave, tilemap collision & town reachability.
  62 tests total, all passing.

Autosave semantics (per the human): autosave fires on dungeon entry — the
trigger is wired in M4; the capability and slot exist now. Loading an autosave
always starts in town, never inside a dungeon.

Out of scope (deferred): real shop economy, equipping/using items, dungeon
selection/entry (M4), real scores (M6), tile/character sprite art (M8).

## M4 — Dungeon generator

Deliverables: seeded generation; room graph/grid; start/boss/main-path/side
rooms; visible enemy teams; chests; ≥3 mandatory gates before boss; exit/retreat
flow.

Implemented:
- Generator (`dungeon/`, pure, unit-tested): deterministic `Rng`; `generate(seed,
  depth, db)` builds a randomized-DFS main path (start→boss) on a 7×7 grid with
  **≥3 mandatory gated doors** on that unique path, dead-end side rooms holding
  chests (≥1 guarded), and a boss team. Enemy teams and chest rewards are drawn
  from the M2 content database; same seed ⇒ identical dungeon.
- Walkable dungeon (chosen over a map view): `DungeonState` builds each room as a
  single-screen tilemap (reusing `town::Tilemap`/`resolveMove`), walls with door
  gaps, walk room-to-room. **Gated doors are impassable** (a team blocks them)
  until the battle milestone; a minimap shows the generated graph and gates.
- Visible teams show **name / count / members / tags** (no danger tier — that is
  the M6 formula). `EncounterPreviewState` inspects a team. Unguarded chests open
  (grant gold); guarded chests prompt "defeat the guard first (M5)".
- Flow: Guild (`GuildState`, replacing the placeholder) → pick/reroll seed →
  **autosave on entry** (wires the M3 capability) → `DungeonState`.
  `DungeonMenuState` → Resume / Retreat to Town.
- Tests: RNG determinism; generation determinism; start/boss/≥3-gate structure;
  boss reachable **only** through gated doors; ≥1 guarded chest; teams/chest
  rewards reference real content. 68 tests total, all passing.

Out of scope (deferred): combat/passing gates (M5); real danger tier and dungeon
score (M6); item inventory (chest items are previewed, not stored); multiple
themes/depth scaling content (M7); sprite art (M8).

## M5 — Battle system MVP

Deliverables: side-view screen; turn order; Attack/Skill/Item/Guard/Escape;
damage/healing; victory/defeat/escape; KO/revive; return to dungeon.

Implemented:
- Combat sim (`battle/`, pure, **deterministic**, unit-tested): `Combatant`,
  `Battle` (speed-ordered `turnOrder`, formula damage/heal/guard, KO, item-revive,
  `Outcome`), `buildBattle` from party + enemy team, deterministic enemy AI.
- `BattleState`: side-view screen (enemies left, party right), phase machine for
  **Attack / Skill / Item / Guard / Escape**, target/skill/item selection, HP/MP
  bars, turn counter. Writes hp/mp back to the party; consumes items.
- **Inventory** (`game/Inventory`): starter potions on New Game, M4 chest items
  now stored, persisted in the save as an optional `inventory` field
  (backward-compatible, save version unchanged at 1).
- **Dungeon integration** (closes the core loop): facing a team + Confirm starts
  a real battle. **Victory** clears the gate / chest guard; beating the **boss**
  completes the dungeon → town. **Defeat** → game over (full heal, lose half
  gold, back to town). **Escape** → stays, gate uncleared. Replaced M4's
  `EncounterPreviewState`.
- Tests: damage/guard/magic/heal formulas, KO + item-revive, turn order, victory/
  defeat detection, enemy AI, inventory ops, save inventory round-trip +
  backward compatibility. 80 tests total, all passing.

Out of scope (deferred): danger tier + dungeon score (M6); damage variance/crits
and status ailments e.g. poison (M7/M9); battle sprites/animation (M8).

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
