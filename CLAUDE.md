# CLAUDE.md

## Project Identity

This project is a feature-complete 16-bit-inspired turn-based JRPG roguelite built in C++20 with raylib. The post-M10 program focuses on turning the playable prototype into a polished, readable, configurable release without replacing its proven core systems.

Working title: `Crystal Dungeons`.

The game is inspired by the clarity, readable UI, side-view battles, party identity, and atmosphere of 16-bit JRPGs. It must not copy Final Fantasy or any other copyrighted game. Do not use copyrighted names, characters, music, sprites, fonts, maps, monsters, spells, UI replicas, story elements, or asset layouts.

Core loop:

Town hub → prepare party → enter generated dungeon → fight visible enemy teams guarding gates/chests → defeat boss → score based mainly on fewest battle turns → return to town → upgrade/unlock → repeat.

This is not a broad story JRPG. It is a focused dungeon-score JRPG roguelite.

## Current Development Mode

Work milestone by milestone.

Do not continue to the next milestone until the human explicitly approves.

At the start of every session:

1. Read this file.
2. Read `docs/game_design.md`, `docs/technical_design.md`, `docs/milestones.md`, `docs/completion_roadmap.md`, and the current milestone note if one exists.
3. Inspect the repository before proposing changes.
4. Identify the current milestone and whether it is complete.
5. Verify build/test commands before claiming work is complete.

If these docs do not exist yet, create them during the appropriate early milestone and keep them updated.

### Document Authority and Scope

Documentation authority order, highest first:

1. `CLAUDE.md` — this repository operating contract.
2. The approved current milestone note under `docs/milestone_notes/` — current implementation scope.
3. `docs/milestones.md` — milestone ledger and status.
4. `docs/game_design.md` — authoritative game behavior.
5. `docs/technical_design.md` — authoritative architecture.
6. `docs/completion_roadmap.md` — long-term strategic direction.
7. Supporting authoring, validation, control, asset, and test documents.

The completion roadmap defines strategic direction and quality targets. It does not authorize work from later milestones. Do not combine UI, input, asset, room-generation, battle-presentation, content, audio, accessibility, and release work into one unapproved pass.

Where documents conflict:

* Do not silently choose the most convenient interpretation.
* Identify the conflict explicitly.
* Use the authority order above where it resolves the issue.
* Escalate material product or architecture conflicts to the human.
* Update the stale document after the decision is resolved.

If a milestone requires a product, architecture, dependency, save-format, public-schema, or expensive-to-reverse decision, stop and ask the human.

### Milestone Status Vocabulary

`docs/milestones.md` and every milestone note use exactly these statuses:

* `planned`
* `in progress`
* `implemented, awaiting manual approval`
* `complete (approved)`
* `blocked`

Claude must never mark a milestone `complete (approved)` based only on builds, tests, screenshots, simulations, or its own review. Only the project owner can approve a milestone, after manual testing. When Claude finishes implementing a milestone, it sets `implemented, awaiting manual approval` and stops. After the owner explicitly approves, Claude updates the status to `complete (approved)`.

## Human Collaboration Rules

Ask the human before making:

* Product-direction decisions
* Game-design decisions that affect player experience
* Architecture changes
* Dependency changes
* Save-format changes
* Public data-schema changes
* Milestone-scope changes
* Anything that would make future reversal expensive

For minor implementation details:

* Make conservative choices.
* Keep them reversible.
* Document assumptions.
* Do not block progress unnecessarily.

When asking questions:

* Ask only questions that materially affect the project.
* Always include a recommended answer.
* Explain the consequence of each option briefly.
* If the human says `I agree 100%`, treat that as approval of all recommended answers.

If you need to verify something but cannot:

* Say exactly what could not be verified.
* Say what command/tool/access is needed.
* Provide the exact command the human should run.
* Do not claim verification you did not perform.

Human validation is required for:

* Visual feel and readability
* Controller/keyboard feel
* Fun factor
* Difficulty/balance feel
* Audio/music acceptability
* Whether a milestone satisfies the intended player experience
* Any test that requires actually playing the game if you cannot run or view it

## Autonomous Operating Model

These rules govern how Claude Code sessions plan, implement, validate, document, and close milestones.

### Repository Inspection

Before planning or editing a milestone:

* Inspect current HEAD and the working tree.
* Read the active milestone documents.
* Inspect the relevant implementation and tests.
* Verify that documented assumptions still hold against the actual code.

Every milestone note must be re-audited and refined against the then-current repository before implementation of that milestone begins. Notes describe the repository as it was when last reviewed; do not implement from a stale note.

### Self-Directed Implementation

Once the owner has approved a milestone plan, Claude may execute the approved milestone slices without asking permission for routine implementation details.

Claude should make reasonable local engineering decisions autonomously where they:

* remain inside approved scope;
* do not change player-facing rules;
* do not introduce a new dependency;
* do not alter a public schema unexpectedly;
* do not break save compatibility;
* do not invalidate deterministic behavior;
* do not create a major architectural commitment.

Do not escalate trivial implementation choices.

### Mandatory Escalation

Claude must stop and ask the owner before:

* changing the approved player experience;
* changing combat, scoring, progression, or dungeon rules outside approved scope;
* adding or replacing major dependencies;
* performing a major architectural rewrite;
* changing public JSON schemas beyond the approved plan;
* changing save compatibility policy;
* changing deterministic seed behavior;
* changing the virtual resolution or core rendering assumptions;
* adopting a final art direction not already approved;
* deleting or replacing substantial historical documentation;
* expanding into a later milestone.

### Testing Responsibility

Claude must run all practical automated tests relevant to its changes.

Claude must not claim that automated tests prove:

* visual quality;
* control intuitiveness;
* game balance;
* enjoyment;
* accessibility in practice;
* release readiness.

Those require owner review or external playtesting.

### Documentation Responsibility

Documentation is part of the implementation, not optional cleanup.

Before declaring a milestone implementation finished, Claude must:

1. Update the active milestone note with the actual implementation.
2. Record deviations from the original plan.
3. Update architecture and design documents affected by the change.
4. Update content, control, asset, save, or validation documentation where relevant.
5. Update `docs/milestones.md`.
6. Set the milestone to `implemented, awaiting manual approval`.
7. Provide the owner with a manual test checklist.
8. Report automated build and test results.

Claude must not leave documentation knowingly describing superseded behavior.

### Approval Responsibility

Only the owner may approve a milestone. After receiving explicit approval, Claude should:

1. Update the milestone to `complete (approved)`.
2. Record the approval date where the ledger records dates.
3. Update the completion summary.
4. Confirm whether the next milestone note needs refinement.
5. Stop, unless the owner also authorizes starting the next milestone.

Approval of one milestone is not automatic authorization to implement the next milestone.

### Git Responsibility

Claude must not:

* commit;
* push;
* amend commits;
* rebase;
* merge;
* create tags;
* force-update branches.

Claude may inspect Git status, diffs, history, and the current HEAD. The project owner handles all Git commits and pushes.

### Working-Tree Discipline

Claude must:

* preserve unrelated owner changes;
* avoid destructive resets;
* avoid broad formatting churn;
* avoid editing unrelated files;
* report pre-existing modifications before changing overlapping files;
* keep changes reviewable.

## Skill Requirement

Before implementation begins, build a project skill if it does not already exist.

Recommended path:

`.claude/skills/crystal-dungeons/SKILL.md`

The skill should capture:

* The game vision
* Milestone workflow
* Gotchas
* Verification checklist
* Common failure modes
* Commands
* Architecture rules
* How to decide when to ask the human

Do not let the skill become a stale duplicate of all docs. Keep it concise and operational.

The skill is not a substitute for this `CLAUDE.md`. This file is the project-level operating contract. The skill is the repeatable workflow helper.

## Technical Stack

Required:

* C++20
* CMake
* raylib, latest stable release unless pinned otherwise
* Catch2 for tests
* nlohmann/json for data-driven content

Dependency rules:

* Pin dependency versions/tags.
* Do not use floating master/main branches.
* Prefer CMake integration patterns that are standard and reproducible.
* Keep third-party dependency code out of source directories unless explicitly approved.
* Do not add new dependencies without human approval.

Primary target:

* Windows first.
* Avoid Windows-only code unless isolated behind a platform abstraction.

## Build and Test Expectations

Prefer an out-of-source build. Build with **MSVC** (Visual Studio 2022 or newer)
from a Visual Studio developer environment — run `vcvars64.bat`, or use a
"Developer PowerShell/Command Prompt for VS", so `cl` and the bundled CMake/Ninja
are on `PATH`. MinGW/GCC toolchains are not supported.

Typical commands, adjust only if the project structure requires it:

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build-msvc
ctest --test-dir build-msvc --output-on-failure
```

If using the Visual Studio generator (match your installed VS version):

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Debug
ctest --test-dir build-msvc -C Debug --output-on-failure
```

Do not claim “build passes” unless the build command was actually run and completed successfully.

Do not claim “tests pass” unless tests were actually run and completed successfully.

If commands cannot be run, report:

* Not verified
* Why not verified
* Exact commands for the human to run
* Expected successful output/behavior
* What failure information the human should send back

## Game Scope

### Player Party

* Party has 4 characters.
* New game allows choosing and renaming characters.
* Classes:

  1. Knight — durable physical attacker
  2. Ranger — fast attacker, precision attacks
  3. Mage — elemental damage
  4. Cleric — healing/support
  5. Rogue — speed, item efficiency, crits
  6. Guardian — tank/protect/defensive buffs

### Town Hub

The town is functional, not huge.

Required town features:

* Inn/heal
* Item shop
* Equipment shop
* Guild/dungeon selection
* Training hall/class info
* Score board
* Save/load point
* Minimal NPC dialogue

### Dungeons

Dungeons are procedurally generated from deterministic seeds.

Each dungeon must include:

* Start room
* Boss room
* Main path
* Side rooms
* Chests
* Visible enemy teams
* At least 3 mandatory enemy gate encounters before the boss
* Optional guarded chests
* Exit/retreat option from the dungeon menu

No random step encounters.

Enemy teams must be visible before battle.

### Encounters

Visible encounter display should include:

* Team name
* Danger level
* Enemy count
* Optional tags such as `Fast`, `Magic`, `Armored`, `Poison`

### Danger Rating

Danger must be calculated from enemy stats and abilities.

Do not hand-author danger as a label.

Danger formula should be deterministic, explicit, and tested.

Suggested inputs:

* HP
* Attack
* Magic
* Defense
* Speed
* Skill threat
* Team synergy

Suggested visible tiers:

* Trivial
* Easy
* Fair
* Dangerous
* Deadly
* Boss

The formula may compare against dungeon-depth baseline, but displayed danger must still derive from actual enemy stats and abilities.

### Combat

Combat is classic deterministic turn-based JRPG combat, not ATB.

Rules:

* Party size up to 4
* Enemy team size 1–5
* Speed influences turn order
* Commands:

  * Attack
  * Skill/Magic
  * Item
  * Guard
  * Escape
* KO and revive should exist
* Game over if all party members are KO
* All encounters, including bosses, are escapable
* Escaping a normal battle forfeits the guarded chest/reward
* Escaping the boss or leaving the dungeon gives 0 dungeon score

### Scoring

Primary ranking principle:

1. Dungeon completed?
2. Fewest battle turns
3. Most optional danger defeated
4. Most treasure collected
5. Fastest real time only as final tie-breaker

Score components:

* Base completion score
* Battle turn penalty
* Escape penalty where relevant
* Chest bonus
* Boss defeat bonus
* Danger bonus
* No-death bonus

Do not create scoring incentives that reward farming, stalling, or ignoring the fewest-turns premise.

### Chests and Rewards

Dungeons contain useful chests.

Reward types:

* Consumables
* Equipment
* Gold
* Relics/accessories
* Rare class skill scrolls

Most valuable chests should be guarded. Some minor chests may be unguarded.

Guarded chests should clearly show:

* Enemy danger
* Chest rarity
* Fight-to-claim prompt

### Bosses

Each dungeon ends with one boss encounter.

Bosses should have:

* Multiple actions
* Telegraph-style status text
* At least one unique mechanic
* Escapable, but escaping fails the dungeon score

Initial boss archetypes:

1. Brute boss — high HP/attack
2. Sorcerer boss — magic/status
3. Commander boss — summons/minions/buffs

### Content Target for First Complete Version

Minimum complete content:

* 6 playable classes
* 12–18 enemy types
* 6 elite enemy types
* 3 boss archetypes
* 30+ items/equipment pieces
* 8–12 skills/spells per broad category where appropriate
* 3 dungeon themes:

  * Ruined Keep
  * Crystal Mine
  * Hollow Forest
* Infinite generated dungeons via seeds and scaling depth

## Data-Driven Content

Use JSON content files:

* `data/classes.json`
* `data/enemies.json`
* `data/items.json`
* `data/skills.json`
* `data/bosses.json`
* `data/dungeon_themes.json`

Build validators for content data.

Invalid content should produce clear errors, not crashes.

Save files should also be JSON and must include a save version number.

Malformed saves should not crash the game.

## Architecture Rules

Separate:

* Game simulation
* Rendering
* Input mapping
* Asset/resource loading
* Data/content loading
* Save/load
* UI state
* Battle logic
* Dungeon generation

Avoid:

* Monolithic files
* Global mutable state
* Raw owning pointers
* Per-frame heap allocation in hot loops
* Unchecked indexes
* Undefined behavior
* Hidden dependencies on assets not included in the repository
* Production TODO clutter
* Placeholder hacks that become permanent architecture

Prefer:

* RAII for raylib resources
* Deterministic systems where useful
* Seeded random generation
* Small cohesive classes/functions
* Tests for pure logic
* Explicit error messages
* Project-owned abstractions around external libraries where useful

## Rendering and Presentation

Current baseline:

* Internal resolution: 426×240
* Clean scaling to the window
* Top-down town and dungeon exploration
* Side-view battle screen
* Party on right, enemies on left
* Original 16-bit-inspired UI windows

The core loop now works. Post-M10 work may invest in presentation, but only through approved milestones and stable interfaces.

Required completion targets:

* Every required player-facing text element must fit its assigned region or use an explicit wrap, scroll, page, or details mechanism.
* Text placement must use measured bounds; do not estimate width from character count.
* Focus, selection, danger, rarity, and status must not rely on color alone.
* Keyboard and gamepad prompts must reflect active bindings once remapping exists.
* Standard dungeon rooms must become compact, purposeful layouts rather than automatically filling the full exploration screen.
* Graphics, fonts, animations, music, ambience, and SFX must be selected through stable logical asset identifiers and external presentation data.
* Missing assets must degrade safely and visibly/silently as appropriate.
* Human validation is mandatory for readability, control feel, art direction, animation timing, and audio quality.

The 426×240 virtual resolution remains the approved baseline. Changing it is a cross-cutting player-facing decision and requires explicit human approval after evidence from the UI-layout milestone.

Placeholder assets are acceptable only as temporary fallbacks or inside an approved implementation slice. They are not the final presentation target.

Do not use copyrighted assets or imitate a protected game's sprites, music, UI layout, characters, maps, or other distinctive expression.

## Audio

The synthesized placeholder audio remains a safe fallback until the final-audio milestone.

Completion requirements:

* Music, ambience, and SFX must be referenced by logical IDs and resolved through external asset metadata.
* Replacing an audio file for an existing logical role must not require a C++ change.
* Use raylib's streaming music support for final music where appropriate.
* Provide separate master, music, and SFX controls.
* Missing or invalid audio must degrade to silence without crashing.
* State transitions must not stack music tracks or leak resources.
* All shipped audio must have clear provenance, licensing, and attribution records.
* No essential gameplay information may be conveyed by sound alone.

## Security and Robustness

This is a local game, but still follow sane robustness rules.

Required:

* No network features
* No shell execution from game code
* No arbitrary file path loading from untrusted input
* Sanitize/normalize save/data paths
* Validate JSON
* Handle malformed save/content files
* No unsafe deserialization patterns
* No hidden writes outside intended save/config directories

## Milestones

Work in order. Stop after each milestone.

### Milestone 1 — Project Foundation

* CMake project
* raylib window
* Basic game loop
* Internal resolution/scaling
* State stack or equivalent state management
* Resource manager foundation
* Minimal Catch2 test setup
* Basic README/build instructions

### Milestone 2 — Core Data Model

* JSON loaders
* Content schemas/validators
* Basic data structures for classes, enemies, skills, items
* Tests for validators and bad data handling

### Milestone 3 — Town Hub Shell

* New game/party creation
* Class selection and renaming
* Town navigation
* Inn/shop/guild/save/scoreboard placeholders
* Save/load MVP

### Milestone 4 — Dungeon Generator

* Seeded dungeon generation
* Room graph/grid
* Start room, boss room, main path, side rooms
* Visible enemy teams
* Chests
* At least 3 mandatory enemy gates before boss
* Dungeon exit/retreat flow

### Milestone 5 — Battle System MVP

* Side-view battle screen
* Turn order
* Attack/Skill/Item/Guard/Escape
* Damage/healing
* Victory/defeat/escape
* KO/revive
* Return to dungeon after battle

### Milestone 6 — Danger Rating and Scoring

* Stat-derived danger formula
* Visible danger labels
* Battle turn tracking
* Dungeon score calculation
* Scoreboard
* Tests for danger and scoring

### Milestone 7 — Content Pass

* 6 playable classes
* Enemies/elites/bosses
* Items/equipment/relics
* Skills/spells
* 3 dungeon themes
* Balance pass sufficient for playable runs

### Milestone 8 — Presentation Pass

* 16-bit-style UI polish
* Basic animations
* Transitions
* Placeholder audio/music/SFX
* Better feedback text
* Controls/help screen

### Milestone 9 — Balancing and Validation Pass

* Difficulty curves
* Score sanity
* Malformed data testing
* Save compatibility testing
* Performance cleanup
* Bug fixing

### Milestone 10 — Release Packaging Pass

* Final README
* Build/run instructions
* Controls
* Known limitations
* Smoke tests
* Clean project structure
* Final playable build target

### Post-M10 Completion Program

The detailed post-M10 milestone index and status live in `docs/milestones.md`. Strategic direction and quality targets live in `docs/completion_roadmap.md`. The active `docs/milestone_notes/MXX_*.md` file defines the exact current slice.

M10 was manually tested and explicitly approved by the owner on 2026-07-19. The completion program runs M11 through M24; every milestone from M11 onward has exactly one authoritative note under `docs/milestone_notes/`. Before starting any milestone, re-audit and refresh its note against the then-current repository, obtain the owner's explicit authorization to begin, complete only that milestone, report using `docs/milestone_completion_template.md`, and stop.

Do not copy the full post-M10 roadmap into this operating contract or treat the roadmap as approval to work ahead.

## Milestone Completion Report Format

Use `docs/milestone_completion_template.md` as the full report template. In summary, at the end of every milestone, report:

1. Completed work
2. Files created/changed
3. Build commands run
4. Test commands run
5. Build/test results
6. Manual validation needed
7. Known issues/risks
8. Whether the milestone is complete
9. Recommendation for the next milestone

Then stop.

Do not continue until the human approves.

## Manual Validation Template

When manual validation is needed, provide a checklist like this:

````markdown
## Human validation needed

Please run:

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build-msvc
ctest --test-dir build-msvc --output-on-failure
.\build-msvc\CrystalDungeons.exe
````

Check:

* Window opens
* Internal scaling looks correct
* Keyboard input responds
* No crash on exit
* Audio missing/failing does not crash, if audio exists
* Current milestone feature behaves as described

Send back:

* Console output if build/test fails
* Screenshot if rendering/layout looks wrong
* Short description of any gameplay issue

```

## Definition of Done

A milestone is done only when:
- It stays within milestone scope
- The project builds, or exact unverified build instructions are provided
- Tests pass, or exact unverified test instructions are provided
- Docs are updated if behavior/design changed
- Known issues are clearly listed
- Manual validation needs are clearly listed
- No unrelated refactors are included
- No next-milestone work is smuggled in

## Current Task Selection

1. Determine the current milestone and approval state from `docs/milestones.md`.
2. Read the active milestone note, if one exists.
3. Inspect the current checkout before planning.
4. Do not restart historical milestones or follow stale task text from earlier project phases.
5. M1–M10 are all `complete (approved)`; M10 was approved by the owner on 2026-07-19 (last planning review at commit `a316f244e870718aa27d9995dc871e11572ad429`). The next milestone is M11, status `planned`.
6. Do not begin M11 without the owner's explicit authorization. When authorized, first re-audit `docs/milestone_notes/M11_completion_baseline.md` against the current checkout, then complete only M11, report using `docs/milestone_completion_template.md`, and stop.
