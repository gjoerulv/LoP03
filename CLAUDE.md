# CLAUDE.md

## Project Identity

This project is a complete 16-bit-inspired turn-based JRPG roguelite built in C++20 with raylib.

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
2. Read `docs/game_design.md`, `docs/technical_design.md`, `docs/milestones.md`, and any current milestone notes if they exist.
3. Inspect the repository before proposing changes.
4. Identify the current milestone and whether it is complete.
5. Verify build/test commands before claiming work is complete.

If these docs do not exist yet, create them during the appropriate early milestone and keep them updated.

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

Prefer an out-of-source build.

Typical commands, adjust only if the project structure requires it:

```powershell
cmake -S . -B build -G "Ninja"
cmake --build build
ctest --test-dir build --output-on-failure
```

If using Visual Studio generator:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
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

Target:

* Internal resolution: 426×240
* Clean scaling to the window
* Top-down town and dungeon exploration
* Side-view battle screen
* Party on right, enemies on left
* Original 16-bit-inspired UI windows

Placeholder sprites are acceptable.

Do not use copyrighted assets.

Do not spend excessive time on polish before the core loop works.

## Audio

Provide placeholder music and sound effects.

Audio requirements:

* Easy to replace later
* Isolated under an assets/audio-style structure
* Graceful failure if audio is missing
* No hard crash on failed audio load
* Replacing audio should not require code changes if filenames/formats remain compatible

Music and SFX do not need final creative direction yet.

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

## Milestone Completion Report Format

At the end of every milestone, report:

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
cmake -S . -B build -G "Ninja"
cmake --build build
ctest --test-dir build --output-on-failure
.\build\CrystalDungeons.exe
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

## Current First Task

If the project has no skill yet:
1. Interview the human only for material ambiguities.
2. Always include recommended answers.
3. Create `.claude/skills/crystal-dungeons/SKILL.md`.
4. Stop for approval if the human wants to review the skill.

After the skill is approved:
1. Start Milestone 1 only.
2. Provide the Milestone 1 implementation plan.
3. Implement Milestone 1.
4. Run/build/test where possible.
5. Report completion using the milestone report format.
6. Stop and wait for approval.
```
