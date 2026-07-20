# Crystal Dungeons — Completion and Presentation Roadmap

> Strategic roadmap for the post-M10 completion program.
>
> Baseline reviewed: commit `e293f49f35ddd3bd4c49202194cadd96aead7811`.
> Last planning review: commit `a316f244e870718aa27d9995dc871e11572ad429`
> (2026-07-19). M10 was manually tested and **approved by the owner on
> 2026-07-19**; the program below (M11–M24) is active planning.
>
> This document defines direction and quality bars. It does **not** authorize
> implementation from later milestones. `docs/milestones.md` and the active
> milestone note control current scope. Each milestone M11–M24 has exactly one
> authoritative note under `docs/milestone_notes/`, indexed from
> `docs/milestones.md`.

## 1. Mission

Turn the current feature-complete prototype into a cohesive, readable,
configurable, and polished game without discarding its proven systems or
expanding it into an unfocused story JRPG.

The finished game should retain the existing core loop:

> Town → prepare party → choose a seeded dungeon → explore visible encounters →
> clear gates and optional rewards → defeat the boss → receive a turn-efficiency
> score → upgrade → repeat.

The completion program must materially improve:

- visual quality and artistic cohesion;
- text readability and layout safety;
- keyboard and controller usability;
- dungeon room scale and spatial composition;
- battle readability and game feel;
- progression, economy, and score integrity;
- onboarding and accessibility;
- replaceability of graphics, fonts, music, ambience, and sound effects;
- automated validation and release quality.

This is not permission to rewrite the engine, replace the deterministic core,
add online services, or create a large narrative campaign.

## 2. Baseline at the reviewed commit

The M1–M10 implementation provides a strong systems foundation:

- C++20, CMake, raylib 6.0, Catch2, and nlohmann/json;
- a 426×240 virtual screen with aspect-preserving scaling;
- a state stack with queued transitions;
- deterministic dungeon generation and battle simulation;
- JSON-defined gameplay content with validation;
- versioned defensive saves and a persistent scoreboard;
- keyboard and gamepad action mapping;
- cached RAII-owned textures and fonts with fallbacks;
- synthesized placeholder music and sound effects;
- XP, leveling, equipment, shops, scoring, and a complete playable loop;
- 125 documented tests at the baseline commit.

The presentation architecture is still prototype-level:

- `src/ui/UiDraw.*` exposes only simple panel, centered-text, and menu drawing
  helpers; screens commonly place text directly with fixed coordinates;
- `InputAction` contains a small fixed action vocabulary and there is no
  player-facing remapping/settings system or dynamic prompt generation;
- `ResourceManager` still requires callers to provide both a cache key and a
  file path, so asset selection is not yet centralized in a catalog;
- `assets/` is empty and `AudioManager` synthesizes all audio at runtime;
- `DungeonState` hard-codes a 26×15 room at 16-pixel tiles, causing each room to
  occupy effectively the entire 426×240 exploration surface;
- dungeon markers, characters, enemies, environments, and most effects are
  rectangles, glyphs, or minimal procedural feedback.

These are not small cosmetic defects. They are structural presentation gaps.
Adding final art before fixing layout, asset identity, room composition, and
control feedback would create avoidable rework.

## 3. Authority and execution model

Document authority for implementation (matching `CLAUDE.md`):

1. `CLAUDE.md` — project operating contract and non-negotiable rules.
2. The approved active `docs/milestone_notes/MXX_*.md` — exact scope for the
   current milestone.
3. `docs/milestones.md` — milestone ledger, historical record, and status.
4. `docs/game_design.md` — authoritative player-facing game rules.
5. `docs/technical_design.md` — authoritative architecture and engineering
   constraints.
6. This roadmap — long-term direction and quality targets.
7. Supporting authoring, validation, control, asset, and test documents.

When documents conflict: identify the conflict, apply the authority order where
it resolves the issue, escalate material product or architecture conflicts to
the human, and update the stale document afterward. Do not silently choose the
broader or more expensive interpretation.

Every milestone must be:

- independently playable;
- reviewable and revertible;
- small enough to audit;
- backed by automated tests where logic can be tested headlessly;
- backed by explicit human validation where appearance, feel, or audio matters;
- completed and approved before work begins on the next milestone.

## 4. Non-negotiable product requirements

### 4.1 All required text must be visible

No required player-facing text may unintentionally:

- extend beyond the virtual viewport;
- extend beyond its assigned panel or content rectangle;
- overlap unrelated content;
- render beneath a modal, footer, or focus indicator;
- be truncated without an explicit and understandable continuation mechanism;
- depend on guessed character widths;
- become unreadable against variable art.

Every text-bearing component must use measured text bounds and one documented
overflow policy:

- wrap;
- scroll;
- paginate;
- resize within a narrow approved range;
- abbreviate through an authored short label;
- ellipsize only when the complete value is available through Details/tooltip.

Critical instructions, descriptions, error messages, score explanations, skill
costs, status effects, save metadata, and destructive confirmations must never
be silently ellipsized.

Use WCAG 2.2 contrast values as practical engineering targets, not as a claim of
formal web conformance:

- normal text: at least 4.5:1 against its effective background;
- large text: at least 3:1;
- meaningful non-text UI, selection, and focus indicators: at least 3:1 against
  adjacent colors.

Text must be backed by a stable panel, shadow, outline, or other treatment when
art behind it is not contrast-safe. Color must not be the only indicator of
selection, danger, rarity, status, or availability.

### 4.2 Controls must be intuitive and consistent

The complete game must be fully playable with:

- keyboard only;
- gamepad only;
- D-pad or left analog stick for menu navigation;
- mouse only as an optional convenience, never a requirement.

Default semantic conventions:

- Move/navigate: arrows, WASD, D-pad, or left stick.
- Confirm/interact: Enter, Space, or primary face button.
- Cancel/back: Escape, Backspace, or secondary face button.
- Pause/menu: one consistent action and prompt.
- Page/tab movement: shoulder buttons where useful, with directional fallback.
- Details: one consistent semantic action where extra information exists.

Confirm and Cancel must never reverse meanings between screens. A state
transition must not consume a buffered Confirm and immediately trigger an
unintended action in the next state.

Prompts and help must be generated from the active bindings. Hard-coded strings
such as `A`, `B`, `Enter`, or `Esc` are not acceptable once remapping exists.

### 4.3 Dungeon rooms must be compact and purposeful

The current 26×15 fixed room is a prototype constraint, not a product
requirement.

Standard rooms should no longer fill the entire exploration screen. Initial
playtest ranges:

- common chambers: approximately 9–15 tiles wide and 7–11 tiles high;
- corridors/connector spaces: approximately 5–9 tiles wide and 7–11 tiles high;
- special and boss rooms: larger only when the encounter or composition needs
  it, commonly no more than about 19×13 tiles at the current tile scale.

These numbers are starting ranges, not immutable rules. The actual decision
must be validated in a vertical slice.

Each room must have a gameplay purpose and a deliberate composition:

- a readable entrance/exit rhythm;
- a focal landmark or obstacle arrangement;
- clear interactable silhouettes;
- limited empty traversal with no decision or atmosphere value;
- collision and spawn validation;
- an intentional relationship with the surrounding HUD and minimap.

Topology and room geometry must be separate systems. The existing dungeon graph
continues to decide connectivity, gates, branches, boss placement, and rewards.
A room-layout layer decides dimensions, door anchors, internal collision,
markers, props, spawn points, and camera framing.

### 4.4 Presentation assets must be replaceable without gameplay-code edits

Gameplay and state code must request stable logical IDs, not arbitrary paths.
Examples:

- `ui.frame.default`
- `ui.cursor.primary`
- `font.ui.body`
- `tiles.ruined_keep.floor_a`
- `actor.knight.overworld`
- `enemy.crystal_slime.battle`
- `music.town.day`
- `music.dungeon.crystal_mine`
- `sfx.ui.confirm`

An external, versioned asset catalog must map those IDs to files and metadata.
Replacing a texture, font, music track, ambience loop, or sound effect must not
require editing C++ when the logical role is unchanged.

Missing or invalid assets must:

- produce a clear development diagnostic;
- use an obvious visual fallback or safe silence;
- preserve resource lifetime correctness;
- never crash the game;
- never read arbitrary untrusted paths.

### 4.5 Maintain the focused game identity

Do not use “more complete” as justification for uncontrolled feature growth.
Prioritize the quality of the existing loop over raw content count.

Do not add:

- a large story campaign;
- open-world traversal;
- real-time action combat;
- online accounts, leaderboards, telemetry, or network play;
- a general-purpose editor or scripting language;
- a full ECS rewrite;
- a large meta-progression tree before the existing replay loop is proven.

## 5. Engineering guardrails

### 5.1 Extend; do not rewrite

Preserve the established boundaries between simulation, rendering, input,
resources, content, saves, UI states, battle, and dungeon generation.

A rewrite is allowed only when the current design blocks a milestone and a
smaller migration has been demonstrated insufficient.

### 5.2 Keep presentation data separate from gameplay data

Gameplay definitions such as damage, XP, encounter composition, and item stats
remain under `data/` and the content model.

Presentation definitions such as asset files, sprite frame rectangles,
animation timing, UI theme values, and audio mixing metadata belong under
`assets/` or a clearly separate presentation-data directory.

Changing art must not accidentally change combat or generation behavior.

### 5.3 Preserve determinism deliberately

Dungeon and battle determinism must remain explicit and tested.

Room realization should use a derived local seed such as a stable hash of:

- dungeon seed;
- generation/layout version;
- room index;
- room archetype.

Do not consume additional calls from the topology RNG merely to decorate a
room. That would make unrelated presentation changes alter graph generation.

If room generation changes the meaning of a published seed, introduce a
reviewed generation-version policy. Score entries may need an optional
`generationVersion` field so future runs can be reproduced and compared. This
is a schema decision and requires human approval.

### 5.4 Use RAII and stable handles for resources

Continue using RAII for raylib resources. Centralize ownership. Do not retain
references or raw handles across catalog reloads unless the handle model
explicitly guarantees stability.

Development hot reload may be manual. A reliable Reload Assets command is
higher value and lower risk than an elaborate file-watcher dependency.

### 5.5 Test pure policy, not raylib drawing calls

Keep layout, wrapping, focus navigation, room connectivity, asset validation,
and settings validation as pure logic where possible.

Raylib-dependent drawing remains manually and visually validated. Do not force a
GPU/window dependency into the headless unit-test target just to test geometry.

### 5.6 Avoid per-frame allocation in hot paths

Precompute or reuse:

- wrapped text lines;
- menu display strings;
- sprite frame metadata;
- room draw layers;
- animation buffers;
- particle pools where needed.

Do not optimize blindly. Profile or inspect first, but do not introduce obvious
allocation churn in every-frame render/update loops.

### 5.7 No new dependencies by default

The current stack is sufficient for the planned work. Asset catalogs, settings,
layout policy, and animation metadata can use the existing JSON library.

Any new dependency needs a concrete capability, cost comparison, license check,
and human approval.

## 6. Milestone execution protocol for Claude Code

At the beginning of a milestone:

1. Read the authority documents and active milestone note.
2. Inspect the current implementation; do not plan from filenames alone.
3. State the baseline commit being used.
4. Identify decisions that require human approval.
5. Produce a slice plan with expected files, schema/save implications, tests,
   manual validation, and rollback risk.
6. Stop for approval when the active note or `CLAUDE.md` requires it.

During implementation:

- work only on the approved slice;
- keep unrelated refactors out;
- add tests with each pure subsystem;
- update docs in the same slice when behavior or contracts change;
- use placeholders only through the approved asset interfaces;
- do not declare visual quality from tests alone.

At completion:

- report exact files changed;
- list build/test commands actually run;
- provide exact results, including test count;
- identify unverified claims;
- provide a focused human validation checklist;
- attach or identify screenshots for visual work;
- state whether acceptance criteria are fully met;
- stop and wait for approval.

## 7. Milestone roadmap

### M11 — Completion baseline and presentation audit

**Goal:** establish a trustworthy visual/usability baseline and convert vague
polish goals into a prioritized defect register.

Deliverables:

- build/test verification at the approved M10 baseline;
- screen inventory for all main states and modals;
- screenshots at native and representative window sizes where tooling permits;
- presentation defect register categorized by severity and subsystem;
- initial UI style guide, control standard, asset-pipeline decision record, and
  manual test matrix;
- confirmed list of player-facing strings/values that can become long or
  dynamic;
- no product code changes unless separately approved.

Acceptance:

- every major screen and flow is represented;
- every known clipping/overlap/focus/control ambiguity is recorded;
- the next UI slice is selected from evidence rather than assumption;
- build/tests are verified or explicitly reported as unverified;
- human approves the defect priorities.

### M12 — UI layout and text-safety foundation

**Goal:** replace fixed-coordinate text assumptions with a small, game-specific,
measured layout foundation.

Recommended slices:

1. **M12-a — Text measurement and wrapping**
   - font-aware measurement;
   - explicit line height and spacing;
   - bounded wrapping;
   - long-token handling;
   - overflow diagnostics;
   - migrate Help and one representative modal.

2. **M12-b — Layout primitives**
   - rectangles, insets, alignment, vertical/horizontal stacks;
   - padded panels;
   - list viewport and scroll model;
   - safe-area and footer reservation;
   - focus visibility contract.

3. **M12-c — Typography and UI theme**
   - typography roles;
   - spacing scale;
   - panel/focus/disabled/danger styles;
   - contrast validation helper or documented manual check;
   - no color-only state.

4. **M12-d onward — Screen-family migrations**
   - title/help/settings shell;
   - party creation/save/load;
   - town services;
   - guild/dungeon HUD;
   - battle;
   - result/scoreboard.

Required architecture:

- build only the primitives this game uses; do not create a browser-style UI
  framework;
- measure with the actual selected font;
- keep layout policy testable without opening a window where practical;
- support wrapping, scrolling, paging, and deliberate ellipsis policies;
- add a debug overlay/log for assigned-vs-rendered bounds.

Acceptance:

- zero unintended text overflow in the manual matrix;
- every variable-length list scrolls or paginates;
- selected controls remain visible;
- disabled actions explain why when the reason is not obvious;
- long-content tests cover every migrated screen family;
- the 426×240 baseline remains unless a separate resolution decision is
  approved.

**Resolution warning:** moving to 640×360 may improve UI and asset production,
but it changes every composition and should not be smuggled into a text-layout
slice. First prove whether 426×240 can meet the approved content and legibility
requirements.

### M13 — Input consistency, remapping, and settings

**Goal:** make the game predictable with keyboard and controller and make all
prompts reflect active bindings.

Recommended slices:

1. audit and remove direct raw input reads outside the platform adapter;
2. expand the semantic action vocabulary only where current screens need it;
3. implement menu navigation repeat, analog deadzone/hysteresis, and
   transition-input suppression;
4. track the most recently intentional input device;
5. generate action labels/prompts from bindings;
6. add settings persistence with defensive loading;
7. add keyboard and gamepad remapping with conflict handling;
8. add display/audio/gameplay accessibility settings that existing systems can
   honor.

Minimum settings target:

- fullscreen/windowed and scale mode;
- master/music/SFX volume;
- battle/message speed;
- screen shake off/reduced/full once shake exists;
- flash intensity off/reduced/full once flashes exist;
- high-contrast UI option;
- reset bindings and settings to defaults.

Acceptance:

- complete keyboard-only and gamepad-only runs;
- D-pad and left stick both navigate all UIs;
- Confirm/Cancel behavior is consistent;
- prompts update after remapping;
- binding conflicts cannot leave the UI unusable;
- malformed settings fall back safely;
- controller disconnect falls back without locking the current screen.

### M14 — Asset manifest and replaceable resources

**Goal:** make presentation roles external, validated, and swappable.

Recommended catalog responsibilities:

- logical ID → file path;
- asset type;
- texture filtering and optional atlas/frame metadata;
- font base size and glyph coverage policy;
- music loop and default volume metadata;
- sound default volume and optional variation set;
- theme/pack overrides;
- catalog version.

Recommended directory shape:

```text
assets/
  manifest.json
  credits.md
  themes/
  fonts/
  textures/
    actors/
    enemies/
    environments/
    effects/
    ui/
  audio/
    music/
    ambience/
    sfx/
```

Required work:

- define and approve the version-1 manifest schema;
- create a raylib-free catalog loader/validator;
- evolve `ResourceManager` so states request logical IDs only;
- evolve `AudioManager` to support real streaming music and loaded SFX while
  retaining safe fallback behavior;
- provide generated checkerboard/default-font/silence fallbacks;
- add a development-only manual asset reload;
- copy the assets tree next to the executable in build and release layouts;
- record origin/license/attribution for every shipped external asset.

Acceptance:

- changing town music requires no C++ edit;
- changing a player/enemy/environment texture requires no C++ edit;
- switching a theme mapping requires no gameplay-code edit;
- duplicate IDs, missing required IDs, unsafe paths, and bad metadata are clear
  validation errors;
- missing optional assets do not crash;
- no state contains a presentation file path.

### M15 — Art-direction vertical slice

**Goal:** prove one coherent final-quality presentation slice before producing a
large asset set.

Create an art bible covering:

- mood and visual pillars;
- palette strategy and crystal accent language;
- authoritative pixel grid and tile size;
- character and enemy silhouette scale;
- outline and shading rules;
- animation frame conventions;
- UI ornament and icon rules;
- environmental composition;
- readable interaction hierarchy;
- prohibited imitation targets.

Recommended direction:

- restrained dark-fantasy environments;
- luminous crystal accents used as focal contrast, not everywhere;
- readable silhouettes and limited palette noise;
- strong class identity;
- compact, deliberate animation;
- interface framing that supports text rather than competing with it.

Vertical-slice content:

- title screen;
- one town area and one service;
- one compact Ruined Keep room;
- one normal battle;
- one boss battle;
- one result screen;
- representative music and SFX through the new catalog.

Acceptance:

- human approves the art bible and slice;
- assets are readable at native resolution and supported scaling modes;
- the slice proves the production effort is affordable;
- no copyrighted or near-replica assets/layouts/music are used;
- full asset production does not begin before this gate.

### M16 — Compact dungeon-room system

**Goal:** replace fixed full-screen room realization with compact, validated,
data-driven room layouts while preserving dungeon topology and run rules.

Required model separation:

- `Dungeon`/room graph: connectivity, mandatory path, gates, reward ownership,
  boss progression;
- room archetype: semantic purpose and allowed layout parameters;
- room layout: dimensions, local tiles, door anchors, spawn points, markers,
  collision, prop zones, draw origin;
- presentation theme: tiles, props, ambience, colors, and effects.

Initial archetypes:

- Entry;
- Connector/Corridor;
- Crossroads;
- Gate Chamber;
- Encounter Chamber;
- Treasure Alcove;
- Shrine/Event Chamber;
- Elite Chamber;
- Boss Antechamber;
- Boss Arena.

Room validation must prove:

- every connected door has a valid anchor;
- the player can reach every required door from the spawn;
- required encounter markers do not block the only path incorrectly;
- chests and interactables are reachable;
- the spawn is valid and not inside collision;
- boss layouts remain navigable;
- local bounds and draw origin stay inside the exploration viewport;
- paired graph doors remain consistent.

Use flood fill/BFS for connectivity. Use derived room-local seeds to preserve
separation from topology RNG.

Score/reproducibility decision:

- determine whether a generator version must be stored with score entries;
- keep existing score/save loading backward compatible if a field is added;
- obtain human approval before changing public JSON/save/score schema.

Acceptance:

- standard rooms no longer cover the full exploration screen;
- at least five archetypes appear across representative runs;
- thousands of generated rooms pass connectivity/reference checks;
- mandatory gate and boss reachability rules remain true;
- same seed + generation version produces the same topology and room layouts;
- navigation and interactables are immediately legible in human testing.

### M17 — Exploration visuals and animation

**Goal:** replace abstract town/dungeon presentation with readable characters,
enemies, tiles, props, effects, and atmosphere using the approved art system.

Required systems:

- sprite animation data with frame rectangles, timing, loop mode, and anchor;
- visual bounds separated from collision bounds;
- deterministic draw layers;
- top-down actor facing/walk/idle states;
- visible enemy silhouettes and danger/elite/boss differentiation;
- theme-specific tiles and props;
- interaction highlights that do not rely only on color;
- restrained particles/glows/overlays suitable for raylib and the target
  hardware.

Theme identity must come from composition and shape, not palette swaps alone:

- Ruined Keep: masonry, defensive geometry, banners, statues, broken furniture;
- Crystal Mine: supports, rails, equipment, fractured rock, luminous clusters;
- Hollow Forest: roots, clearings, fallen trunks, shrines, organic boundaries.

Acceptance:

- player/enemy/interactable silhouettes remain readable at native resolution;
- collision is unaffected by texture dimensions;
- draw ordering has no obvious depth errors;
- themes remain distinguishable in grayscale composition tests;
- decorative density does not obscure exits, hazards, enemies, or chests;
- missing animation definitions fall back to a static valid frame.

### M18 — Battle presentation and game feel

**Goal:** make turn-based combat fast, clear, and satisfying without delaying the
fewest-turn scoring loop.

Required information hierarchy:

- active combatant;
- legal commands and unavailable reasons;
- selected target and valid target set;
- HP/MP and status effects;
- skill/item description and cost;
- current round/turn score context;
- action message and result;
- boss telegraph and escalation state.

Action presentation should use a short sequence:

1. anticipation;
2. motion/pose or projectile;
3. impact;
4. number/status feedback;
5. brief recovery;
6. next action.

Add restrained, configurable:

- hit stop;
- sprite flash;
- screen shake;
- impact particles;
- elemental/heal/status effects;
- KO/victory feedback;
- boss phase/enrage feedback.

Support Normal, Fast, and near-instant battle presentation where safe. Effects
must not change simulation timing or results.

Acceptance:

- source, target, effect, and result are always understandable;
- normal actions resolve briskly;
- Fast materially reduces downtime;
- reduced-flash and reduced-shake settings work;
- status effects and telegraphs are visible without opening a separate manual;
- combat remains fully understandable with audio muted;
- no animation changes deterministic battle outcomes.

### M19 — Progression, economy, and score-integrity hardening

**Goal:** ensure the newly implemented XP/leveling and town economy support
replay without making score comparisons meaningless.

This milestone is an audit and tuning milestone, not a reimplementation of XP.

Required analysis:

- XP curve and level-cap pacing;
- battle XP versus paid Training Hall progression;
- gold inflow and sinks by depth;
- healing and failure consequences;
- consumable/equipment purchase dominance;
- class power growth and equipment scaling;
- low-level versus high-level score comparability;
- incentives to farm weak encounters before scoring runs.

A scoring policy must be explicit. Recommended default:

- segment or tag scoreboards by meaningful run conditions such as seed, depth,
  theme, generator version, party level/power band, and difficulty modifiers;
- do not hide a complex power-normalization formula from the player unless
  simulation proves it is robust and the UI can explain it.

Acceptance:

- no obvious infinite or dominant gold/XP loop;
- Training Hall and battle XP have distinct, defensible roles;
- every class retains a meaningful tactical identity;
- score comparison conditions are visible and honest;
- progression and economy values remain data-driven;
- simulations and human runs identify no trivial progression exploit.

### M20 — Encounter and dungeon-content variety

**Goal:** deepen replay through meaningful tactical combinations rather than
unbounded content count.

Define encounter roles and composition constraints:

- bruiser;
- glass cannon;
- healer;
- buffer/debuffer;
- protector;
- poison/attrition;
- speed disruption;
- anti-magic/anti-physical;
- elite specialist.

Externalize composition rules such as depth/theme pools, incompatible roles,
maximum support count, elite frequency, team size, and boss-minion rules.

Add a deliberately small room-event set only after M16 is stable:

- shrine trade-off;
- limited healing source;
- trapped treasure;
- merchant or exchange;
- optional elite challenge;
- score-risk modifier.

Every event must create a decision with visible consequences. Decorative text
alone is not enough.

Boss acceptance:

- recognizable silhouette and audio identity;
- unique mechanic and telegraph;
- counterplay;
- escalation/phase;
- reward identity;
- not merely inflated HP.

Acceptance:

- generated teams cannot violate authored composition constraints;
- representative runs contain distinct tactical questions;
- events expose risk/reward before confirmation;
- boss mechanics are distinguishable in blind playtest notes;
- content quantity is added only after role coverage gaps are identified.

### M21 — Final music, ambience, and sound effects

**Goal:** replace synthesized placeholders with coherent, licensed/original,
manifest-driven audio.

Minimum music states:

- title;
- town;
- preparation/guild;
- dungeon theme or approved shared/variant model;
- normal battle;
- boss battle;
- victory;
- defeat;
- result.

Minimum ambience:

- town;
- Ruined Keep;
- Crystal Mine;
- Hollow Forest.

Mixing requirements:

- streamed music through raylib music streams;
- click-free loops and transitions;
- separate master/music/SFX controls;
- SFX variation for repetitive actions where useful;
- no clipping at maximum configured volume;
- rate-limited or softened rapid menu navigation;
- safe silence fallback;
- game remains understandable without sound.

Acceptance:

- all major scenes have intentional audio;
- music does not stack across state transitions;
- loops and transitions are clean;
- settings persist and apply consistently;
- every shipped asset has provenance and license/attribution records;
- swapping a track or effect requires no C++ edit.

### M22 — Onboarding and accessibility

**Goal:** teach the game through play and remove avoidable barriers without
removing tactical challenge.

Onboarding sequence should teach progressively:

1. town movement and interaction;
2. party preparation and equipment;
3. dungeon danger and gates;
4. battle command/target selection;
5. turn-efficiency scoring;
6. retreat/failure consequences;
7. XP, shops, and deeper runs.

Requirements:

- interactive or in-context tutorial actions, not only a static controls page;
- tutorial prompts can be disabled and revisited;
- contextual Details for stats, statuses, danger, score components, and
  equipment comparisons;
- high-contrast UI option;
- no essential color-only or sound-only information;
- reduced motion/flash/shake controls;
- configurable text/message pacing;
- destructive-action confirmation and clear error recovery;
- settings reachable before starting a game.

Acceptance:

- a new tester completes a depth-1 run without external instruction;
- every critical mechanic is explained in-game;
- keyboard-only and gamepad-only operation remain complete;
- critical text meets the documented contrast target;
- tutorial state persists defensively and can be reset;
- accessibility options are covered by the manual matrix.

### M23 — Automated visual validation, playtesting, and balance hardening

**Goal:** prevent presentation regressions, make long-content/edge states
repeatable, and harden usability and balance with observed external
playtesting — so that M24 can be pure packaging.

Recommended tools:

- deterministic debug/capture scenarios for representative screens;
- screenshot capture at native virtual resolution;
- UI bounds and overlap diagnostics;
- content linting for missing asset IDs, undefined action tokens, invalid
  animations, and text overflow policies;
- large-scale room generation/connectivity tests;
- expanded deterministic battle simulations and machine-readable reports.

Representative capture states:

- title/help/settings;
- party creation with maximum names;
- full save metadata and empty slots;
- each town service with long item/class descriptions;
- guild with maximum seed/depth strings;
- each dungeon theme and archetype;
- battle with maximum statuses, long skill descriptions, and five enemies;
- victory/defeat/result/scoreboard extremes.

Do not introduce fragile pixel-perfect image comparisons as the only validation.
Geometry assertions and reviewed screenshots are more maintainable initially.
If image diffs are later added, define tolerance and intentional-update policy.

Playtest with at least:

- a player unfamiliar with the project;
- an experienced JRPG player;
- a roguelite/score-chasing player;
- a controller-first player;
- a keyboard-first player;
- a smaller-display or low-vision test setup where available.

Observe without coaching:

- time to begin a run;
- wrong-button and navigation errors;
- missed interactables;
- unread/clipped text;
- room-navigation hesitation;
- battle decision clarity;
- score comprehension;
- run duration;
- dominant strategies;
- whether players voluntarily start another run.

Then fix repeated control/readability/navigation/score-comprehension defects
and make final balance and pacing changes backed by that evidence.

Acceptance:

- representative captures are reproducible;
- unintended UI overflow is a failing validation result;
- missing required assets fail clearly;
- thousands of generated rooms remain valid;
- balance reports identify outlier encounters and progression bands;
- all listed tester profiles were observed; repeated usability defects are
  resolved, not documented away;
- new players complete the core loop unaided; score and danger are explainable
  by players;
- no dominant strategy trivializes representative runs;
- capture/debug tooling is excluded or safely disabled in release builds.

### M24 — Release packaging and final release validation

**Goal:** produce and validate a reproducible, clean, polished Windows release
candidate. Playtesting and balance are complete (M23); only release-blocking
fixes are in scope.

Release work:

- reproducible Release preset/build;
- packaged `data/` and `assets/` trees;
- executable/version metadata and icon;
- stable save/settings paths;
- licenses and credits;
- graceful audio/controller failure;
- clean logs and known limitations;
- smoke test on a machine without the development environment;
- final screenshot and manual test matrix sign-off on the packaged build.

Acceptance:

- packaged build runs without Visual Studio installed;
- saves and settings persist correctly in the packaged build;
- no debug overlays, capture tooling, placeholder paths, or unlicensed assets
  ship;
- final manual test matrix is signed off on the packaged build;
- known limitations are documented honestly;
- human explicitly approves the polished release candidate.

## 8. Recommended implementation order

The order is intentional:

1. M11 audit what is actually broken.
2. M12 make information safe before decorating it.
3. M13 make interaction consistent before adding more screens.
4. M14 make assets replaceable before producing them.
5. M15 prove one final-quality slice before committing to volume.
6. M16 fix room scale and architecture before environment production.
7. M17 produce exploration presentation on the stable room system.
8. M18 polish combat after UI and asset contracts exist.
9. M19 protect progression and score integrity.
10. M20 add targeted variety.
11. M21 finalize audio through the proven asset pipeline.
12. M22 harden onboarding and accessibility across the full game.
13. M23 automate regression detection, playtest with real players, and harden
    balance from that evidence.
14. M24 package and validate the polished release candidate.

Starting with final art or music would be the wrong order. It would bind expensive
creative work to unstable layouts and hard-coded presentation contracts.

## 9. Definition of done for every player-facing screen

A screen is not complete unless:

- all required text fits or has an explicit continuation mechanism;
- long, empty, and maximum-data cases are tested;
- focus is visible and never unintentionally obscured;
- Confirm, Cancel, Details, and paging semantics are consistent;
- keyboard and gamepad both work;
- control hints match active bindings;
- color is not the sole state indicator;
- critical text and focus meet the documented contrast targets;
- no presentation file path is hard-coded in the state;
- missing optional assets degrade safely;
- state entry suppresses unintended buffered input;
- the screen works at every supported window shape/scale;
- screenshots are reviewed for visual work;
- relevant tests and manual checks pass.

## 10. Major failure modes to reject

### One massive polish branch

A large cross-cutting rewrite will be difficult to review, bisect, validate, and
revert. Use vertical slices and screen-family migrations.

### Final art before stable contracts

Final assets created before M12–M15 will be redone. Build the layout, catalog,
and vertical slice first.

### General-purpose framework building

Do not build a full UI toolkit, ECS, editor, scripting runtime, or dependency
injection system to solve a focused game problem.

### Cosmetic-only fixes

New colors and borders do not solve clipped text, hard-coded prompts, empty
rooms, unclear focus, or weak feedback.

### Procedural sameness disguised as variety

Random dimensions and random props are not room design. Archetypes, focal
composition, readable navigation, and gameplay purpose are required.

### Animation that punishes score play

Long unskippable effects undermine a game built around efficient repeated
battles. Feedback must be impactful and fast.

### Progression invalidating score comparison

Permanent power changes the meaning of fewest turns. Scoreboards must expose
comparable conditions rather than presenting unlike runs as equivalent.

### Color-only or audio-only information

Danger, status, selection, rarity, and telegraphs require a second information
channel.

### Tests used as proof of visual quality or fun

Headless tests prove logic. Human visual review and playtesting prove
presentation and player experience.

## 11. Reference guidance

These sources inform quality targets and implementation priorities. They are
engineering references, not a claim that the game is formally certified against
web or platform standards.

- W3C, WCAG 2.2 Understanding and Quick Reference:
  - https://www.w3.org/WAI/WCAG22/Understanding/
  - https://www.w3.org/WAI/WCAG22/quickref/
- Microsoft, Xbox Accessibility Guideline 101 — Text display:
  - https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/101
- Microsoft, Xbox Accessibility Guideline 107 — Input:
  - https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/107
- Microsoft, Xbox Accessibility Guideline 112 — UI navigation:
  - https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/112
- Microsoft, Xbox Accessibility Guideline 114 — UI context:
  - https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/114
- Microsoft, Xbox Accessibility Guideline 115 — Error messages and destructive
  actions:
  - https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/115
- Game Accessibility Guidelines — Basic:
  - https://gameaccessibilityguidelines.com/basic/
- raylib 6.0 examples and API reference:
  - https://www.raylib.com/examples.html
  - https://www.raylib.com/cheatsheet/raylib_cheatsheet_v6.0.pdf
