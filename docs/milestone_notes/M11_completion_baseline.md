# M11 — Completion Baseline and Presentation Audit

## A. Status and authority

- **Status:** implemented, awaiting manual approval (implemented 2026-07-19;
  see section N for the as-implemented record).
- **Last reviewed repository commit:**
  `827187124f9123c7275abcbbb45d5a4aabedf92b` (2026-07-19).
- **Approval gate:** M10 was manually tested and approved by the owner on
  2026-07-19, so the M10 prerequisite is satisfied. Do not begin M11 without
  the owner's explicit authorization to start it.
- **Relationship to `docs/milestones.md`:** this note is the single
  authoritative detailed scope for the M11 entry in the ledger. The ledger
  holds the current status; this note holds the plan. On conflict, follow the
  authority order in `CLAUDE.md`.
- This note must be **re-audited and refreshed against the then-current
  repository** before implementation begins. Its baseline observations
  describe the repository at the commit above and may go stale.

## B. Problem statement

The project is mechanically complete (M1–M10 approved) but its presentation is
prototype-level, and presentation work cuts across many systems: fixed-
coordinate UI drawing, font measurement and wrapping, input bindings and
prompts, state transitions, resource loading, audio, room geometry, battle
feedback, and dynamic content text.

Concrete repository evidence at the reviewed commit:

- `src/ui/UiDraw.*` exposes only panel/centered-text/menu helpers; states draw
  text directly (~48 raw `DrawText` call sites) with exactly one
  `MeasureText` use (`drawTextCentered`);
- `assets/` is empty (`.gitkeep`) and `AudioManager` synthesizes all audio;
- `ResourceManager` requires callers to pass both cache key and file path;
- there is no settings file, no remapping UI, and one production raw-input
  exception (`PartyCreationState` reads Backspace/Enter directly);
- `DungeonState` hard-codes 26×15 rooms at 16-pixel tiles, filling the whole
  426×240 exploration surface.

Making broad changes without a captured baseline risks: fixing low-value
cosmetic issues while missing clipping or usability blockers; designing final
assets for layouts about to change; and scope expansion across several
milestones at once. M11 converts broad complaints ("basic graphics", "text
does not fit", "controls are unclear", "rooms are too large") into a concrete,
prioritized defect register tied to actual screens, code paths, and acceptance
criteria.

M11 is an audit and documentation milestone. It must not become an unapproved
implementation pass.

## C. Goals

- A verified (or honestly reported unverified) build/test/run baseline at the
  audited commit.
- A complete inventory of player-facing screens, modals, overlays, and
  important dynamic variants, each with recorded risks and severity.
- A prioritized presentation defect register with stable IDs, categories,
  severities, and recommended owning milestones.
- Initial contracts: UI style guide, control standard, asset-pipeline decision
  record, and manual test matrix.
- An evidence-based recommendation for the first M12 slice (M12-a).

## D. Non-goals

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
support the audit. Any product-code change requires separate owner approval
and must be justified as necessary to complete M11.

## E. Dependencies

- M10 `complete (approved)` — satisfied 2026-07-19.
- Owner authorization to start M11.
- A working MSVC/Ninja build environment (Visual Studio developer shell).
- If Claude cannot launch or observe the game, the owner must capture the
  screenshot baseline following the exact steps Claude provides (see slice
  M11-c and section J).

Required reading before proposing work: `CLAUDE.md`, `docs/game_design.md`,
`docs/technical_design.md`, `docs/milestones.md`, `docs/completion_roadmap.md`,
this note, and `.claude/skills/crystal-dungeons/SKILL.md`. Then inspect the
repository — especially `src/ui/`, `src/states/`, `src/input/`,
`src/resource/`, `src/audio/`, `src/render/`, `src/town/`, `src/dungeon/`,
`src/battle/`, `data/`, `tests/`, and the root `CMakeLists.txt` plus `cmake/`.
Do not rely only on this note's baseline observations.

## F. Proposed slices

### M11-a — Build and test baseline

Run the supported MSVC/Ninja build from a Visual Studio developer environment:

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build-msvc
ctest --test-dir build-msvc --output-on-failure
```

Record: current commit; configure result; build result; warnings from project
code; test count and result; executable startup and clean exit; any mismatch
between docs and actual commands. Do not claim verification if the commands
were not run.

### M11-b — Screen and flow inventory

Inventory every player-facing state, modal, overlay, and important dynamic
variant. At minimum include:

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

For each entry record: source state/class; entry path; dynamic inputs; current
controls; text/layout risks; visual hierarchy issues; missing feedback;
screenshot status; severity and recommended owning milestone.

### M11-c — Screenshot baseline

Capture the inventory where the available environment permits. Required
display cases:

- native virtual output at 426×240;
- default 1278×720 window;
- 1280×720;
- 1920×1080;
- a narrow resizable window;
- a near-square resizable window;
- integer and non-integer scale situations.

Prefer native-resolution captures for layout review and at least one window
capture for scaling/letterboxing review.

If Claude cannot launch or see the game: state that screenshot capture is not
verified; provide exact navigation steps and filenames for the owner; do not
fabricate visual findings; continue with static code/text risk analysis,
clearly marked as such.

Recommended capture location if screenshots are committed:
`docs/screenshots/m11_baseline/`. Use small PNGs and meaningful names. Do not
commit duplicate window-scaled captures when the native image demonstrates the
same issue. If the owner prefers screenshots outside Git, attach them to the
milestone report and keep a markdown index with references.

### M11-d — Presentation defect register

Create `docs/presentation_audit.md`. Each defect must include: stable ID (for
example `UI-TEXT-001`); screen/state; observed behavior; expected behavior;
reproduction steps; severity; frequency; affected input/display modes; likely
subsystem owner; recommended milestone/slice; screenshot/reference where
available; whether the finding is observed or inferred from static analysis.

Categories: text clipping/overflow; overlap or unsafe bounds; weak
hierarchy/readability; insufficient contrast; unclear focus/selection;
inconsistent controls; hard-coded control prompts; state-transition/input
buffering; excessive empty space; oversized dungeon rooms; unclear navigation
or interactables; placeholder art; animation/feedback gap; audio gap;
asset-pipeline coupling; inconsistent terminology; accessibility barrier;
onboarding/comprehension issue; performance/resource-lifetime risk;
test/validation gap.

Severity:

- **Blocker** — prevents completion, loses data, traps input, crashes, or
  makes required information unavailable.
- **High** — materially harms readability, control, or understanding in a
  common flow.
- **Medium** — visible quality/usability defect with a workable path around it.
- **Low** — polish issue with limited impact.

Do not inflate every visual imperfection to High. Prioritize by player impact,
frequency, and rework dependency.

### M11-e — Initial contracts

Create four baseline documents. These are contracts for later milestones, not
final designs.

**`docs/ui_style_guide.md`** — current virtual resolution and safe bounds;
provisional margins and spacing scale; text roles found in the current game;
minimum contrast targets; focus/selection requirements; disabled-state
behavior; color-plus-secondary-indicator rule; footer/control-hint
reservation; wrapping/scrolling/pagination/ellipsis policies; confirmation and
error-message conventions; known unresolved decisions for M12/M15. Do not
invent final colors, fonts, frames, or art motifs before M15.

**`docs/control_standard.md`** — current semantic actions and default
bindings; current raw-input exceptions; expected Confirm/Cancel/Menu
semantics; navigation rules for lists, tabs, grids, and modals; keyboard-only
and gamepad-only requirements; D-pad/left-stick requirements; repeat,
deadzone, and transition-input risks to resolve in M13; prompt-generation
requirement after remapping; controller-disconnect behavior target; unresolved
actions such as Details/PageLeft/PageRight. Do not implement remapping or
settings in M11.

**`docs/asset_pipeline.md`** — current `ResourceManager` caller-supplied
key/path behavior; current empty `assets/` tree; current synthesized
`AudioManager` behavior; proposed logical asset ID model; proposed versioned
manifest responsibilities; required fallback behavior; proposed directory
organization; build/package copying requirements; attribution/license
tracking; questions requiring approval before M14. Do not finalize the public
manifest schema or change resource APIs in M11.

**`docs/manual_test_matrix.md`** — a matrix covering: screen/flow; test data
variant; keyboard; gamepad D-pad; gamepad left stick; display/window case;
text/readability result; focus/navigation result; audio result where relevant;
screenshot/evidence; pass/fail/not run. Include explicit maximum-content cases
rather than only normal happy paths.

### M11-f — Prioritized next slice

Conclude the audit with a recommendation for M12-a. The default recommendation
is:

> Central font-aware text measurement, bounded wrapping, long-token handling,
> overflow diagnostics, and migration of Help plus one representative modal.

Change that recommendation only if the audit finds a more severe prerequisite.
Explain the evidence and trade-off.

## G. Expected systems affected

Documentation only. Expected committed outputs:

```text
docs/presentation_audit.md
docs/ui_style_guide.md
docs/control_standard.md
docs/asset_pipeline.md
docs/manual_test_matrix.md
```

Updated only when necessary: `docs/milestones.md`, `docs/technical_design.md`,
`README.md`, `.claude/skills/crystal-dungeons/SKILL.md`. Screenshot files are
conditional on the agreed evidence policy. No `src/`, `data/`, `assets/`, or
CMake changes are expected; any would require separate owner approval.

## H. Data, schema, and save compatibility

**No impact.** M11 changes no JSON schemas, asset manifests, save files,
settings files, deterministic seeds, scoring, or public content identifiers.

## I. Automated validation

- Configure, build, and run the existing test suite (125 tests at the reviewed
  commit) with the commands in slice M11-a; report exact results.
- Launch the executable and confirm startup and clean exit.
- No new automated tests are expected from an audit milestone.
- Anything not run must be reported as **not verified** with exact commands
  and expected output for the owner.

## J. Manual validation for the owner

- Review the screen/flow inventory for completeness against your knowledge of
  the game.
- Review the defect register: agree/adjust severities and owning milestones.
- Review the four initial contracts for direction errors.
- If Claude could not capture screenshots: follow the provided navigation
  steps and capture the listed cases (native 426×240, default 1278×720,
  1280×720, 1920×1080, narrow window, near-square window), then send them
  back.
- Approve or amend the recommended M12-a slice.

## K. Acceptance criteria

M11 is complete only when:

- current commit, build, test, and executable status are reported honestly;
- all major screens and dynamic variants are inventoried;
- observed findings are distinguished from static-analysis risks;
- the defect register is prioritized and assigns ownership to later
  milestones;
- the initial UI, control, asset, and manual-test contracts exist;
- no final art direction or public schema was silently decided;
- no unrelated code changes were made;
- the owner approves the defect priorities and recommended M12-a slice.

## L. Risks and failure modes

- **Scope creep into implementation** — the audit tempts "quick fixes"; any
  product-code change needs separate approval.
- **Unverifiable visuals** — if Claude cannot see the game, findings must be
  labeled static-analysis; fabricated visual claims would poison the register.
- **Severity inflation** — over-rating cosmetic issues buries the real
  blockers; prioritize by player impact, frequency, and rework dependency.
- **Stale baseline** — findings are tied to the audited commit; if the repo
  advances, re-verify before relying on them.
- **Missing maximum-content cases** — auditing only happy paths would let
  text-overflow defects survive into M12 acceptance.
- **Second-order risk** — wrong milestone-ownership recommendations here
  misroute work for M12–M18; the owner review of priorities is the control.

## M. Required documentation updates on completion

- `docs/milestones.md` — M11 status to `implemented, awaiting manual
  approval`, then `complete (approved)` after owner sign-off.
- This note — record the actual audit results, deviations, and evidence
  locations.
- `docs/technical_design.md` — only if the audit corrects a factual claim.
- `README.md` — only if commands/instructions proved wrong.
- `.claude/skills/crystal-dungeons/SKILL.md` — new gotchas discovered while
  auditing.
- Completion report per `docs/milestone_completion_template.md`, including:
  count of inventoried screens/variants; defect counts by severity and
  category; screenshot coverage count; number of unverified visual cases; top
  five blockers/high-impact defects; recommended M12-a scope; decisions
  deferred to M12–M16; explicit statement that M12 has not begun.

## N. As-implemented record (2026-07-19)

- **Audited commit:** `827187124f9123c7275abcbbb45d5a4aabedf92b`, clean tree.
- **M11-a:** MSVC 19.51 / Ninja Debug configure+build clean (fresh
  FetchContent clones verified on pinned tags raylib `6.0`, Catch2
  `v3.15.1`, json `v3.12.0`); zero project warnings; `ctest` 125/125 passed
  (10.73s); executable launched and exited cleanly (code 0).
- **M11-b:** 31-entry screen/flow inventory with maximum-content cases —
  `docs/presentation_audit.md` §1.
- **M11-c (partial, deviation):** 7 live captures at the default 1278×720
  window (`docs/screenshots/m11_baseline/01…06`), taken by driving the game
  with `PostMessage` keystrokes addressed to its window handle. Native
  426×240 capture is not possible without engine support (M23 tooling);
  window cases W2–W6 and ~26 remaining screens are delegated to the owner via
  the matrix (rows list exact steps and target filenames). **Incident:** the
  first capture attempt used focus-dependent `SendKeys`; three keystrokes
  (Down, Down, Enter) leaked into the foreground application instead of the
  game, and two mis-captured PNGs (showing the foreground app, not the game)
  were created and immediately deleted. Method switched to
  PostMessage-to-HWND + topmost-no-activate, which cannot leak input.
- **M11-d:** 24-defect register (1 Blocker, 7 High, 9 Medium, 7 Low; 8
  observed / 3 static-certain / 13 static) — `docs/presentation_audit.md` §2.
- **M11-e:** initial contracts written: `docs/ui_style_guide.md`,
  `docs/control_standard.md`, `docs/asset_pipeline.md`,
  `docs/manual_test_matrix.md`.
- **M11-f:** M12-a recommendation = default (measurement/wrapping/overflow
  diagnostics + Help migration) **plus** the battle bottom panel as the
  representative modal, and two one-line fixes for owner approval
  (UI-TEXT-010 stale title label, UI-LAYOUT-009 overlay default-off) —
  `docs/presentation_audit.md` §4.
- **No product code, data, assets, or build files were changed.** All
  changes are documentation plus baseline screenshots.
- **Notable audit results beyond the plan:** design-contract violation
  UI-INFO-005 (encounter team name/members not shown — `game_design.md` §6),
  gamepad soft-lock Blocker UI-INPUT-001, advertised-but-missing left-stick
  support CTRL-006, and 64 authored descriptions that are never rendered
  (UI-INFO-004).
