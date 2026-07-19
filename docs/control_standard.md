# Control Standard — Baseline Contract (M11)

> Status: **provisional baseline** from the M11 audit at commit `8271871`.
> Documents the input system as it exists and the semantics M13 must
> implement. M13 updates this document to "implemented".

## 1. Semantic actions today

`InputAction` (gameplay code never reads raw keys — one violation, below):

| Action | Keyboard default | Gamepad default | Used by |
|---|---|---|---|
| MoveUp/Down/Left/Right | Arrows + WASD | D-pad buttons | movement, menu nav, value adjust |
| Confirm | Enter, Space | A (right-face-down) | select, interact, advance |
| Cancel | Esc, Backspace | B (right-face-right) | back, close, (opens pause in town/dungeon) |
| Menu | Tab | Start | opens pause in town/dungeon |
| Pause | P | Select/Back | **bound but consumed nowhere** (CTRL-021) |
| ToggleDebug | F1 | — | debug overlay |

**No analog axis support exists** — `InputQuery` has button/key callbacks
only. Help and README advertise "Left Stick" (defect CTRL-006).

**Raw-input exceptions (production):** exactly one —
`PartyCreationState` name editing (`GetCharPressed`,
`IsKeyPressed(BACKSPACE/ENTER/ESCAPE)`), which soft-locks gamepad-only play
(Blocker UI-INPUT-001). M13-1 removes it by routing text entry through the
input layer.

## 2. Semantics that must hold everywhere (M13 acceptance)

- **Confirm** activates/advances; **Cancel** backs out/closes. Never reversed
  between screens. Cancel from a root screen may open the pause menu (town/
  dungeon convention) but must never be destructive by itself.
- One **Menu/Pause** action with one consistent prompt. Resolve the dead
  `Pause` action (merge into Menu or delete) — owner call in M13-2.
- Value adjustment (Guild theme/depth, class cycling) uses Left/Right while
  the row is focused; the prompt must say so.
- A state transition must consume buffered Confirm/Cancel: entering a screen
  with a held button must not activate anything (roadmap rule; no suppression
  exists today — audit found no observed double-fire, but nothing prevents
  it).
- Every screen reachable and exitable with keyboard alone and gamepad alone.
  Known violations: UI-INPUT-001 (blocker); everything else currently passes
  by code inspection.

## 3. Navigation rules

- **Lists:** Up/Down moves, wraps at ends (current `Menu` behavior — keep);
  disabled rows are skipped by the cursor but visible; selection must remain
  inside the visible viewport once scrolling exists (M12-b).
- **Multi-column/battle-field selection:** target selection cycles the legal
  target set (current behavior); the targeted unit must be visibly marked.
- **Battle lists today map Left/Right to Up/Down** (CTRL-022) — M13 decides:
  either keep as an alias everywhere or reserve Left/Right for
  columns/adjust. Recommendation: reserve; use Up/Down only in vertical
  lists.
- **Repeat:** holding a direction must auto-repeat after an initial delay
  (target ≈ 350ms delay, ≈ 90ms interval — tune in M13-3). No repeat exists
  today; long lists require hammering the key.
- **Deadzone/hysteresis (once axes exist):** enter ≈ 0.5, release ≈ 0.35;
  stick and D-pad must be equivalent everywhere.

## 4. Prompts

- Today: hard-coded strings, some naming abstract actions ("Menu: Pause" —
  CTRL-007). After M13-5, all prompts derive from active bindings for the
  most recently used device; hard-coded key names are prohibited.
- Grammar decision for M13: `<KeyLabel>: <Verb>` (e.g. "Tab: Pause",
  "A: Confirm") in footers; Help renders the full live binding table.
- The Help screen must be generated from `InputMap`, not a static table.

## 5. Device handling

- Active device = last device with an *intentional* input (threshold so
  drift can't flip prompts) — M13-4.
- **Controller disconnect target:** the game continues with keyboard;
  gameplay pauses if disconnect happens outside a menu (owner to confirm in
  M13); no screen may become unusable. Today disconnect is simply "gamepad
  unavailable" in the query — likely safe but unverified (manual matrix
  case).
- Mouse: optional convenience only; never required. (No mouse support exists
  today; adding any is out of scope until the owner asks.)

## 6. Remapping requirements (M13-7)

- Per-action, per-device rebind with a listen-for-input flow; cancellable.
- Conflict policy: swap or block-with-message; an action can never end up
  unbound; navigate/Confirm/Cancel can never all be unbound; reset-to-default
  always reachable from both devices.
- Settings persist in a versioned `settings.json` (M13-6); malformed files
  fall back to defaults.
- Text entry (names, future seed entry — DATA-023) must work through the
  input layer with a gamepad-usable path (on-screen keyboard or explicit
  keyboard-only labeling — owner decision in M13).

## 7. Unresolved actions (decide in M13-2)

- `Details` — needed by UI-INFO-004/013 (descriptions, gear stats) and M22.
- `PageLeft`/`PageRight` — shoulder buttons for paged lists (scoreboard,
  shops) if M12 chooses pagination anywhere.
- Explicit `Quit` — currently only via title menu; decide whether a global
  quit affordance is wanted.
- Fate of the dead `Pause` action (CTRL-021).

## 8. What M11 changes

Nothing — this is a contract document. Implementation is M13 (plus the M12
list-viewport prerequisites).
