# M11 — Completion Baseline and Presentation Audit

> Status: ☐ not started.
>
> Approval gate: do not begin M11 until the human has explicitly approved M10.
>
> Baseline for this specification: commit
> `e293f49f35ddd3bd4c49202194cadd96aead7811`.

## 1. Objective

Establish a trustworthy baseline for the post-M10 completion program before
changing UI, controls, assets, dungeon rooms, or presentation.

This milestone converts broad complaints such as “basic graphics,” “text does
not fit,” “controls are unclear,” and “rooms are too large” into a concrete,
prioritized defect register tied to actual screens, code paths, and acceptance
criteria.

M11 is primarily an audit and documentation milestone. It must not become an
unapproved implementation pass.

## 2. Why this milestone comes first

The current project is mechanically complete, but presentation work cuts across
many systems:

- fixed-coordinate UI drawing;
- font measurement and wrapping;
- input bindings and prompts;
- state transitions;
- resource loading;
- audio;
- room geometry;
- battle feedback;
- content descriptions and dynamic values.

Making broad changes without a captured baseline would create three risks:

1. fixing low-value cosmetic issues while missing clipping or usability
   blockers;
2. designing final assets for layouts that are about to change;
3. allowing Claude Code to expand scope across several milestones at once.

## 3. Required reading

Before proposing work, read:

- `CLAUDE.md`;
- `docs/game_design.md`;
- `docs/technical_design.md`;
- `docs/milestones.md`;
- `docs/completion_roadmap.md`;
- this file;
- `.claude/skills/crystal-dungeons/SKILL.md`.

Then inspect the repository, especially:

- `src/ui/`;
- `src/states/`;
- `src/input/`;
- `src/resource/`;
- `src/audio/`;
- `src/render/`;
- `src/town/`;
- `src/dungeon/`;
- `src/battle/`;
- `data/`;
- `tests/`;
- root `CMakeLists.txt` and relevant files under `cmake/`.

Do not rely only on this note’s baseline observations. Confirm them against the
current checkout because the repository may have advanced.

## 4. Scope

### 4.1 Build and test baseline

Run the supported MSVC/Ninja build from a Visual Studio developer environment:

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build-msvc
ctest --test-dir build-msvc --output-on-failure
```

Record:

- current commit;
- configure result;
- build result;
- warnings from project code;
- test count and result;
- executable startup and clean exit;
- any mismatch between docs and actual commands.

Do not claim verification if the commands were not run.

### 4.2 Screen and flow inventory

Inventory every player-facing state, modal, overlay, and important dynamic
variant.

At minimum include:

- title/main menu;
- new game and party creation;
- name entry with minimum and maximum names;
- Continue/save-slot states with empty, valid, and long metadata;
- town exploration and interaction prompt;
- Inn;
- Item Shop;
- Equipment Shop;
- Training Hall;
- Guild/dungeon selection;
- Scoreboard;
- Save Point;
- Help/controls;
- pause/town menu;
- dungeon exploration;
- dungeon pause/retreat;
- unopened, guarded, and opened chest states;
- normal gate, elite, and boss encounter prompts;
- battle command selection;
- skill selection with longest description/cost;
- item selection with long names and counts;
- target selection with maximum party/enemy sizes;
- status-heavy battle;
- boss telegraph;
- victory, defeat, and escape;
- dungeon result and score breakdown;
- error/fallback states reachable through malformed or missing external data.

For each entry record:

- source state/class;
- entry path;
- dynamic inputs;
- current controls;
- text/layout risks;
- visual hierarchy issues;
- missing feedback;
- screenshot status;
- severity and recommended owning milestone.

### 4.3 Screenshot baseline

Capture the inventory where the available environment permits.

Required display cases:

- native virtual output at 426×240;
- default 1278×720 window;
- 1280×720;
- 1920×1080;
- a narrow resizable window;
- a near-square resizable window;
- integer and non-integer scale situations.

Prefer native-resolution captures for layout review and at least one window
capture for scaling/letterboxing review.

If Claude cannot launch or see the game:

- state that screenshot capture is not verified;
- provide exact navigation steps and filenames for the human;
- do not fabricate visual findings;
- continue with static code/text risk analysis, clearly marked as such.

Recommended capture location if screenshots are committed:

```text
docs/screenshots/m11_baseline/
```

Use small PNGs and meaningful names. Do not commit duplicate window-scaled
captures when the native image demonstrates the same issue. If the human
prefers screenshots outside Git, attach them to the milestone report and keep a
markdown index with references.

### 4.4 Presentation defect register

Create:

```text
docs/presentation_audit.md
```

Each defect must include:

- stable ID, for example `UI-TEXT-001`;
- screen/state;
- observed behavior;
- expected behavior;
- reproduction steps;
- severity;
- frequency;
- affected input/display modes;
- likely subsystem owner;
- recommended milestone/slice;
- screenshot/reference where available;
- whether the finding is observed or inferred from static analysis.

Use these categories:

- text clipping/overflow;
- overlap or unsafe bounds;
- weak hierarchy/readability;
- insufficient contrast;
- unclear focus/selection;
- inconsistent controls;
- hard-coded control prompts;
- state-transition/input buffering;
- excessive empty space;
- oversized dungeon rooms;
- unclear navigation or interactables;
- placeholder art;
- animation/feedback gap;
- audio gap;
- asset-pipeline coupling;
- inconsistent terminology;
- accessibility barrier;
- onboarding/comprehension issue;
- performance/resource-lifetime risk;
- test/validation gap.

Severity:

- **Blocker** — prevents completion, loses data, traps input, crashes, or makes
  required information unavailable.
- **High** — materially harms readability, control, or understanding in a common
  flow.
- **Medium** — visible quality/usability defect with a workable path around it.
- **Low** — polish issue with limited impact.

Do not inflate every visual imperfection to High. Prioritize by player impact,
frequency, and rework dependency.

### 4.5 Initial UI style guide

Create:

```text
docs/ui_style_guide.md
```

This is a baseline contract, not a final art bible. It should define:

- current virtual resolution and safe bounds;
- provisional margins and spacing scale;
- text roles found in the current game;
- minimum contrast targets;
- focus/selection requirements;
- disabled-state behavior;
- color-plus-secondary-indicator rule;
- footer/control-hint reservation;
- wrapping/scrolling/pagination/ellipsis policies;
- confirmation and error-message conventions;
- known unresolved decisions for M12/M15.

Do not invent final colors, fonts, frames, or art motifs before M15.

### 4.6 Initial control standard

Create:

```text
docs/control_standard.md
```

Document:

- current semantic actions and default bindings;
- current raw-input exceptions, if any;
- expected Confirm/Cancel/Menu semantics;
- navigation rules for lists, tabs, grids, and modals;
- keyboard-only and gamepad-only requirements;
- D-pad/left-stick requirements;
- repeat, deadzone, and transition-input risks to resolve in M13;
- prompt-generation requirement after remapping;
- controller disconnect behavior target;
- unresolved actions such as Details/PageLeft/PageRight.

Do not implement remapping or settings in M11.

### 4.7 Asset-pipeline decision draft

Create:

```text
docs/asset_pipeline.md
```

Document the current state and proposed M14 contract:

- current `ResourceManager` caller-supplied key/path behavior;
- current empty `assets/` tree;
- current synthesized `AudioManager` behavior;
- proposed logical asset ID model;
- proposed versioned manifest responsibilities;
- required fallback behavior;
- proposed directory organization;
- build/package copying requirements;
- attribution/license tracking;
- questions requiring approval before M14.

Do not finalize the public manifest schema or change resource APIs in M11.

### 4.8 Manual test matrix

Create:

```text
docs/manual_test_matrix.md
```

The matrix must cover:

- screen/flow;
- test data variant;
- keyboard;
- gamepad D-pad;
- gamepad left stick;
- display/window case;
- text/readability result;
- focus/navigation result;
- audio result where relevant;
- screenshot/evidence;
- pass/fail/not run.

Include explicit maximum-content cases rather than only normal happy paths.

### 4.9 Prioritized next slice

Conclude the audit with a recommendation for M12-a.

The default recommendation is:

> Central font-aware text measurement, bounded wrapping, long-token handling,
> overflow diagnostics, and migration of Help plus one representative modal.

Change that recommendation only if the audit finds a more severe prerequisite.
Explain the evidence and trade-off.

## 5. Explicit non-goals

M11 must not:

- redesign screens;
- change the virtual resolution;
- implement a UI framework;
- change controls or bindings;
- add settings/remapping;
- create the final asset manifest schema;
- replace synthesized audio;
- add sprites, tilesets, fonts, or music;
- change dungeon generation or room dimensions;
- change battle, progression, scoring, economy, saves, or content schemas;
- perform broad cleanup refactors;
- begin M12 implementation.

Small documentation corrections are allowed when they are factual and directly
support the audit. Any product-code change requires separate human approval and
must be justified as necessary to complete M11.

## 6. Required output files

Expected committed files from M11:

```text
docs/presentation_audit.md
docs/ui_style_guide.md
docs/control_standard.md
docs/asset_pipeline.md
docs/manual_test_matrix.md
```

Also update only when necessary:

```text
docs/milestones.md
docs/technical_design.md
README.md
.claude/skills/crystal-dungeons/SKILL.md
```

Screenshot files are conditional on the agreed evidence policy.

## 7. Acceptance criteria

M11 is complete only when:

- M10 has been explicitly approved first;
- current commit, build, test, and executable status are reported honestly;
- all major screens and dynamic variants are inventoried;
- observed findings are distinguished from static-analysis risks;
- the defect register is prioritized and assigns ownership to later milestones;
- the initial UI, control, asset, and manual-test contracts exist;
- no final art direction or public schema was silently decided;
- no unrelated code changes were made;
- the human approves the defect priorities and recommended M12-a slice.

## 8. Completion report additions

In addition to the standard milestone report, include:

- count of inventoried screens/variants;
- defect counts by severity and category;
- screenshot coverage count;
- number of unverified visual cases;
- top five blockers/high-impact defects;
- recommended M12-a scope;
- decisions deferred to M12–M16;
- explicit statement that M12 has not begun.

Then stop and wait for human approval.
