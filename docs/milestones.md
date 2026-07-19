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
| 9  | Balancing & validation pass       | ☑ complete (approved) |
| 10 | Release packaging pass            | ☑ complete (approved) |
| 11 | Completion baseline & presentation audit | ☐ not started |
| 12 | UI layout, typography & text safety | ☐ not started |
| 13 | Input consistency, remapping & settings | ☐ not started |
| 14 | Asset catalog & replaceable resources | ☐ not started |
| 15 | Art-direction vertical slice       | ☐ not started |
| 16 | Compact dungeon-room architecture  | ☐ not started |
| 17 | Exploration visual production      | ☐ not started |
| 18 | Battle presentation & game feel    | ☐ not started |
| 19 | Progression, economy & score integrity | ☐ not started |
| 20 | Encounter, event & boss variety    | ☐ not started |
| 21 | Final music, ambience & SFX        | ☐ not started |
| 22 | Onboarding & accessibility         | ☐ not started |
| 23 | Automated visual/content validation | ☐ not started |
| 24 | Playtesting, tuning & polished release candidate | ☐ not started |

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

Deliverables: final README; build/run/controls; known limitations; smoke tests;
clean structure; final playable build target. Plus (human-requested): XP/leveling
(Option A), Item Shop, and Training Hall.

Implemented:
- **XP/leveling** (`game/Party`): `xpToNext`/`grantXp`/`grantPartyXp`. Battle
  victories grant the party XP and gold from the defeated team (enemies + boss
  `xpReward`/`goldReward`); level-ups recompute stats via `refreshCharacter` and
  heal the HP gained. `level`/`xp` already persist; level cap 50.
- **Item Shop** (`ItemShopState`): buy consumables with gold.
- **Training Hall** (`TrainingHallState`): pay gold to level a character up by
  one (gold→level progression alongside battle XP).
- Starting gold (150) for onboarding; the town's seven locations are now all
  real states (removed the now-unused `PlaceholderLocationState`).
- **Release packaging:** final README (what it is, build/run, controls, how to
  play, project layout, smoke test, known limitations, originality note); Release
  build documented; the test suite (125 tests) serves as the smoke test; clean
  project structure.
- Tests: XP curve, level-up stat recompute + heal, level cap, party XP, boss
  `xpReward` in shipped data. 125 tests total, all passing.

The full Milestone 1–10 plan is complete: a feature-complete, playable build.

## Post-M10 completion program

The strategic rationale, non-negotiable requirements, cross-cutting engineering
rules, and detailed acceptance criteria for M11–M24 are in
`docs/completion_roadmap.md`.

Current-scope authority remains:

1. `CLAUDE.md`;
2. this milestone ledger;
3. the active file under `docs/milestone_notes/`;
4. the broader completion roadmap.

M10 is implemented but not yet human-approved at the reviewed baseline. **Do not
begin M11 until the human explicitly approves M10.**

## M11 — Completion baseline & presentation audit

Goal: verify the approved M10 baseline and inventory every player-facing screen,
dynamic variant, control path, presentation defect, and validation gap before
changing production code.

Deliverables:

- verified build/test baseline and honest unverified items;
- screen/flow inventory and representative screenshots where possible;
- prioritized `docs/presentation_audit.md` defect register;
- initial `docs/ui_style_guide.md`;
- initial `docs/control_standard.md`;
- initial `docs/asset_pipeline.md`;
- `docs/manual_test_matrix.md`;
- evidence-based recommendation for M12-a.

Out of scope: UI implementation, remapping/settings, asset schema/API changes,
art/audio replacement, room-generation changes, combat/content/economy changes.

Acceptance: all major screens and maximum-content risks are represented;
observed defects are distinguished from static-analysis risks; priorities are
human-approved; no later milestone has begun.

Detailed proposed scope: `docs/milestone_notes/M11_completion_baseline.md`.

## M12 — UI layout, typography & text safety

Goal: introduce a small measured layout/text system and migrate screens in
reviewable families so required text never clips or overlaps.

Deliverables:

- font-aware measurement, wrapping, line-height, long-token handling, and
  overflow diagnostics;
- reusable insets/alignment/stacks/panels/list viewport/scroll policy;
- typography roles, spacing scale, focus/disabled/danger conventions;
- screen-family migrations with long/empty/maximum-data tests;
- human readability validation at supported window sizes.

Out of scope: final art direction, broad control-remapping work, room-system
rewrite, final content/audio.

Acceptance: zero unintended overflow in the manual matrix; every dynamic list
scrolls or paginates; focus remains visible; no required information is silently
ellipsized.

## M13 — Input consistency, remapping & settings

Goal: make all menus and gameplay predictable with keyboard and controller and
make prompts reflect active bindings.

Deliverables:

- semantic-action audit and removal of production raw-input exceptions;
- stable navigation repeat, deadzone/hysteresis, and transition-input
  suppression;
- intentional active-device tracking;
- binding-derived prompt labels;
- defensive settings persistence;
- keyboard/gamepad remapping with conflict recovery;
- initial display/audio/gameplay/accessibility options.

Acceptance: complete keyboard-only and gamepad-only runs; D-pad and left stick
navigate all UIs; remapping cannot strand the player; malformed settings and
controller disconnect recover safely.

## M14 — Asset catalog & replaceable resources

Goal: centralize presentation identity in a validated external catalog so
states request logical IDs instead of file paths.

Deliverables:

- approved versioned asset-manifest schema;
- raylib-free loader/validator;
- logical-ID `ResourceManager` interface;
- file-backed `AudioManager` with safe synthesized/silent fallback;
- texture/font/music/ambience/SFX metadata;
- manual development reload;
- packaging and attribution/license policy.

Acceptance: replacing a texture, font, music track, or SFX for an existing role
requires no C++ edit; missing optional assets do not crash; no state contains a
presentation path.

## M15 — Art-direction vertical slice

Goal: prove the final visual/audio direction and production cost before creating
the full asset set.

Deliverables:

- approved art bible and pixel-grid conventions;
- polished title, one town area/service, one compact Ruined Keep room, one
  normal battle, one boss battle, and one result screen;
- representative manifest-driven music/SFX;
- readability and scaling review.

Acceptance: human approves the coherent slice; assets are readable at native
resolution; full production has not started before the gate; all assets are
original or correctly licensed.

## M16 — Compact dungeon-room architecture

Goal: separate dungeon topology from compact room realization and replace the
fixed 26×15 full-screen room assumption.

Deliverables:

- room archetypes and data-defined bounded dimensions;
- layout model for local tiles, door anchors, spawn points, collision, markers,
  prop zones, and draw origin;
- derived room-local deterministic seeds;
- flood-fill/BFS validation;
- score/generator-version decision if reproducibility semantics change;
- representative compact-room human testing.

Acceptance: standard rooms no longer fill the exploration screen; at least five
archetypes appear across representative runs; large generation samples pass
connectivity and progression invariants; same seed plus generation version is
stable.

## M17 — Exploration visual production

Goal: produce final-quality town/dungeon characters, enemies, tiles, props,
animations, effects, and theme atmosphere on the stable asset and room systems.

Deliverables:

- animation metadata and renderer;
- visual bounds separated from collision bounds;
- stable draw layers;
- player and visible-enemy sprites;
- theme-specific environment sets and interactable indicators;
- restrained glows/particles/overlays.

Acceptance: silhouettes and interactables are readable; themes differ through
shape/composition rather than hue alone; decoration does not obscure gameplay;
missing animations fall back safely.

## M18 — Battle presentation & game feel

Goal: make deterministic combat clear, fast, and satisfying without undermining
fewest-turn score play.

Deliverables:

- redesigned information hierarchy;
- active combatant/target/status/cost/telegraph clarity;
- short action sequences, hit feedback, particles, KO/victory feedback;
- configurable flash, shake, and battle speed;
- presentation timing separated from simulation results.

Acceptance: source, target, effect, and result are always clear; Fast mode
materially reduces downtime; reduced-effect options work; combat remains
understandable muted; deterministic outcomes are unchanged.

## M19 — Progression, economy & score integrity

Goal: audit and tune the existing XP/leveling, Training Hall, shops, equipment,
and scoring so progression supports replay without presenting incomparable runs
as equivalent.

Deliverables:

- XP/gold/purchase/failure simulations and human-run evidence;
- class and equipment growth review;
- exploit/dominant-path analysis;
- explicit score-comparability policy, preferably segmented/tagged by relevant
  run conditions rather than opaque normalization;
- data-driven tuning.

Acceptance: no obvious farming or economy exploit; Training Hall and battle XP
have defensible roles; class identities remain meaningful; score conditions are
visible and honest.

## M20 — Encounter, event & boss variety

Goal: add targeted tactical variety after the presentation and room foundations
are stable.

Deliverables:

- encounter-role taxonomy and externalized composition constraints;
- limited meaningful room-event set with visible trade-offs;
- dungeon modifiers only where risk/reward is clear;
- boss mechanic, telegraph, escalation, silhouette, reward, and audio identity
  improvements.

Acceptance: generated teams cannot violate composition rules; events create
real decisions; representative bosses are mechanically distinguishable; new
content fills identified role gaps rather than chasing quantity.

## M21 — Final music, ambience & SFX

Goal: replace synthesized placeholders with coherent original/licensed audio
through the asset catalog.

Deliverables:

- title/town/preparation/dungeon/battle/boss/victory/defeat/result music states;
- town and per-theme ambience;
- complete UI/exploration/battle SFX families;
- streamed music, clean loops/transitions, mixing and persistent volume
  controls;
- provenance/license records.

Acceptance: no stacked tracks, clicks, clipping, or hard crashes; audio is
replaceable without C++; the game remains understandable muted.

## M22 — Onboarding & accessibility

Goal: teach the game through play and remove avoidable barriers while retaining
tactical challenge.

Deliverables:

- progressive contextual onboarding;
- revisit/disable/reset tutorial controls;
- Details help for stats/status/danger/score/equipment;
- high-contrast and reduced motion/flash/shake options;
- configurable message pacing;
- non-color and non-audio alternatives;
- clear error/destructive-action handling.

Acceptance: a new tester completes depth 1 without external instruction;
critical mechanics are available in-game; accessibility cases pass the manual
matrix; settings are reachable before New Game.

## M23 — Automated visual/content validation

Goal: make representative presentation states reproducible and prevent future
layout, asset, room, and balance regressions.

Deliverables:

- deterministic debug/capture scenarios;
- native-resolution screenshot capture;
- UI bounds/overlap/overflow diagnostics;
- presentation-content linting;
- large-scale room validation;
- expanded simulation reports.

Acceptance: unintended overflow and missing required assets fail validation;
representative captures are reproducible; thousands of rooms pass invariants;
debug tooling is excluded or safely disabled in Release.

## M24 — Playtesting, tuning & polished release candidate

Goal: validate the complete experience with external players, resolve high-value
issues, and ship a reproducible polished Windows candidate.

Deliverables:

- observed keyboard/controller/new-player playtests;
- control/readability/navigation/score-comprehension fixes;
- final balance and pacing changes backed by evidence;
- Release build/preset and packaged `data/` + `assets/`;
- version/icon/credits/licenses/logging/known limitations;
- clean-machine smoke test and final manual-matrix sign-off.

Acceptance: new players complete the loop unaided; repeated usability defects
are fixed; no dominant strategy trivializes representative runs; packaged build
runs without Visual Studio; no debug presentation, unlicensed asset, or
unintended placeholder ships; human approves the release candidate.

