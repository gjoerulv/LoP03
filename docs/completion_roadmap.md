# Crystal Dungeons — Completion and Presentation Roadmap

> Strategic frame for the post-M10 completion program (M11–M24) and for
> planning further work beyond it.
>
> This document defines **direction and quality bars only**. It does not
> authorize implementation, and it is not a status record: `docs/milestones.md`
> is the authoritative ledger, and the active `docs/milestone_notes/MXX_*.md`
> controls current scope. Every milestone from M11 on has exactly one note
> there, indexed from the ledger.
>
> Deliberately carries no commit pointers or status claims — those went stale
> here twice. Read the ledger for state; read this for the bars that state has
> to clear.

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

The per-milestone plans that once lived here have been delivered and are
recorded where the work actually happened. This section is deliberately a
pointer, not a copy — three parallel descriptions of the same milestones drift
apart, and the plan is the least reliable of the three once code exists.

| For | Read |
|-----|------|
| Current status of every milestone | `docs/milestones.md` (the ledger — authoritative) |
| What a milestone actually delivered | `docs/milestones.md`, per-milestone section |
| Full scope, decisions, and deviations | `docs/milestone_notes/MXX_*.md` (one note per milestone) |
| Why the program is ordered this way | §8 below |
| Quality bars any new milestone must meet | §4, §5, §9, and §10 of this document |

The program covers M11–M24. Sections §1–§6 and §8–§11 remain the strategic
frame for planning further work: they define the mission, the non-negotiable
product requirements, the engineering guardrails, the per-screen definition of
done, and the failure modes to reject. None of that is milestone-specific, and
all of it still applies to milestones not yet planned.

This document never authorizes implementation. `docs/milestones.md` and the
active milestone note control current scope.

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

## 12. Polish program (M25–M30) — direction

Added 2026-07-20, after an owner review found the game not release-ready and
deferred M23/M24 behind this work. Direction and quality bars only; scope lives
in `docs/milestones.md` and the per-milestone notes.

### Why this phase exists

The completion program (M11–M24) delivered systems, safety, and replaceability.
What it did not deliver is *identity*: enemies share three sprites across 27
definitions, town services are flat colour fields, and every enemy in the game
makes the same decision every turn. The result reads as a well-built prototype
rather than a finished game. Two defect classes dominate:

- **Visual sameness.** Presentation contracts are correct and the fallbacks work
  exactly as designed — but a fallback used everywhere becomes the art
  direction. Per-enemy art was always a manifest drop-in; nobody dropped it in.
- **Tactical staleness.** Deterministic lowest-HP targeting is readable and
  testable, and it made M5–M22 possible. It also means the optimal play is to
  keep one character wounded, which inverts party roles: an efficiently played
  mage becomes the tank.

### Quality bars for this phase

Additional to §4, §5, §9 and §10, which all still apply:

1. **Distinctness is a requirement, not a polish item.** Every enemy, boss,
   service screen, and dungeon theme must be identifiable without reading its
   name. Tier-generic fallbacks remain in the code as a safety net, but shipping
   content relying on one is a defect the lint suite must catch.
2. **Unpredictable must not mean random.** Enemy behaviour should be hard for a
   player to exploit and impossible for the simulator to disagree about. Seeded
   variance yes; nondeterminism never. The simulator and live play must continue
   to produce identical outcomes from identical inputs.
3. **Player agency over threat.** Removing exploitable targeting is only half
   the change. If the player cannot deliberately influence who gets attacked,
   the result is frustration rather than tactics — the control skills are part
   of the AI change, not a follow-up.
4. **Score history stays honest.** M19's principle holds: runs are never
   silently normalised. A battle-rules change is as comparability-breaking as a
   generation change and gets its own version tag, because a player comparing
   two runs is entitled to know which rules produced each.
5. **Progression should have texture.** Content volume alone does not fix
   staleness. Skills arriving over levels give a run shape that a larger flat
   roster does not.

### Ordering rationale

Presentation corrections (M25) first: they are risk-free, independently
valuable, and M28's enmity model is unreadable without the target-info UI that
M25 introduces. Art identity (M26) before content expansion (M29), so new
enemies inherit an established language rather than setting one. The combat
rules change (M28) before content built for it (M29), so behaviour profiles
exist before enemies are designed around them. Economy (M30) last, because gold
income depends on battle outcomes and content — tuning a rest cost before those
settle is tuning against a moving target.

M23 and M24 run last — after whatever expansion program is newest (the
ledger's execution-order note is authoritative) — rather than being
cancelled. Their tooling and
packaging work is built and retained; only the sequencing changed. Validating
and packaging a build known to need this work would have measured the wrong
build, and the capture scenes and balance battery both need extending for the
new AI, content, and art before that validation is worth trusting.

## 13. Expansion program (M31–M34) — direction

Added 2026-07-20, after the M25–M30 polish program closed. Direction and quality
bars only; scope lives in `docs/milestones.md` and the per-milestone notes.

### Why this phase exists

M25–M30 gave the game identity and a real economy, but the loop is still short:
one town, one difficulty band, and no reason to push past a comfortable depth.
This program adds **connected systems that reward climbing** — a seven-town
ladder that raises both difficulty and score, a stakes penalty that erodes the
score of a run that does not escalate, and a rare black market that turns
optional risk into legendary gear — plus a small usability fix (shop categories)
that the growing item list needs. It is content and systems, not a new genre:
the town → prepare → seeded dungeon → boss → score → upgrade loop is unchanged;
towns are new hubs onto the same loop, not a world map or a story campaign.

### Quality bars for this phase

Additional to §4, §5, §9 and §10, which all still apply:

1. **Climbing is the only intended power fantasy.** Higher towns pay in **score**,
   not in a better farm. XP/gold rewards do not scale with town unless the owner
   explicitly decides otherwise; a top town must never become the optimal place
   to grind, or the stakes system it anchors is meaningless.
2. **Escalation must read before it bites.** The stakes penalty is forewarned in
   the Guild for the run being configured and shown, itemized, on the result and
   scoreboard. A penalty the player discovers only after the run is punitive, not
   tactical — M19's "visible and honest" bar is the acceptance line.
3. **Save-scum resistance where a reward is at stake.** The black-market roll and
   the stakes baseline are derived from committed run state (dungeon seed,
   autosaved penalty), so reloading cannot reroll a bad outcome or shed a
   penalty. Randomness the player can farm by reloading is a defect, not content.
4. **Determinism and comparability are non-negotiable.** Town 1 generation stays
   byte-identical (`generationVersion` unchanged); battle rules stay untouched
   (`battleRulesVersion` unchanged) — town scaling changes combat *inputs*,
   visibly tagged, not the *rules*. New score-affecting conditions (town bonus,
   stakes penalty) are tagged and displayed, never normalized. Simulator and live
   play still agree exactly.
5. **Distinctness carries up the ladder.** Each town must be identifiable by
   exterior art and music without reading a label, and its music grows more
   sinister as the chain deepens — the M25–M30 distinctness bar, extended to
   towns. The presentation lint proves every town resolves its art and audio.

### Ordering rationale

Shop categories (M31) first: risk-free, independently valuable, and it touches no
system the later milestones depend on. The town ladder (M32) is the structural
spine — `currentTown`, travel, and town-scaled difficulty/score — and both later
milestones read it, so it precedes them. The stakes penalty (M33) needs the
`(town, depth)` stakes vector M32 introduces. The black market (M34) is last
because its spawn trigger *is* M33's stakes-raise event and its legendaries must
be balanced against M32's town-7 difficulty. M23 → M24 run last (see the
ledger's execution-order note), re-audited for all the new content.

## 14. Endgame program (M35–M42) — direction

Added 2026-07-21, after the M31–M34 expansion program closed. Direction and
quality bars only; scope lives in `docs/milestones.md` and the per-milestone
notes.

### Why this phase exists

M31–M34 built a seven-town ladder that rewards climbing, but the ladder has no
summit and combat still turns on the M7 status set (poison + attack/defense
buffs/debuffs). The game reaches town 7 and stops. This program adds an
**endgame**: deeper combat (Confusion/Silence/Blind statuses, then passive
skills), per-town equipment and enemy content so the ladder has fresh gear and
foes at every rung, seeded boss drops that reward deep high-town clears, and — the
payoff — a **castle above the town-7 ceiling** where a King guards three
challenges with their own records. A light-hearted serial story and three
enrichment features (bestiary, victory stats, achievements) round it out. It is
content and systems on the same loop: town → prepare → seeded dungeon → boss →
score → upgrade is unchanged, and the castle is a distinct summit hub reached by
the same road rule, not a world map, a story campaign, or a new genre.

### Quality bars for this phase

Additional to §4, §5, §9 and §10, which all still apply:

1. **Determinism carries into every new source of chance.** This program adds the
   game's first to-hit roll, confusion targeting, boss drops, and endless castle
   waves — every one derived from an existing **seeded** stream (`Battle.rngSeed`,
   the run seed, a challenge seed), never wall-clock or unseeded RNG. The
   simulator and live play must continue to agree exactly, as they do today via
   shared `tickStatuses`. Randomness a player can farm by reloading is a defect.
2. **The castle stays out of the ladder and the scoreboard.** `WorldLadder`
   assumes towns 1..7 and the dungeon scoreboard encodes the M19 comparability
   policy; the castle is a **distinct place** (`kCastleTown = 8`, its own state)
   with its **own records**, never fed through town scaling and never writing a
   `ScoreEntry`. Blurring the two would corrupt both the ladder clamps and score
   comparability.
3. **Rules changes are versioned and visible.** M35 and M36 each change how a
   battle resolves and bump `battle::kBattleRulesVersion`; M37/M38 change what a
   seed generates and bump `dungeon::kGenerationVersion`. Tags are displayed for
   comparability, never used for ranking — M19's principle, extended to the new
   systems.
4. **Climbing stays the power fantasy.** Per-town gear and boss drops pay in
   power for pushing deeper and higher, but the stakes rule (now re-tuned to
   −30 %/step, −99 % cap) still fights farming a safe stake; drops are gated to
   town ≥ 3 / depth ≥ 4 and sim-validated not to trivialize the black market or
   the top town. The castle pays in records and one-time first-clear rewards, not
   a better farm.
5. **Originality and distinctness carry up the ladder and into the castle.** Every
   new enemy, boss, NPC, and castle asset must be identifiable without reading its
   name (the M25–M30 distinctness bar), and all writing/art/music is original (the
   hard constraint). The presentation lint proves every new sprite and castle asset
   resolves.

### Ordering rationale

Statuses (M35) first: they are the foundation every later combat system builds on,
and landing them (plus the self-contained stakes re-tune) before passives means
the to-hit layer exists when Evasion needs it. Passives (M36) next, while combat
rules are still in flux, so the last `battleRulesVersion` bump of the program is
spent here rather than later. Per-town equipment (M37) before per-town enemies
(M38) so new foes are balanced against the gear the player can actually reach, and
both before boss drops (M39) so the drop pool (legendary weapons from M37, the
full boss roster from M38) exists. The castle (M40) needs that full boss roster
for its gauntlet and sits at the top of everything below it. Story (M41) hangs on
the finished town-and-castle geography, and the enrichment features (M42) —
bestiary, victory stats, achievements — reference the complete enemy roster,
combat systems, and story voice, so they come last. M23 → M24 now run after
the King's Gambit program (§15), re-audited for everything.

## 15. King's Gambit program (M43–M45) — direction

Added 2026-07-22, after the owner approved M42, a deep audit of M35–M42 found
four fixable defects and no blockers, and the owner specified nine features
forming one arc: the King doubles in power, and the game ships his counterplay
and his reward. Scope lives in `docs/milestones.md`; decisions were taken at
planning (recorded there).

### Why this phase exists

The Hollow King is the game's summit, but he fell on the first serious
attempt, several economy numbers no longer sting, and the audit showed two
places where a stated rule and the code disagree (confusion vs. skill-casters;
the castle defeat message). This program makes the summit worth the climb:
honest rules, prices that matter, a King who demands preparation, rare relics
that reward exploration with exactly that preparation, and three
deliberately-absurd unlockable classes as the trophy.

### Quality bars for this phase

All prior bars (§4, §5, §9, §10, §12, §14) still apply. Additionally:

1. **Rules must match their descriptions.** If the help text says confusion
   makes a unit attack its own side, it does — for every unit, in the sim and
   on screen. A discrepancy between stated and actual rules is a defect even
   when deterministic and consistent.
2. **Counterplay ships with the threat.** The doubled King lands in the same
   milestone as the relics and after the snacks, and the clearability
   evidence may never rely on the rarest drop.
3. **Joke content is still content.** The Dragon/Jester/Goose classes and the
   relic items must be funny AND mechanically sound: schema-driven,
   deterministic, score-tagged, and covered by the battery like everything
   else. Forced/automatic actions live in shared battle code; presentation
   humor (jest lines) never touches the RNG stream.

### Ordering rationale

Fixes and repricing first (M43): they change the baselines every later
balance judgment depends on, and the confusion fix is a rules bump that the
relic effects should land on top of, not underneath. The relics and the
doubled King together (M44): shipping the threat without its counterplay
would make the summit a wall, and shipping relics first would trivialize the
current King. Classes last (M45): they are the reward for the fight M44
defines, and their score modifiers should be tuned against the final King
economy. The owner-directed M46 presentation facelift followed.

## 16. Court & Comfort program (M47–M51) — direction

Added 2026-07-23, after M43–M46 merged and the owner specified eleven
features. Scope and owner decisions live in `docs/milestones.md` (the
program section); this section records only why and in what order.

### Why this phase exists

The King fell too gently (a lost challenge healed the party for free), his
throne room is empty of court, the Boss Rush is a line of lonely bosses, and
several comfort seams remain: town exits hidden by the M46 footer, no way to
quit from play, a flat settings list, and a damage model with no elemental
texture. This program raises the endgame's stakes and manners at once —
harsher defeats with a guaranteed way home, a guarded King on a deterministic
revive clock, sparse elemental affinities that reward preparation without a
matchup spreadsheet, a town that is walked rather than operated, and options
(CRT, background audio, categorized settings) that respect the player.

### Quality bars for this phase

All prior bars still apply. Additionally:

1. **Harshness never soft-locks.** Every defeat outcome leaves a playable
   state (the full-wipe survivor rule exists for exactly this); clearability
   of the guarded King and the minion-bearing Boss Rush is re-proven with
   sim evidence under the new defeat stakes before shipping.
2. **Sparse means sparse.** Elements are curated seasoning (a handful of
   tags, shown honestly once encountered), never a coverage matrix; Dark
   stays reserved.
3. **Comfort changes keep determinism and compatibility.** New battle rules
   live in shared code (rules bumps 7/8/9); settings/schema additions are
   optional fields with defaults; the CRT shader isolates at the blit and
   fails soft; capture stays byte-identical.

### Ordering rationale

Rules and flow first (M47): the defeat stakes and Purify scope change the
baselines the King/rush balance work must be judged against, and the quit/
pause comforts are independent quick wins. Elements next (M48): the guards'
kits and any boss affinities build on them. The King's Court third (M49):
it is balanced against the M47 stakes with M48 tools. The town rework (M50)
and the presentation/options pass (M51) are self-contained and land last so
the balance-critical work is never blocked on UI. M23 → M24 run after M51,
unless a later expansion is authorized (M52 was).

## 17. Comforts & secrets (M52) — direction

Authorized 2026-07-23 as one quality-of-life milestone before M23/M24, and the
last authorized expansion. Scope and owner decisions live in
`docs/milestones.md` and `docs/milestone_notes/M52_comforts_secrets.md`; this
section records only why and in what order.

### Why this phase exists

M47–M51 closed the endgame's stakes and manners. Six small comforts and one
secret remained worth doing before the game is validated and packaged: ambience
was chained to the SFX slider and always full-blast; a battle offered no way to
review what just happened; the equip shop hid owned counts and stat diffs; the
bestiary stopped at base stats; the Dragon Crown had no special interaction
with the King it names; and the black market never rewarded the deepest,
highest-stakes runs. None of these touches the core loop.

### Quality bars for this phase

All prior bars still apply. Additionally:

1. **Secrets stay determined and shared.** The Crown's hidden effect lives in
   shared `battle::` code and is schema-driven (an optional field, no id
   branched on), so simulation and live play agree by construction and design
   docs — not the game — are where the secret is written down.
2. **Comforts stay additive.** Ambience and the high-stakes market add a field
   and a fresh-salt roll with no settings/save/generation version bump; the
   battle log is presentation-only and never reads or writes the battle model.
3. **The densest panels stay overflow-clean.** New readouts (the equip diff,
   the bestiary max stats) are fitted and verified against the worst-case
   capture scenes (the King's bestiary entry, the town-7 buy list).

### Ordering rationale

One milestone, six independent slices; the only cross-cutting change is the
Crown's rules bump (9 → 10), so it and its schema field are implemented and
tested as a unit. M23 → M24 run after M52, re-audited against the post-M52
checkout.
