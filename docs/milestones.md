# Crystal Dungeons — Milestones

> Work in order. **Stop after each milestone and wait for owner approval.**
> Status legend: ☐ planned · ◐ in progress · ◑ implemented, awaiting manual
> approval · ☑ complete (approved) · ⊘ blocked.
> Only the project owner can set `complete (approved)`, after manual testing;
> builds, tests, screenshots, or simulations alone never justify it.

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
| 11 | Completion baseline & presentation audit | ☑ complete (approved) |
| 12 | UI layout & text-safety foundation | ☑ complete (approved) |
| 13 | Input consistency, remapping & settings | ☑ complete (approved) |
| 14 | Asset manifest & replaceable resources | ☑ complete (approved) |
| 15 | Art-direction vertical slice       | ☑ complete (approved) |
| 16 | Compact dungeon-room system        | ☑ complete (approved) |
| 17 | Exploration visuals & animation    | ☑ complete (approved) |
| 18 | Battle presentation & game feel    | ☑ complete (approved) |
| 19 | Progression, economy & score-integrity hardening | ☑ complete (approved) |
| 20 | Encounter & dungeon-content variety | ☑ complete (approved) |
| 21 | Final music, ambience & sound effects | ☑ complete (approved) |
| 22 | Onboarding & accessibility         | ☑ complete (approved) |
| 23 | Automated visual validation, playtesting & balance hardening | ☐ planned — **deferred, runs after M30** (tooling + tuning already built) |
| 24 | Release packaging & final release validation | ☐ planned — **deferred, runs after M23** (engineering already built) |
| 25 | UI corrections & battle HUD | ☑ complete (approved) |
| 26 | Enemy visual identity | ☐ planned |
| 27 | Environment & ambience identity | ☐ planned |
| 28 | Enmity, AI diversity & control skills | ☐ planned |
| 29 | Content expansion & class learnsets | ☐ planned |
| 30 | Economy: paid rest & rest-token event | ☐ planned |

**Execution order is not numeric order.** M25 → M26 → M27 → M28 → M29 → M30,
**then** M23 → M24. M23/M24 were deferred by the owner on 2026-07-20: the game
is not release-ready, so validating and packaging it would have measured the
wrong build. Their existing tooling and packaging work is retained, not
discarded — only their position in the sequence changed.

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

**Approved by the owner on 2026-07-19 after manual testing.**

## Post-M10 completion program

M10 was manually tested and **approved by the owner on 2026-07-19**. The
completion program runs M11–M24. The strategic rationale, non-negotiable
requirements, cross-cutting engineering rules, and quality targets are in
`docs/completion_roadmap.md`. Every milestone below has exactly one
authoritative note under `docs/milestone_notes/`; that note must be re-audited
and refreshed against the then-current repository before the milestone begins.

Documentation authority order (highest first): `CLAUDE.md`; the approved active
milestone note; this ledger; `docs/game_design.md`; `docs/technical_design.md`;
`docs/completion_roadmap.md`; supporting documents. The roadmap does not
authorize implementation from future milestones.

**Do not begin M11 until the owner explicitly authorizes it.** Approval of one
milestone is not automatic authorization to start the next.

## M11 — Completion baseline & presentation audit

- **Status:** ☑ complete (approved by the owner 2026-07-19; audited commit
  `8271871`; outputs: `docs/presentation_audit.md`, `docs/ui_style_guide.md`,
  `docs/control_standard.md`, `docs/asset_pipeline.md`,
  `docs/manual_test_matrix.md`, 7 baseline screenshots; 24-defect register —
  1 Blocker, 7 High. Approval includes the recommended M12-a slice.)
- **Goal:** verify the approved M10 baseline and convert vague polish goals into
  a prioritized, evidence-based defect register before changing production code.
- **Player-facing outcome:** none — the game is unchanged; this milestone is an
  audit.
- **Engineering outcome:** verified build/test baseline; complete screen/flow
  inventory; prioritized defect register; initial UI, control, asset, and
  manual-test contracts.
- **Primary deliverables:** `docs/presentation_audit.md`,
  `docs/ui_style_guide.md`, `docs/control_standard.md`,
  `docs/asset_pipeline.md`, `docs/manual_test_matrix.md`; screenshots where
  tooling permits; evidence-based M12-a recommendation.
- **Out of scope:** any production-code change; UI implementation;
  remapping/settings; asset schema/API changes; art/audio replacement;
  room-generation changes; combat/content/economy changes.
- **Dependencies:** M10 approved (satisfied 2026-07-19).
- **Acceptance criteria:** all major screens, dynamic variants, and
  maximum-content risks inventoried; observed defects distinguished from
  static-analysis risks; defect priorities owner-approved; no later milestone
  begun.
- **Automated validation:** existing test suite built and run green at the
  audited commit (or honestly reported unverified with exact commands).
- **Owner manual validation:** review the defect register, priorities, and the
  recommended M12-a slice; approve or amend.
- **Milestone note:** `docs/milestone_notes/M11_completion_baseline.md`

## M12 — UI layout & text-safety foundation

- **Status:** ☑ complete (approved by the owner 2026-07-19; base commit
  `5311ef9`; 144/144 tests; see the note's §N for the as-implemented record
  and deviations)
- **Goal:** replace fixed-coordinate text assumptions with a small measured
  layout/text system and migrate screens in reviewable families so required
  text never clips or overlaps.
- **Player-facing outcome:** readable text everywhere; scrolling/paged lists;
  visible focus; no clipped or overlapping required information.
- **Engineering outcome:** pure, testable text-measurement/wrapping and layout
  primitives in `src/ui/`; typography and spacing conventions; migrated screen
  families with overflow diagnostics.
- **Primary deliverables:** font-aware measurement, wrapping, line-height,
  long-token handling, overflow diagnostics; insets/alignment/stacks/panels/
  list-viewport/scroll policy; typography roles, spacing scale,
  focus/disabled/danger conventions; screen-family migrations with
  long/empty/maximum-data tests.
- **Out of scope:** final art direction; control-remapping work; room-system
  rewrite; final content/audio; any virtual-resolution change (separate owner
  decision).
- **Dependencies:** M11 defect register and initial UI style guide.
- **Acceptance criteria:** zero unintended overflow in the manual matrix; every
  dynamic list scrolls or paginates; focus remains visible; no required
  information silently ellipsized; 426×240 baseline retained unless separately
  approved.
- **Automated validation:** pure layout/wrapping/scroll unit tests including
  long, empty, and maximum-data cases; full suite green.
- **Owner manual validation:** readability pass over migrated screens at
  supported window sizes/shapes with keyboard and gamepad.
- **Milestone note:** `docs/milestone_notes/M12_ui_text_safety.md`

## M13 — Input consistency, remapping & settings

- **Status:** ☑ complete (approved by the owner 2026-07-19; base commit
  `a247b09`; 173/173 tests; see the note's §N for the as-implemented record
  and deviations)
- **Goal:** make all menus and gameplay predictable with keyboard and
  controller, make prompts reflect active bindings, and persist settings.
- **Player-facing outcome:** consistent Confirm/Cancel semantics; remappable
  keyboard and gamepad controls; an options menu; prompts that match the
  player's actual bindings.
- **Engineering outcome:** no raw-input reads in production states; navigation
  repeat, deadzone/hysteresis, transition-input suppression; active-device
  tracking; binding-derived prompt labels; versioned defensive settings
  persistence.
- **Primary deliverables:** semantic-action audit and removal of raw-input
  exceptions; stable navigation feel; binding-derived prompts; settings file in
  the user data dir; keyboard/gamepad remapping with conflict recovery; initial
  display/audio/gameplay/accessibility options.
- **Out of scope:** final UI theme; new gameplay actions beyond what current
  screens need; audio content.
- **Dependencies:** M12 layout primitives (settings/remapping screens are
  list-heavy).
- **Acceptance criteria:** complete keyboard-only and gamepad-only runs; D-pad
  and left stick navigate all UIs; remapping cannot strand the player;
  malformed settings and controller disconnect recover safely.
- **Automated validation:** binding-resolution and conflict tests; settings
  round-trip and malformed-file tests; full suite green.
- **Owner manual validation:** keyboard-only run, gamepad-only run, remap +
  prompt check, controller-disconnect check.
- **Milestone note:** `docs/milestone_notes/M13_input_settings.md`

## M14 — Asset manifest & replaceable resources

- **Status:** ☑ complete (approved by the owner 2026-07-19; base commit
  `67689b4`; schema v1 owner-approved 2026-07-19; 181/181 tests; see the
  note's §N)
- **Goal:** centralize presentation identity in a validated, versioned external
  asset manifest so states request logical IDs instead of file paths.
- **Player-facing outcome:** none directly (placeholders remain); groundwork
  that makes all later art/audio replaceable.
- **Engineering outcome:** approved manifest schema v1; raylib-free
  loader/validator; logical-ID `ResourceManager` interface; file-backed
  `AudioManager` with safe synthesized/silent fallback; no presentation paths
  in states.
- **Primary deliverables:** versioned `assets/manifest.json` schema;
  loader/validator with clear errors; texture/font/music/ambience/SFX
  metadata; manual development reload; packaging and attribution/license
  policy.
- **Out of scope:** producing final assets; art direction; final audio content.
- **Dependencies:** M11 asset-pipeline decision record; **owner approval of the
  manifest schema before it is frozen (public-schema decision)**.
- **Acceptance criteria:** replacing a texture, font, music track, or SFX for
  an existing role requires no C++ edit; duplicate/missing IDs, unsafe paths,
  and bad metadata are clear validation errors; missing optional assets do not
  crash; no state contains a presentation file path.
- **Automated validation:** manifest validation tests (duplicates, missing
  required IDs, unsafe paths, malformed metadata); fallback tests; full suite
  green.
- **Owner manual validation:** swap one placeholder asset via the manifest
  alone and confirm it appears; delete an optional asset and confirm safe
  fallback.
- **Milestone note:** `docs/milestone_notes/M14_asset_pipeline.md`

## M15 — Art-direction vertical slice

- **Status:** ☑ complete (approved) — approved by the owner on 2026-07-19
  after manual testing. The approval is the art-direction gate: the generated
  in-project direction (art bible + slice) is authorized for M17/M21
  production. (181/181 tests; captures in `docs/screenshots/m15_slice/`; see
  the note's §N.)
- **Goal:** prove the final visual/audio direction and its production cost on
  one coherent slice before creating the full asset set.
- **Player-facing outcome:** one polished path through the game: title, one
  town area/service, one compact Ruined Keep room, one normal battle, one boss
  battle, one result screen, with representative music/SFX.
- **Engineering outcome:** approved art bible and pixel-grid conventions; slice
  assets flowing entirely through the M14 manifest; validated production
  workflow and per-asset effort estimates.
- **Primary deliverables:** art bible; vertical-slice assets; representative
  manifest-driven music/SFX; readability and scaling review.
- **Out of scope:** full asset production; remaining themes and screens; any
  gameplay change.
- **Dependencies:** M12 layout, M14 manifest; coordination with M16 room-size
  ranges for the compact-room preview; owner approval of the art direction.
- **Acceptance criteria:** owner approves the art bible and the coherent slice;
  assets readable at native resolution and supported scaling modes; all assets
  original or correctly licensed; full production has not started before this
  gate.
- **Automated validation:** manifest/lint checks over slice assets; full suite
  green.
- **Owner manual validation:** review the slice at native resolution and
  representative window sizes; grayscale readability check; approve or reject
  the direction.
- **Milestone note:** `docs/milestone_notes/M15_art_direction_vertical_slice.md`

## M16 — Compact dungeon-room system

- **Status:** ☑ complete (approved) — approved by the owner on 2026-07-19
  after manual testing. (193/193 tests; mass validation of thousands of
  generated rooms; captures in `docs/screenshots/m16_rooms/`; see the note's
  §N. Owner decisions executed: optional `generationVersion` score field
  without a format bump; the 8 topology-derived archetypes.)
- **Goal:** separate dungeon topology from compact room realization and replace
  the fixed 26×15 full-screen room assumption.
- **Player-facing outcome:** compact, purposeful rooms with readable entrances,
  landmarks, and interactables instead of screen-filling boxes.
- **Engineering outcome:** room archetypes with data-defined bounded
  dimensions; a layout model (local tiles, door anchors, spawn points,
  collision, markers, prop zones, draw origin); derived room-local
  deterministic seeds; flood-fill/BFS validation.
- **Primary deliverables:** archetype and layout systems; room validation;
  score/generator-version decision if reproducibility semantics change;
  representative compact-room human testing.
- **Out of scope:** final environment art (M17); room events (M20); topology
  rule changes (gates, main path, boss placement stay as designed).
- **Dependencies:** M11 audit; M15 room-scale evidence; **owner decision on a
  generation-version/score-schema field if published seed semantics change**.
- **Acceptance criteria:** standard rooms no longer fill the exploration
  screen; at least five archetypes appear across representative runs; large
  generation samples pass connectivity and progression invariants; same seed
  plus generation version is stable.
- **Automated validation:** mass generation tests (thousands of rooms) for
  connectivity, reachability, spawn validity, and gate/boss invariants;
  determinism tests; full suite green.
- **Owner manual validation:** walk representative rooms of each archetype with
  keyboard and gamepad; confirm navigation and interactables are immediately
  legible.
- **Milestone note:** `docs/milestone_notes/M16_compact_dungeon_rooms.md`

## M17 — Exploration visuals & animation

- **Status:** ☑ complete (approved) — approved by the owner on 2026-07-19
  after manual testing. (201/201 tests; captures in
  `docs/screenshots/m17_explore/`; see the note's §N. Owner decisions
  executed: manifest v2 animation schema; all three themes in one pass with
  a single review.)
- **Goal:** produce final-quality town/dungeon characters, enemies, tiles,
  props, animations, effects, and theme atmosphere on the stable asset and
  room systems.
- **Player-facing outcome:** real art and animation in town and dungeons;
  visually distinct themes; readable interactables.
- **Engineering outcome:** animation metadata and renderer; visual bounds
  separated from collision bounds; stable deterministic draw layers;
  interaction indicators that do not rely on color alone.
- **Primary deliverables:** player and visible-enemy sprites; theme-specific
  environment sets; restrained glows/particles/overlays; animation fallback
  behavior.
- **Out of scope:** battle presentation (M18); audio content (M21); room-layout
  changes (M16 is done).
- **Dependencies:** M14 manifest, M15 approved art bible, M16 compact rooms.
- **Acceptance criteria:** silhouettes and interactables readable at native
  resolution; themes differ through shape/composition rather than hue alone;
  decoration does not obscure exits, hazards, enemies, or chests; collision is
  unaffected by texture dimensions; missing animations fall back safely.
- **Automated validation:** animation-metadata validation; asset lint;
  collision-independence tests; full suite green.
- **Owner manual validation:** walkthrough of each theme; grayscale readability
  check; decoration-density check.
- **Milestone note:** `docs/milestone_notes/M17_exploration_presentation.md`

## M18 — Battle presentation & game feel

- **Status:** ☑ complete (approved) — approved by the owner on 2026-07-19
  after manual testing. (208/208 tests, battle/simulator tests unmodified;
  live-battle captures in `docs/screenshots/m18_battle/`; see the note's §N.
  Owner decision executed: optional `effectFlash`/`effectShake` settings
  fields, no version bump.)
- **Goal:** make deterministic combat clear, fast, and satisfying without
  undermining fewest-turn score play.
- **Player-facing outcome:** readable battle information hierarchy; short
  impactful action feedback; configurable battle speed and effect intensity.
- **Engineering outcome:** presentation timing fully separated from simulation
  results; effect configuration honored through M13 settings; no change to
  deterministic outcomes.
- **Primary deliverables:** redesigned information hierarchy (active
  combatant/target/status/cost/telegraph); short action sequences, hit
  feedback, particles, KO/victory feedback; configurable flash, shake, and
  battle speed (Normal/Fast/near-instant).
- **Out of scope:** combat rule changes; balance tuning (M19/M23); audio
  content (M21).
- **Dependencies:** M12 layout, M13 settings, M14 manifest, M15 art direction.
- **Acceptance criteria:** source, target, effect, and result are always
  clear; Fast mode materially reduces downtime; reduced-effect options work;
  combat remains understandable muted; deterministic outcomes unchanged.
- **Automated validation:** existing battle/simulator tests stay green
  (identical outcomes); presentation-configuration tests; full suite green.
- **Owner manual validation:** battles at each speed with keyboard and gamepad;
  a status-heavy battle and a five-enemy battle; reduced-effect settings check.
- **Milestone note:** `docs/milestone_notes/M18_battle_presentation.md`

## M19 — Progression, economy & score-integrity hardening

- **Status:** ☑ complete (approved) — approved by the owner on 2026-07-19
  after manual testing. (216/216 tests; audit found no exploit loops and no
  tuning warranted — values unchanged; the depth>6 difficulty plateau is
  documented and deferred to M20; capture in `docs/screenshots/m19_score/`;
  see the note's §N. Owner decisions executed: `partyLevel` comparability
  tag + visible conditions; economy rules kept.)
- **Goal:** audit and tune the existing XP/leveling, Training Hall, shops,
  equipment, and scoring so progression supports replay without presenting
  incomparable runs as equivalent.
- **Player-facing outcome:** a tuned economy; visible, honest score-comparison
  conditions on the scoreboard.
- **Engineering outcome:** simulation-backed tuning evidence; an explicit
  score-comparability policy (segmented/tagged by run conditions rather than
  opaque normalization); values remain data-driven.
- **Primary deliverables:** XP/gold/purchase/failure simulations and human-run
  evidence; class and equipment growth review; exploit/dominant-path analysis;
  score-comparability policy; data-driven tuning changes.
- **Out of scope:** new content (M20); new mechanics; reimplementing XP.
- **Dependencies:** stable presentation (M12–M18); **owner approval for any
  scoreboard/save schema addition** (e.g., run-condition tags).
- **Acceptance criteria:** no obvious farming or economy exploit; Training
  Hall and battle XP have distinct, defensible roles; class identities remain
  meaningful; score conditions are visible and honest.
- **Automated validation:** expanded simulator reports; economy/XP-curve
  tests; scoreboard compatibility tests; full suite green.
- **Owner manual validation:** progression sessions attempting the identified
  exploit paths; scoreboard comprehension check.
- **Milestone note:** `docs/milestone_notes/M19_progression_economy_scoring.md`

## M20 — Encounter & dungeon-content variety

- **Status:** ☑ complete (approved) — approved by the owner on 2026-07-20
  after manual testing. (228/228 tests; the depth plateau is fixed —
  clearing levels now 1/3/5/9/11 at depths 1/5/6/8/10; captures in
  `docs/screenshots/m20_events/`; see the note's §N. Owner decisions
  executed in full: role taxonomy + composition.json + depth scaling +
  generation v3; all six events; one sim mechanic per boss archetype.)
- **Goal:** add targeted tactical variety after the presentation and room
  foundations are stable.
- **Player-facing outcome:** more distinct enemy teams, a small set of
  meaningful room events with visible trade-offs, and mechanically
  distinguishable bosses.
- **Engineering outcome:** encounter-role taxonomy; externalized composition
  constraints; event system built on M16 rooms; boss mechanic/telegraph/
  escalation improvements.
- **Primary deliverables:** role taxonomy and composition rules in data; a
  limited room-event set; dungeon modifiers only where risk/reward is clear;
  boss identity improvements.
- **Out of scope:** quantity-first content production; new game modes; story
  content.
- **Dependencies:** M16 rooms; M17/M18 presentation; M19 scoring policy.
- **Acceptance criteria:** generated teams cannot violate composition rules;
  events create real decisions with risk/reward visible before confirmation;
  representative bosses are mechanically distinguishable; new content fills
  identified role gaps rather than chasing quantity.
- **Automated validation:** composition-constraint validation; generation
  tests; content lint; full suite green.
- **Owner manual validation:** representative runs across themes/depths; blind
  boss-distinction check.
- **Milestone note:** `docs/milestone_notes/M20_content_variety.md`

## M21 — Final music, ambience & sound effects

- **Status:** ☑ complete (approved) — approved by the owner on 2026-07-20
  after the manual listening pass. (235/235 tests; 30 original WAVs — 11
  music incl. per-theme dungeon tracks and one-shot victory/defeat
  jingles, 4 ambience beds, 15 SFX — all deterministic, manifest-driven,
  provenance-recorded; ambience channel + crossfades + SFX rate limiting
  in `AudioManager`; smoke-tested incl. the missing-file drill; see the
  note's §N. Owner decisions 2026-07-20: in-project generated sourcing;
  per-theme dungeon music; fanfare/dirge jingles at battle end.)
- **Goal:** replace synthesized placeholders with coherent original/licensed
  audio delivered through the asset manifest.
- **Player-facing outcome:** real music, ambience, and sound effects across
  every scene, with persistent volume controls.
- **Engineering outcome:** streamed music; clean loops and transitions; mixing;
  safe silence fallback; provenance/license records for every shipped asset.
- **Primary deliverables:**
  title/town/preparation/dungeon/battle/boss/victory/defeat/result music
  states; town and per-theme ambience; complete UI/exploration/battle SFX
  families; mixing and persistent volume controls.
- **Out of scope:** gameplay changes; new audio-driven mechanics.
- **Dependencies:** M14 manifest; M13 volume settings; M15 approved direction.
- **Acceptance criteria:** no stacked tracks, clicks, clipping, or hard
  crashes; audio replaceable without C++; the game remains understandable
  muted; every shipped asset has provenance and license/attribution records.
- **Automated validation:** manifest audio validation; missing-audio fallback
  tests; full suite green.
- **Owner manual validation:** listen through all scenes and transitions; loop
  seam check; maximum-volume clipping check; full run with audio muted.
- **Milestone note:** `docs/milestone_notes/M21_audio_music.md`

## M22 — Onboarding & accessibility

- **Status:** ☑ complete (approved) — approved by the owner on 2026-07-20
  after manual testing. (244/244 tests; 9 one-time contextual prompts
  persisted in tutorial.json with disable/reset in Settings; remappable
  Details action with contextual panels in
  battle/dungeon/result/scoreboard/equip shop; high-contrast palette
  behind style::palette(); save-overwrite and quit-to-title
  second-Confirm guards; see the note's §N. Owner decisions 2026-07-20:
  contextual one-time prompts; own tutorial.json; Details action +
  palette accessor approved.)
- **Goal:** teach the game through play and remove avoidable barriers while
  retaining tactical challenge.
- **Player-facing outcome:** progressive contextual onboarding; in-game
  Details help; high-contrast and reduced-motion options; configurable message
  pacing.
- **Engineering outcome:** defensively persisted, resettable tutorial state;
  accessibility options honored across all screens; non-color and non-audio
  information alternatives.
- **Primary deliverables:** progressive onboarding;
  revisit/disable/reset tutorial controls; Details help for
  stats/status/danger/score/equipment; high-contrast and reduced
  motion/flash/shake options; message pacing; clear error and
  destructive-action handling.
- **Out of scope:** difficulty redesign; new mechanics; balance changes.
- **Dependencies:** M13 settings; M12 layout; M18 battle presentation.
- **Acceptance criteria:** a new tester completes depth 1 without external
  instruction; critical mechanics are explained in-game; accessibility cases
  pass the manual matrix; settings are reachable before New Game.
- **Automated validation:** tutorial-state persistence and reset tests;
  settings tests; full suite green.
- **Owner manual validation:** fresh-player observation; accessibility-option
  runs (high contrast, reduced motion, pacing).
- **Milestone note:** `docs/milestone_notes/M22_onboarding_accessibility.md`

## M23 — Automated visual validation, playtesting & balance hardening

- **Status:** ☐ planned — **deferred on 2026-07-20; runs after M30.** The
  tooling, diagnostics, lint/mass/report suites, and sim-justified early-ramp
  tuning (generation v4) are already implemented and remain in the tree; they
  are not re-work. What changed is sequencing: playtesting a build with
  known-stale gameplay and placeholder-grade enemy art would produce findings
  about problems M25–M30 already exist to fix. Re-audit this note against the
  post-M30 checkout before starting — the capture scene list and balance
  battery will both need extending for the new AI, content, and art.
- **Goal:** make representative presentation states reproducible, prevent
  layout/asset/room/balance regressions, and harden balance with observed
  external playtesting evidence.
- **Player-facing outcome:** fixes for the control, readability, navigation,
  score-comprehension, and balance problems that playtests surface.
- **Engineering outcome:** deterministic debug/capture scenarios;
  native-resolution screenshot capture; UI bounds/overlap/overflow
  diagnostics; presentation-content linting; large-scale room validation;
  expanded machine-readable simulation reports.
- **Primary deliverables:** the validation tooling above; observed
  keyboard-first, controller-first, and new-player playtests; final balance
  and pacing changes backed by evidence.
- **Out of scope:** release packaging (M24); new features or content.
- **Dependencies:** M12–M22 substantially complete so representative captures
  and playtests reflect the real game.
- **Acceptance criteria:** unintended overflow and missing required assets
  fail validation; representative captures are reproducible; thousands of
  generated rooms pass invariants; repeated usability and balance defects from
  playtests are fixed, not documented away; no dominant strategy trivializes
  representative runs; debug tooling is excluded or safely disabled in
  Release.
- **Automated validation:** the new validation suites themselves plus the full
  test suite, all green.
- **Owner manual validation:** run/observe the external playtests; review the
  capture set and balance reports; approve the fixes.
- **Milestone note:** `docs/milestone_notes/M23_validation_playtesting_balance.md`

## M24 — Release packaging & final release validation

- **Status:** ☐ planned — **deferred on 2026-07-20; runs last, after M23.**
  The packaging engineering is already implemented and stays in the tree
  (presets with static CRT, version 0.9.0 plumbing, generated icon +
  VERSIONINFO, one-command stage/validate/zip via tools/package.ps1;
  package smoke-tested, capture-inert; owner decisions 2026-07-20: v0.9.0
  until playtests pass, plain zip, emblem-generated icon). Only the final
  validation and sign-off are deferred. Note that M25–M30 add assets and
  content, so the packaging manifest coverage and package-size expectations
  must be re-checked before sign-off.
- **Goal:** produce and validate a reproducible, clean, polished Windows
  release candidate.
- **Player-facing outcome:** the final packaged game.
- **Engineering outcome:** reproducible Release preset/build; packaged `data/`
  and `assets/` trees; version metadata, icon, credits, and licenses; clean
  logs; stable save/settings paths.
- **Primary deliverables:** Release build configuration/preset; packaged
  distribution layout; credits/licenses; known limitations; clean-machine
  smoke test; final manual-test-matrix sign-off.
- **Out of scope:** feature, content, or balance changes other than
  release-blocking fixes.
- **Dependencies:** M23 complete; all prior milestones `complete (approved)`.
- **Acceptance criteria:** packaged build runs on a machine without Visual
  Studio; no debug presentation, unlicensed asset, or unintended placeholder
  ships; final manual matrix signed off; the owner explicitly approves the
  release candidate.
- **Automated validation:** Release-configuration build plus full test suite;
  packaging/layout validation.
- **Owner manual validation:** clean-machine smoke test; final manual-matrix
  pass; release-candidate approval.
- **Milestone note:** `docs/milestone_notes/M24_release_packaging.md`

## Polish program (M25–M30)

Authorized by the owner on 2026-07-20 after a hands-on review found the game
not release-ready. Ten reported defects grouped into six milestones, ordered so
that no work is designed against a system that is about to change: presentation
corrections first, then art identity, then the combat-rules change, then content
built for the new rules, then economy. Strategic framing lives in
`docs/completion_roadmap.md` §12; per-milestone scope lives in the notes.

Owner decisions taken at planning time (2026-07-20):

- **Battle-rules comparability:** `ScoreEntry` gains a `battleRulesVersion`
  field alongside `generationVersion`. `generationVersion` covers what a seed
  *generates*; it does not cover how a battle *resolves*, and M28 changes the
  latter for every encounter. Entries written before the field read as v0.
- **Skill delivery:** classes gain a level-based learnset. Expanding
  `startingSkills` was rejected — it hands every skill over at character
  creation, bloating the battle menu and adding no progression.
- **Ambience volume:** routed to the existing SFX slider. No new Settings
  field, no save migration.
- **Numbering:** new work is M25–M30; M23/M24 keep their numbers and run last.
  Execution order is recorded in the status table above.

**Note authoring is deliberately just-in-time.** Only
`M25_ui_corrections_battle_hud.md` exists so far; the notes for M26–M30 are
authored when the owner authorizes each milestone. A detailed note written six
milestones ahead describes a repository that will not exist by the time anyone
reads it, and the workflow already requires re-auditing a note against the
current checkout before implementing from it. The scope summaries below are the
commitment; each note adds the detail once the ground under it is stable.

## M25 — UI corrections & battle HUD

- **Status:** implemented, awaiting manual approval (owner authorized
  2026-07-20; scope amended to add a font-replacement slice — see the note's
  §A.1 and as-implemented record §N. 252/252 tests; 23/23 capture scenes
  overflow-clean; Debug + Release both build clean)
- **Goal:** fix the readability defects that make the game look unfinished, and
  replace raylib's default font with an original bitmap font, at zero
  simulation risk.
- **Player-facing outcome:** no overlapping title text; battle sprites no
  longer buried under always-on name labels; MP legible as numbers; Guild
  values sit next to the controls that change them.
- **Primary deliverables:**
  - Title screen: resolve the version/content-line overlap (version at
    `(4, h-12)` size 8 vs content at `(6, h-16)` size 10) and gate the content
    line out of Release builds via the existing `CRYSTAL_SHIPPING_BUILD` guard.
  - Battle: remove the unconditional name label above every sprite
    (`BattleState.cpp`); surface name plus basic target info (HP, and the stats
    needed to judge a target) only for the **currently targeted** unit.
  - Battle: current/max MP as numerals for party members, matching the existing
    HP numeral treatment.
  - Guild: Theme/Depth/Seed values rendered inline with their menu rows,
    following the `SettingsState::volumeLabel` idiom (`MenuItem` carries no
    value field; values are composed into the label on rebuild).
- **Out of scope:** any battle-rule, targeting, or content change; new art.
- **Dependencies:** none. Can start immediately.
- **Acceptance criteria:** no text overlap at 426x240 in any capture scene; the
  content line is absent from a Release build and non-overlapping in Debug;
  target info appears only on the targeted unit; MP numerals present; Guild
  values adjacent to their controls.
- **Automated validation:** full suite; capture run must stay overflow-clean.
- **Owner manual validation:** title screen legibility; battle readability with
  a full party and five enemies; Guild adjust feel.
- **Milestone note:** `docs/milestone_notes/M25_ui_corrections_battle_hud.md`

## M26 — Enemy visual identity

- **Status:** planned
- **Goal:** give every enemy and boss its own silhouette.
- **Context:** the code already resolves `enemy.<sourceId>.battle` and
  `boss.<sourceId>.battle` before falling back to the tier sprite
  (`BattleState.cpp`). The manifest ships only `enemy.normal.battle`,
  `enemy.elite.battle`, and `boss.generic.battle`, so 23 enemies and 4 bosses
  collapse onto three images. **This milestone needs no gameplay code.**
- **Primary deliverables:** distinct battle sprites for all 16 normal enemies,
  7 elites, and 4 bosses, produced by `tools/asset_gen/`; manifest rows;
  `assets/credits.md` provenance; tier fallbacks retained for future ids that
  do not yet have art.
- **Definition-of-done guard:** extend `tests/test_presentation_lint.cpp` so a
  content enemy/boss id without a matching manifest sprite **fails the suite**,
  rather than silently degrading to the tier image.
- **Out of scope:** new enemies (M29); overworld/dungeon markers; animation.
- **Dependencies:** none, but should precede M29 so new content inherits the
  established art language.
- **Acceptance criteria:** every shipped enemy and boss id resolves to its own
  sprite; lint fails on a deliberately removed row; generators rerun
  byte-identical.
- **Owner manual validation:** visual distinctness and tier readability at
  426x240; art direction acceptance.
- **Milestone note:** `docs/milestone_notes/M26_enemy_visual_identity.md`

## M27 — Environment & ambience identity

- **Status:** planned
- **Goal:** make town services and dungeon themes feel like distinct places.
- **Context:** all six town service states draw a single
  `ClearBackground(solid)`. Per-theme ambience *is* correctly wired
  (`DungeonState::themeAmbience` maps mine/forest/keep) — the beds simply sound
  too alike, and ambience volume is routed through `musicVolume_`
  (`AudioManager.cpp`), so the Music slider controls it and no SFX-side control
  exists.
- **Primary deliverables:** non-intrusive background art for Inn, Item Shop,
  Equip Shop, Training Hall, Scoreboard, and Guild — legibility of overlaid
  text is the binding constraint; genuinely differentiated ambience beds per
  theme; ambience volume routed to `sfxVolume`.
- **Out of scope:** town tilemap/layout changes; new music tracks; a dedicated
  ambience slider (explicitly rejected at planning).
- **Dependencies:** none.
- **Acceptance criteria:** every service screen keeps all text within contrast
  and overflow budgets over its new background; the three theme beds are
  distinguishable in a blind listen; the SFX slider demonstrably moves ambience
  and the Music slider no longer does.
- **Owner manual validation:** background intrusiveness, audio distinctness and
  mix balance.
- **Milestone note:** `docs/milestone_notes/M27_environment_ambience.md`

## M28 — Enmity, AI diversity & control skills

- **Status:** planned
- **Highest-risk milestone in the project.** It changes the outcome of every
  battle, invalidates the tuned balance battery, and makes prior scores
  incomparable.
- **Goal:** replace predictable targeting with a threat model the player can
  read and influence but not trivially exploit.
- **Context:** `chooseEnemyAction` (`Battle.cpp`) scans for the lowest-HP living
  party member with a deterministic index tie-break. There is no enmity concept
  anywhere in the codebase. The consequence the owner reported: playing a mage
  efficiently turns the mage into the tank.
- **Primary deliverables:**
  - An enmity/threat model in the pure sim — threat accrued from damage dealt,
    healing done, and skill use, decaying over time.
  - Per-enemy behaviour profiles (e.g. aggressive / opportunist / tactician /
    protector) so different enemies weigh threat, low HP, and role differently.
  - Seeded per-battle variance: unpredictable to the player, **fully
    deterministic** given seed and inputs — the simulator and live play must
    still agree exactly, as they do today via shared `tickStatuses`.
  - New enmity-control skills (taunt / redirect / fade) giving deliberate
    aggro management.
  - `ScoreEntry.battleRulesVersion` + scoreboard display/filtering; pre-field
    entries read as v0.
  - Re-tuned balance battery and updated `[economy-report]` / `[sim-report]`
    expectations.
- **Out of scope:** new enemies or bosses (M29); boss mechanic rewrites beyond
  what enmity requires.
- **Dependencies:** M25 (targeting UI must already show target info, since
  enmity is unreadable without it).
- **Acceptance criteria:** identical seed + identical inputs still reproduce a
  battle exactly; simulator and live play agree; no single-character
  degenerate-tank strategy survives; taunt/redirect measurably redirect
  targeting; scoreboard distinguishes rules versions.
- **Owner manual validation:** whether battles feel less predictable without
  feeling arbitrary; whether enmity is legible in play; difficulty balance.
- **Milestone note:** `docs/milestone_notes/M28_enmity_ai.md`

## M29 — Content expansion & class learnsets

- **Status:** planned
- **Goal:** more enemies, more bosses, and per-class skill growth — designed for
  the M28 enmity roles, shipped at M26 art standard.
- **Context:** current content is 16 normal enemies, 7 elites, 4 bosses, 28
  skills. Each class has exactly 3 `startingSkills` and there is **no
  level-based skill learning** — skills reach the player only at creation or via
  3 chest scrolls.
- **Primary deliverables:** additional enemies and bosses with distinct enmity
  behaviour profiles and their own sprites; a per-class learnset (new content
  schema field) granting skills at set levels; new skills per class; content
  validator and loader coverage for the learnset.
- **Out of scope:** changing the enmity model itself (M28); economy changes
  (M30).
- **Dependencies:** M26 (art language) and M28 (behaviour profiles) — both must
  be `complete (approved)` first, or this work is designed against systems that
  then change under it.
- **Acceptance criteria:** every new enemy/boss has its own sprite and a
  declared behaviour profile; learnset validates and round-trips through saves;
  no existing save breaks; battle menus stay within layout budgets as skill
  counts grow.
- **Owner manual validation:** whether the additions feel varied; whether skill
  pacing across levels is satisfying; balance.
- **Milestone note:** `docs/milestone_notes/M29_content_learnsets.md`

## M30 — Economy: paid rest & rest-token event

- **Status:** planned
- **Goal:** make resting a real economic decision, and reward exploration with
  relief from it.
- **Context:** `InnState` is a bare `healFull(context_.party)` with no cost.
  `RoomEventKind` already supports Shrine, HealingSpring, Merchant,
  EliteChallenge, and ScoreWager, so the quest-item event extends a proven
  system rather than introducing one.
- **Primary deliverables:** gold cost for the inn (scaling rule to be decided in
  the note, with the owner); a new room event granting a one-time free-rest
  item; inventory/save support for that item; UI showing cost and token state.
- **Out of scope:** wider shop or reward-curve rebalancing.
- **Dependencies:** M28 and M29 — gold income depends on battle outcomes and
  content, so tuning a cost before those settle would be tuning against a
  moving target.
- **Acceptance criteria:** inn cost is affordable but meaningful across depths
  per simulation; the token grants exactly one free rest and persists across
  save/load; declining a paid rest is never a soft-lock.
- **Owner manual validation:** whether the cost changes decision-making without
  becoming a grind; event discovery rate.
- **Milestone note:** `docs/milestone_notes/M30_economy_rest.md`
