# M79 — Input & QoL: party cycling, three-slot remapping, defaults

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Implemented 2026-08-06 on the
post-M78 checkout (`97c74d2`).

## A. Status

**◑ implemented, awaiting manual approval** — implemented 2026-08-06.
Evidence in §F.

## B. Goal (owner brief)

JRPG-standard shoulder-button character cycling in the Equip Shop and
Training Hall, a keyboard remap system with two directly-mappable
alternative keys per action and in-use warnings, and small default
adjustments (volumes, the Decisive achievement).

## C. As implemented

No rules/generation/save version motion; settings schema stays v1.

### Party cycling

- Two new remappable `InputAction`s — **CyclePrev / CycleNext** — defaults
  **Q / E**, alternates **Ctrl / Alt** (which now have real prompt names
  instead of "Key#341"), gamepad **LB / RB**. Serialization names
  `cycle_prev` / `cycle_next`; a pre-M79 settings file never mentions them,
  so the defensive loader leaves the defaults in place.
- **Equip Shop**: in the member list the pair walks the cursor; in the
  slot and item phases it switches the outfitted member IN PLACE — phase,
  chosen slot and scroll position kept, portrait and lists refreshed. The
  footer advertises "Q/E Member" (binding-derived) through all three
  member-scoped phases.
- **Training Hall**: the same — cursor in the member list, in-place member
  switch through the char menu and passive screens (costs, level and
  ownership marks all refresh).
- The M82 scoreboard will reuse the same pair for its 1F/4F boards.

### The three-slot keyboard remap

- `input::assignKeySlot` (pure, beside the M13 engine): every remappable
  action exposes **Primary / Alt 1 / Alt 2**, each directly addressable.
  Assigning to an occupied slot replaces exactly that slot; an empty slot
  appends; **slots pack left** (no gaps — see §E). Moving a key between an
  action's own slots needs no ceremony.
- **The in-use warning**: a key bound anywhere else returns
  `NeedsConfirm` and changes nothing; the remap screen raises a modal —
  *"T already does Move Up (Alt 1)."* — with explicit Steal/Keep. A
  confirmed steal that would leave the old owner with zero keyboard keys
  is **Blocked** with the reason (the M13 never-unbound rule).
- **RemapState (keyboard)**: rows are actions, the three slots render as
  live columns with headers; **Left/Right** picks the slot, Confirm
  listens. Rows tightened to 14px so the grown list (10 actions) stays
  inside the frame. The **gamepad flow is untouched** (M13
  replace-and-swap), per the note's out-of-scope line.
- **Owner review fixes (2026-08-06, same day):** the transient banner now
  rides the TOP of the screen and **times out** (message-speed scaled), so
  it can never sit on the Back row — and a plain no-conflict rebind shows
  **no banner at all** (the slot cell updating is the feedback; steals,
  blocks and resets still announce themselves). The addressed slot is
  **highlighted text with an underline** instead of a rectangle, and the
  columns are computed from the 426-wide frame, so nothing overlaps the
  cursor slab or spills past the border. The **gamepad list uses the same
  aligned columns** (label column + bindings column, no " - " run-ons).
  The **numpad and navigation cluster got real key names** — "Num 8",
  "Num Enter", "PrtScr", "Home", "PgUp"… — where the fallback used to
  print "Key#328".
- New defaults: **Confirm alt2 = Z, Cancel alt2 = X**.
- Persistence: the same version-1 arrays (a slot is just a position);
  every successful change saves immediately, exactly as before.

### Defaults & achievements

- Fresh profiles: **music 0.7 (7/10)**, **ambience 0.3 (3/10)**; the
  absent-key parse fallbacks track the same numbers, and any written value
  is the player's and wins.
- **Decisive: ≤20 → ≤15 turns**, condition and description.

## D. Files changed

- **Input:** `src/input/InputAction.hpp` (two actions, count 12→14,
  remappable 8→10, names), `src/input/InputMap.cpp` (defaults),
  `src/input/Remap.hpp/.cpp` (`assignKeySlot`, `kKeySlotCount`,
  `SlotOutcome/SlotResult`), `src/input/PromptLabels.cpp` (modifier key
  names).
- **States:** `src/states/RemapState.hpp/.cpp` (slot columns, steal
  modal), `src/states/EquipShopState.cpp` and
  `src/states/TrainingHallState.cpp` (cycling + footer hints).
- **Settings:** `src/settings/Settings.hpp/.cpp` (volume defaults).
- **Achievements:** `src/game/Achievements.hpp/.cpp` (Decisive 15).
- **Tests:** `tests/test_input_qol.cpp` (new, 10 cases, `[qol]`),
  `tests/CMakeLists.txt`, honest pin updates in `tests/test_comforts.cpp`
  (ambience default 0.5→0.3) and `tests/test_prompt_labels.cpp` (Confirm
  now reads "Enter or Space or Z").
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md`,
  `docs/technical_design.md` §33, `docs/manual_test_matrix.md`.

## E. Plan deviations

- **Slots pack left (no gaps).** The settings arrays have carried plain
  key lists since M13; a gapped slot would need a sentinel in the public
  schema. Assigning "Alt 2" while "Alt 1" is empty therefore lands the key
  in the next free position (the UI shows it there immediately). Cosmetic
  only — which keys work is exactly what the player chose.
- **Cycling walks ALL party members, not only the living** — the member
  lists in both screens include the fallen (equipping or training a KO'd
  member is legal), so the cycling pair matches what Up/Down can reach.
- The keyboard flow no longer uses the M13 `remapKey` (replace-and-swap);
  it remains the gamepad engine and keeps its tests.

## F. Automated validation (all run in this session, 2026-08-06)

- Debug build: clean, zero project-code warnings.
- New `[qol]` cases (11) green: the M79 defaults (Q/E/Ctrl/Alt, LB/RB,
  Z/X, prompt names), the numpad/navigation key names, slot
  replace/append/pack, the own-slot move, the warn-then-steal flow with
  the untouched-map guarantee, the stranding-steal block,
  Esc/non-remappable/out-of-range blocks, the three-slot settings
  round-trip, the pre-M79-file defaults, the volume defaults (and
  player-value precedence), and Decisive at 15/16.
- Honest pin updates: ambience absent-default 0.5→0.3; Confirm's label
  list gains "or Z".
- **Full Debug suite: 672/672 tests green. Capture lint: 85/85 scenes
  clean (incl. the reworked `04_remap_keyboard`). Release build + suite:
  668/668 tests green.** (First pass verified at 671/667 before the
  owner's same-day review; the fix pass re-verified everything with the
  new key-name case added.)

## G. Manual owner checklist

1. **Keyboard session** (fresh profile so Z/X are on): Equip Shop → equip
   a member → Q/E and Ctrl/Alt cycle the member with the portrait, slots
   and lists following; the phase never resets. Training Hall likewise
   through the char menu and passives. Z confirms, X cancels everywhere.
2. **Gamepad session**: LB/RB do the same in both screens; the footer
   hints show LB/RB.
3. **Remap** (Settings → Controls → Keyboard): the three columns read
   correctly; Left/Right picks a slot; rebind Primary/Alt1/Alt2 of two
   actions. Deliberately assign a key that is in use — the warning names
   its current home; Steal moves it, Keep leaves everything. Try to steal
   Menu's lone Tab — the block explains itself. Reset restores defaults
   (incl. Z/X and Q/E). **Review-fix checks:** a plain rebind shows no
   banner (the cell just updates); a steal/block/reset banner appears at
   the TOP and fades on its own, never covering Back; the addressed slot
   is underlined text (no boxes colliding); the gamepad list's bindings
   sit in one aligned column; bind numpad keys — they read "Num 8",
   "Num Enter", not "Key#328".
4. **Fresh profile**: volumes read 7/10 music, 3/10 ambience; an existing
   profile keeps its values.
5. Clear a dungeon in ≤15 turns → Decisive fires; the achievement text
   says 15.
6. On any failure: the screen, the keys pressed, and settings.json.

## H. Known limitations

- Slot gaps are not representable (bindings pack left, §E).
- The gamepad remap keeps the one-button replace-and-swap flow by design.
- No unbind affordance: a slot's key leaves only by being replaced or
  stolen — an action can never be reduced below one key anyway.

## I. Final status

`implemented, awaiting manual approval`
