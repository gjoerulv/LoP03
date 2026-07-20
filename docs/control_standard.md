# Control Standard — Implemented (M13)

> Status: **implemented** by M13 (input frame model in `src/input/Input.*`,
> labels in `src/input/PromptLabels.*`, remapping in `src/input/Remap.*`,
> persistence in `src/settings/`). This documents the live semantics; update
> it whenever they change.

## 1. Semantic actions

`InputAction` (gameplay code never reads raw keys; zero production
exceptions since M13 — text entry flows through the input layer):

| Action | Keyboard default | Gamepad default | Used by |
|---|---|---|---|
| MoveUp/Down/Right/Left | Arrows + WASD | D-pad **and left stick** (hysteresis 0.5 enter / 0.35 release) | movement, menu nav, value adjust |
| Confirm | Enter, Space | A | select, interact, advance |
| Cancel | Esc, Backspace | B | back, close, (opens pause in town/dungeon) |
| Menu | Tab | Start | opens pause in town/dungeon |
| Details | C | Y | M22: contextual help panels (battle stats/statuses, dungeon danger, score components, gear comparison) |
| TextBackspace | Backspace (fixed) | X (fixed) | delete-one-char in text editing |
| ToggleDebug | F1 (fixed) | — | debug overlay |
| Quit | — | — | reserved, unbound |

The former `Pause` action (P / Select) was **removed** — it was bound but
consumed nowhere (CTRL-021). Text editing resolves the Backspace overlap by
checking TextBackspace before Confirm/Cancel, so Backspace deletes while Esc/
Enter/A/B finish editing — the gamepad soft-lock UI-INPUT-001 is gone; typed
characters still require a keyboard (labeled in-game).

## 2. Semantics that must hold everywhere (M13 acceptance)

- **Confirm** activates/advances; **Cancel** backs out/closes. Never reversed
  between screens. Cancel from a root screen may open the pause menu (town/
  dungeon convention) but must never be destructive by itself.
- One **Menu/Pause** action with one consistent prompt (the dead `Pause`
  action was deleted in M13).
- Value adjustment (Guild theme/depth, class cycling, Settings) uses
  Left/Right while the row is focused; the footer prompt says so.
- State transitions and remap listens suppress everything held until release
  (see §3) — a buffered press can never activate the next screen.
- Every screen reachable and exitable with keyboard alone and gamepad alone
  (UI-INPUT-001 fixed in M13; owner's manual pass confirms in practice).
- **Details** (M22) opens a read-only overlay wherever the footer offers it;
  Confirm, Cancel, or Details again closes it. It is remappable and never
  required to progress — everything it explains is also learnable by play.
- **Destructive actions need a second Confirm** (M22): overwriting an
  existing save slot and quitting to title from the pause menu both arm on
  the first Confirm (with an explicit warning) and execute on the second;
  moving the cursor or Cancel disarms. One-time tutorial prompts freeze the
  scene below and dismiss with a single Confirm/Cancel.

## 3. Navigation rules

- **Lists:** Up/Down moves, wraps at ends (current `Menu` behavior — keep);
  disabled rows are skipped by the cursor but visible; selection must remain
  inside the visible viewport once scrolling exists (M12-b).
- **Multi-column/battle-field selection:** target selection cycles the legal
  target set (current behavior); the targeted unit must be visibly marked.
- **Left/Right are reserved** for value adjust (Guild, Settings) and future
  columns; vertical lists use Up/Down only (the old battle alias was removed
  — CTRL-022 resolved).
- **Repeat:** holding a direction auto-repeats after 0.35s at 90ms intervals
  (`Input::navPressed`; constants in `Input.hpp`). Menus use `navPressed`;
  Confirm/Cancel never repeat.
- **Deadzone/hysteresis:** left stick enters a direction at 0.5 and releases
  below 0.35; stick and D-pad are equivalent in every UI.
- **Transition suppression:** whenever the state stack's top changes — and
  after every remap listen — everything currently held is suppressed until
  released (`Input::suppressUntilRelease`), so a buffered press can never
  activate the next screen or fire as a freshly bound action.

## 4. Prompts

- All prompts derive from the active bindings for the most recently used
  device (`input::prompt` / `primaryLabel` / `allLabels`); hard-coded key
  names are prohibited.
- Grammar: `[KeyLabel] Verb` (e.g. "[Tab] Pause", "[A] Confirm") in footers;
  the Help/Controls screen is generated from the live `InputMap` (all
  bindings per action, both devices).

## 5. Device handling

- Active device = last device with an *intentional* input (threshold so
  drift can't flip prompts) — M13-4.
- **Controller disconnect:** all pad-held actions release cleanly (tested)
  and the keyboard keeps working; prompts flip to keyboard labels on the
  next keyboard press. The game does not auto-pause on disconnect —
  flagged as an owner decision if wanted later. Manual matrix row 34 covers
  the live check.
- Mouse: optional convenience only; never required. (No mouse support exists
  today; adding any is out of scope until the owner asks.)

## 6. Remapping (implemented)

- Settings → Remap Keyboard / Remap Gamepad: per-action listen-for-input
  rebind; `[Esc]` always cancels listening and can never be bound (reserved).
- Conflict policy: **swap** — the input's previous owner takes the action's
  old primary binding; a swap with nothing to donate is **blocked** (map
  unchanged). Invariants (tested in `test_remap.cpp`): only the 7 remappable
  actions participate; no remappable action ever ends up unbound;
  TextBackspace/ToggleDebug are fixed.
- Reset to defaults exists in both the remap screen and Settings; every
  successful change saves immediately.
- Persistence: versioned `settings.json` (v1) in the user data dir —
  volumes, window mode, battle/message speed, and both devices' bindings.
  Malformed/foreign-version files fall back to defaults with a logged
  report; a binding set that would strand the keyboard restores that
  action's defaults (tested in `test_settings.cpp`).
- Text entry works through the input layer; gamepad can delete/finish but
  typing needs a keyboard (labeled in-game). An on-screen keyboard is a
  future owner call (revisit by M22).

## 7. Still-unresolved actions

- `Details` — will be added when M22's contextual help needs it.
- `PageLeft`/`PageRight` — only if a screen adopts pagination.
- Explicit `Quit` — reserved, still unbound; global quit affordance remains
  an open owner decision.
- Seed text entry at the Guild (DATA-023) — can now reuse the text-entry
  path; owner decision on wanting it.
