# M13 — Input Consistency, Remapping, and Settings

## A. Status and authority

- **Status:** complete (approved by the owner on 2026-07-19; implemented
  2026-07-19; see section N for the as-implemented record and deviations).
- **Last reviewed repository commit:** `a247b09` (2026-07-19).
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M13 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- This note must be **re-audited and refreshed against the then-current
  repository before implementation begins** — in particular against
  `docs/control_standard.md` produced by M11 and the M12 layout primitives the
  settings/remapping screens will use.

## B. Problem statement

At the reviewed commit:

- `src/input/` provides a solid action abstraction (`InputAction`, `InputMap`,
  pure `InputQuery` resolution, keyboard + gamepad defaults), but the action
  vocabulary is small and fixed, and nothing persists;
- `InputMap` supports `bindKey`/`bindButton`, yet there is no player-facing
  remapping and no settings file of any kind — bindings and volumes reset
  every launch;
- one production raw-input exception exists: `PartyCreationState` reads
  `IsKeyPressed(KEY_BACKSPACE)`/`IsKeyPressed(KEY_ENTER)` directly for name
  entry, bypassing the input layer;
- prompts are hard-coded strings ("Enter", "Esc", "A", "B") in states and in
  `HelpState`/README — wrong the moment remapping exists;
- there is no navigation key-repeat, no analog deadzone/hysteresis policy, and
  no suppression of buffered Confirm across state transitions (a CLAUDE.md
  completion target);
- `AudioManager` has volume controls but no persistence.

## C. Goals

- All production states consume semantic actions; raw-input reads exist only
  inside the input/platform adapter (text entry included).
- Menu navigation feels consistent: initial-delay + repeat on held directions;
  analog stick with deadzone and hysteresis; no double-activation across state
  transitions.
- Prompts and the Help screen derive their labels from the active bindings.
- A versioned, defensively-loaded settings file persists bindings and options
  in the user data dir.
- Keyboard and gamepad remapping with conflict recovery that can never strand
  the player.
- An initial options surface (display/audio/gameplay/accessibility) that
  existing systems can honor.

## D. Non-goals

- No final UI theme or art (M15).
- No new gameplay actions beyond what current screens need (Details and
  page-left/page-right may be added only where the M11 control standard
  identified a concrete use).
- No audio content changes (M21) — only volume persistence.
- No touch/mouse-first redesign; mouse remains optional convenience.
- No accessibility features beyond the initial options list (M22 completes
  them).

## E. Dependencies

- M11 `docs/control_standard.md` (semantic conventions, unresolved actions,
  disconnect target).
- M12 layout primitives — settings and remapping screens are list-heavy and
  must be built on the measured-layout system, not fixed coordinates.
- Owner decisions needed before or during implementation:
  - final minimum settings list for this milestone (recommendation in F-8);
  - whether remapping UI allows combining keyboard+gamepad edits in one screen
    or two (recommendation: two lists, one per device).

## F. Proposed slices

### M13-1 — Raw-input audit and removal

- Sweep `src/` for `IsKey*`/`IsGamepadButton*`/`GetCharPressed` outside
  `src/input/`; the known offender is `PartyCreationState` (Backspace/Enter
  during name entry).
- Route text entry through the input layer: extend the `InputQuery`/adapter
  with a character-input and text-editing path so `ui::TextInput` consumes
  injected events (stays headless-testable).
- Tests: text-entry resolution through fakes; a static/CI-style check or test
  asserting no raw-input symbols in state code is optional but welcome.

### M13-2 — Semantic-action vocabulary

- Add only actions current screens need per the control standard (candidates:
  `Details`, `PageLeft`, `PageRight`, explicit `Adjust` semantics used by the
  Guild pickers).
- Update `InputMap` defaults for both devices; keep resolution pure.

### M13-3 — Navigation feel

- Initial-delay + repeat for held directional actions (pure, time-injected,
  tested).
- Analog deadzone + hysteresis so stick navigation cannot flutter.
- Transition-input suppression: entering a state consumes buffered
  Confirm/Cancel so a held press cannot trigger an unintended action
  (CLAUDE.md/roadmap requirement).

### M13-4 — Active-device tracking

- Track the most recent *intentional* input device (threshold so stick drift
  does not flip prompts); expose to UI for prompt selection.

### M13-5 — Binding-derived prompts

- A prompt-label service mapping action → current binding label for the active
  device; replace hard-coded prompt strings in states and `HelpState`.
- Tests: label generation for defaults and remapped bindings.

### M13-6 — Settings persistence

- Versioned JSON settings file (e.g. `settings.json`, version `1`) in
  `paths::userDataDir()`, written atomically; defensive load via the existing
  `ObjectReader`/`LoadReport` pattern — malformed/foreign-version settings
  fall back to defaults, never crash, never partially apply.
- Persist: bindings (both devices), master/music/SFX volume, and the options
  from M13-8.
- Tests: round-trip, malformed file, unknown fields, missing file, version
  mismatch.

### M13-7 — Remapping UI with conflict recovery

- Listen-for-input rebind flow per action, per device, built on M12 list
  primitives; cancel path always available.
- Conflict policy: rebinding to a bound input swaps or blocks with a message —
  never leaves an action unbound; core actions (navigate/Confirm/Cancel)
  cannot all be unbound; a "reset to defaults" always exists and is reachable
  by both devices.
- Tests: conflict resolution, reset, cannot-strand invariants (pure map
  logic).

### M13-8 — Initial options

Recommended minimum (from the roadmap; confirm with owner):

- fullscreen/windowed and scale mode;
- master/music/SFX volume (wired to `AudioManager`, persisted);
- battle/message speed placeholder values that M18/M22 will honor (store now,
  apply where systems exist);
- reset bindings and settings to defaults.

Defer shake/flash intensity and high-contrast toggles to M18/M22 where the
effects/UI they control exist — do not ship dead options unless the owner
prefers placeholders.

## G. Expected systems affected

- `src/input/` — `InputAction`, `InputMap`, `RaylibInputQuery`, new repeat/
  device-tracking/prompt-label logic.
- New settings module (location decided at implementation: `src/save/` sibling
  or `src/settings/`).
- `src/ui/TextInput` + `src/states/PartyCreationState.cpp` (text-entry
  rerouting).
- `src/states/` — prompt strings everywhere; new settings/remapping states;
  `HelpState` rewritten to render live bindings.
- `src/audio/AudioManager` — volume persistence hookup.
- `src/core/Application`/`AppContext` — settings lifetime and window-mode
  application.
- `tests/` — new input/settings test files.
- `README.md` controls table — note that defaults are listed and remapping
  exists.

## H. Data, schema, and save compatibility

- **New file:** versioned `settings.json` in the user data dir — a new public
  schema; its shape needs owner sign-off before freezing (public-schema rule).
- **Save files: no impact.** Content schemas: no impact. Deterministic seeds
  and scoring: no impact.

## I. Automated validation

- Pure tests: text-entry injection, repeat timing, deadzone/hysteresis,
  device tracking, prompt labels, conflict/reset invariants, settings
  round-trip + malformed/version cases.
- Full suite green in Debug and Release.
- Existing `test_input_map.cpp` extended, not replaced.

## J. Manual validation for the owner

- Complete a full run (town → dungeon → boss → score) **keyboard-only**, then
  **gamepad-only** (D-pad, then left stick).
- Remap several actions on both devices; confirm every prompt and the Help
  screen update; restart the game and confirm persistence.
- Attempt to break remapping: bind conflicts, try to unbind Confirm/Cancel,
  reset to defaults from both devices.
- Unplug the controller mid-menu and mid-exploration: game falls back to
  keyboard without locking the screen.
- Corrupt `settings.json` by hand; game starts with defaults and no crash.
- Hold Confirm through a state transition; confirm nothing double-activates.

## K. Acceptance criteria

- Complete keyboard-only and gamepad-only runs are possible.
- D-pad and left stick both navigate all UIs.
- Confirm/Cancel semantics never reverse between screens.
- Prompts update after remapping; no hard-coded key names remain in migrated
  UI.
- Remapping cannot strand the player; reset-to-defaults always reachable.
- Malformed settings fall back safely; controller disconnect recovers without
  locking the current screen.
- No production state reads raw input.

## L. Risks and failure modes

- **Text-entry regression** — rerouting name entry can break the modal editor;
  keep `ui::TextInput` pure and covered before touching the state.
- **Settings-schema churn** — freezing a bad shape forces migrations; get
  owner sign-off on version 1 first.
- **Prompt-label sprawl** — dozens of call sites; migrate alongside M12's
  screen families to avoid double-touching states.
- **Conflict-policy deadlock** — a poorly designed swap/block flow can strand
  the player; the cannot-strand invariants must be tested, not assumed.
- **Dead options** — options for systems that do not exist yet confuse
  testers; ship only what is honored (or get explicit owner preference).
- **Determinism** — none of this may leak into battle/dungeon logic; input
  stays presentation-side.

## M. Required documentation updates on completion

- `docs/milestones.md` — status transitions.
- This note — actual implementation and deviations.
- `docs/control_standard.md` — from provisional to implemented (final actions,
  navigation rules, disconnect behavior).
- `docs/technical_design.md` — input/settings architecture.
- `README.md` — controls table marked as defaults; remapping documented.
- `docs/manual_test_matrix.md` — input/settings rows.
- `.claude/skills/crystal-dungeons/SKILL.md` — input-layer gotchas.
- Completion report per `docs/milestone_completion_template.md`.

## N. As-implemented record (2026-07-19)

Base commit `a247b09`. All eight slices delivered; 173/173 tests pass
(29 new across `test_input_frame/remap/prompt_labels/settings`).

**Core (`src/input/`):** `InputAction` reworked (dead `Pause` removed —
CTRL-021; `TextBackspace` added, Backspace + gamepad X; stable
serialization/display names); `InputQuery` extended (axes, text codepoints,
pressed-key queue); the `Input` facade now computes a per-frame model —
unified down states with left-stick hysteresis 0.5/0.35 (CTRL-006 fixed for
real), own edges, `navPressed` hold-repeat (0.35s/90ms), held-input
suppression on stack-top changes, active-device tracking. New pure modules:
`PromptLabels` (binding-derived "[Tab] Pause" labels — CTRL-007) and `Remap`
(swap-with-donor conflicts, Esc reserved, never-unbound invariants).

**Settings (`src/settings/`):** versioned `settings.json` v1 (volumes /
borderless / battle+message speed / both devices' bindings) with defensive
load and strand-protection; `AudioManager::setVolumes`; applied at startup
and live from the Settings screen; battle speed drives the resolve pause,
message speed scales transient messages (no dead options shipped).

**States:** name editing through the input layer (Blocker UI-INPUT-001
fixed; gamepad exits/deletes, typing labeled keyboard-only); new
`SettingsState` (title + both pause menus, so settings are reachable before
New Game) and `RemapState` per device with listen-flow, conflict messages,
reset; Controls page generated from the live `InputMap`; every footer/hint
binding-derived; menu navigation uses `navPressed`; battle Left/Right
aliases removed (CTRL-022).

**Runtime-verified** (driven session, captures in
`docs/screenshots/m13_after/`): Settings screen with disabled gamepad row;
remap listen overlay; J-bind + conflict-free rebind; reset; settings.json
round-trip including reset-to-defaults. **Defect found and fixed during
verification:** a just-bound key that was still physically held immediately
fired as its new action (observed: binding J to Move Up moved the cursor);
fixed by suppress-until-release after every remap.

**Deviations / owner-attention items:**
1. `settings.json` v1 shipped under the blanket M13 authorization; the shape
   follows this note's spec but the owner has not explicitly signed the
   schema — review it before it hardens (it is trivially replaceable until a
   release ships).
2. No on-screen keyboard for gamepad typing (conservative recommendation
   adopted); revisit by M22.
3. Shake/flash/high-contrast options deferred to M18/M22 per the note (no
   dead options).
4. `Quit` action remains reserved/unbound.
5. Controller disconnect does not auto-pause gameplay (pad inputs release
   cleanly, keyboard continues); flagged as an owner decision.
