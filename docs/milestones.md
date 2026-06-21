# Crystal Dungeons — Milestones

> Work in order. **Stop after each milestone and wait for human approval.**
> Status legend: ☐ not started · ◐ in progress · ☑ complete (human-approved).

| #  | Milestone                         | Status |
|----|-----------------------------------|--------|
| 1  | Project foundation                | ☑ complete (approved) |
| 2  | Core data model                   | ☑ complete (approved) |
| 3  | Town hub shell                    | ☑ complete (approved) |
| 4  | Dungeon generator                 | ☑ complete (approved) |
| 5  | Battle system MVP                 | ☑ complete (approved) |
| 6  | Danger rating & scoring           | ☑ complete (approved) |
| 7  | Content pass                      | ☑ complete (approved) |
| 8  | Presentation pass                 | ☑ complete (approved) |
| 9  | Balancing & validation pass       | ◐ implemented, awaiting approval |
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

Deliverables: stat-derived danger formula + labels; battle-turn tracking; dungeon
score; scoreboard; tests for danger and scoring.

Implemented:
- Danger (`danger/`, pure, tested): `teamThreat` = stat-weighted sum
  (HP/Attack/Magic/Defense/Speed) + per-skill threat, scaled by a team-synergy
  factor (enemy count + healer presence); `tierFor` maps threat vs a depth
  baseline to **Trivial / Easy / Fair / Dangerous / Deadly**, with boss teams
  always **Boss**. Derived only from stats/abilities — never hand-authored.
- Visible danger: enemy teams now show a colored tier label on the dungeon map,
  and the fight prompt shows tier + count + tags; guarded chests show rarity and
  a fight-to-claim prompt.
- Battle-turn tracking: a battle reports a `BattleResult` (outcome, **rounds**,
  party-KO). The dungeon accumulates rounds, teams/danger defeated, chests/
  treasure, escapes, and a no-death flag across the run.
- Scoring (`score/`, pure, tested): `computeScore`/`scoreBreakdown` — base +
  boss − per-round turn penalty + chest + danger-defeated + treasure + no-death −
  escape; **0 if not completed**. Boss victory shows a `DungeonResultState`
  breakdown, records the run, and returns to town.
- Scoreboard (`score/Scoreboard`): persistent JSON in the user data dir
  (defensive load, ranked best-first); real `ScoreboardState` replaces the town
  placeholder.
- Tests: danger monotonicity/tiers/boss/thresholds/determinism; scoring
  (incomplete=0, fewer-turns-higher, bonuses, clamp); scoreboard
  add/sort/save/load/malformed. 98 tests total, all passing.

Decisions (reversible): "battle turns" = rounds; "no-death" = no party KO during
the run; weights/thresholds/coefficients are tunable in the balance pass (M9).

Out of scope (deferred): balance tuning (M9); depth-scaled content/themes (M7);
real-time tie-breaker on the scoreboard; battle/UI art (M8).

## M7 — Content pass

Deliverables: 6 classes; enemies/elites/bosses; items/equipment/relics;
skills/spells; 3 themes; balance pass for playable runs; infinite depth-scaled
runs.

Implemented:
- Content: 6 classes, **28 skills** (across physical/magic/heal/support, several
  status-applying), **20 enemies** (14 normal + 6 elite), **36 items**
  (consumables, weapons, armor, accessories, relics, scrolls), **4 bosses**
  (Brute/Sorcerer/Commander/Rush), **3 themes** (Ruined Keep / Crystal Mine /
  Hollow Forest). New `bosses.json` + `dungeon_themes.json` with loaders,
  validators, and reference checks.
- **Status-effect system** (chosen): Poison (DoT) plus Attack/Defense up/down,
  applied by skills/items, ticked each turn, folded into the damage formulas;
  antidote/`Cure` now lift poison and debuffs. Bosses use a deterministic
  **enrage** (Brute) and **telegraph** text; minions accompany Commander/Rush.
- **Equipment system** (chosen, minimal): weapon/armor/accessory slots whose
  stat bonuses fold into derived stats (`refreshCharacter`); a functional **Equip
  Shop** (buy with gold + equip/unequip). Equipped gear is saved as an optional
  field (save version stays 1; old saves load).
- **Generation:** theme-aware enemy/boss pools + **depth scaling** (path length,
  team size, elite chance, gate count, rewards); the Guild gains **theme + depth
  pickers** → infinite seeded runs. Bosses build from `BossDef`.
- Tests: status effects (poison/buff/debuff/cure/enrage), equipment stat
  application, boss/theme loaders + validation, themed/depth-scaled generation.
  108 tests total, all passing.

Decisions (reversible): "battle turns" = rounds; "no-death" = no party KO; status
set is focused (poison + atk/def buffs/debuffs); balance numbers are first-pass.

Out of scope (deferred): dynamic summons and true multi-wave rush (Commander/Rush
use a fixed minion team); leveling/XP economy curve and deeper balance (M9);
sprite art/animation (M8).

## M8 — Presentation pass

Deliverables: UI polish; animations; transitions; placeholder audio/music/SFX;
better feedback text; controls/help screen.

Implemented:
- **Audio** (`audio/AudioManager`): placeholder SFX and looping music are
  **synthesized at runtime** (no asset files needed). Fully guarded — if the
  audio device or generation fails, every call is a silent no-op (never a crash).
  Per-scene music (Town/Dungeon/Battle) and SFX wired to menus, combat
  (hit/heal/KO/victory/defeat), chests, and the inn. Real files can replace it
  later under `assets/audio/`.
- **Transitions:** a shared `FadeController` (pure, tested) drives a fade-in on
  every scene change (menu↔town↔dungeon↔battle↔result); `Application` draws the
  fade in virtual space so it scales cleanly.
- **Animation:** floating damage/heal numbers rise and fade in battle; the title
  prompt blinks; the player shows a facing tick.
- **Controls/help screen** (`HelpState`) from the main menu, listing keyboard and
  gamepad mappings.
- **UI polish:** layered 16-bit window frames (double border); clearer battle/
  status/danger feedback.
- Test: `FadeController` timing.  110 tests total, all passing.

Note: audio + visuals are **human-validation** items (cannot be auto-tested).
The synthesized audio is intentionally primitive; final creative direction is
out of scope.

Out of scope (deferred): real sprite/tileset art and bespoke music (placeholder
remains); richer battle animation; screen-shake and particle effects.

## M9 — Balancing & validation pass

Deliverables: difficulty curves; score sanity; malformed-data testing; save
compatibility; performance cleanup; bug fixing.

Implemented:
- **Headless battle simulator** (`battle/Simulator`, pure): deterministically
  auto-resolves a fight with AI for both sides — the validation instrument for
  difficulty/score sanity.
- **Difficulty curves (validated):** deeper dungeons are more threatening (danger
  rises with depth); the same party fares worse against deeper gates; a starting
  level-1 party clears a depth-1 gate; a geared level-1 party (the realistic
  in-game power ceiling) defeats a depth-1 boss. No coefficient tuning was
  required — the existing balance passes the sanity checks.
- **Score sanity:** a simulated full clear yields a positive, sensible score;
  fewer turns score higher (existing tests).
- **Malformed-data hardening:** added tests for `bosses.json`/`dungeon_themes.json`
  (bad/missing archetype, missing enemy/boss pools) on top of the existing
  per-file validators; loading stays defensive and never crashes.
- **Save compatibility:** minimal v1 saves (no inventory/equipment) load; saved
  equipment round-trips; gear ids the content no longer knows are dropped on load.
- **Performance:** reviewed the per-frame render/update paths — no heap
  allocation in the movement hot path; the 426×240 target is well within budget,
  so no changes were needed.
- **Bug review:** no critical bugs found (the lifecycle-hook transition bug was
  fixed during M5). 120 tests total, all passing.

KNOWN GAP (surfaced by this pass): **XP/leveling is not implemented** — parties
stay level 1; power grows only via gold/equipment. The game is winnable this way
(validated), but the design brief lists "Characters gain XP and levels."
Recommend adding minimal leveling before release (M10) — a scope decision for the
human.

## M10 — Release packaging pass
Final README; build/run/controls; known limitations; smoke tests; clean
structure; final playable build target.
