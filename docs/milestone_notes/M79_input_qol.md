# M79 — Input & QoL: party cycling, three-slot remapping, defaults

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Re-audit this note against the
then-current checkout before implementation begins.

## A. Status

☐ planned

## B. Goal (owner brief)

JRPG-standard shoulder-button character cycling in the Equip Shop and
Training Hall, a keyboard remap system with two directly-mappable
alternative keys per action and in-use warnings, and small default
adjustments (volumes, the Decisive achievement).

## C. Scope (planned)

### Party cycling

- Two new `InputAction`s — `CyclePrev` / `CycleNext` — defaults:
  keyboard **Q / E** (fits WASD), alternates **Ctrl / Alt** (fits arrows),
  gamepad **left/right shoulder buttons** (all verified unused today).
- Wired wherever a selected member's portrait is shown: the Equip Shop's
  member-scoped phases (EquipChar/EquipSlot/EquipItem) and the Training
  Hall's phases — cycling moves to the prev/next living party member,
  keeping the current phase, refreshing portrait/lists, with footer hints
  from the active bindings.
- Bonus reuse: the M82 scoreboard cycles its 1F/4F boards with the same
  actions.

### Keyboard remap overhaul (`src/input/`, RemapState)

- Every remappable action exposes **Primary / Alternative 1 /
  Alternative 2** keyboard slots, each *directly* remappable (today a
  remap replaces the whole binding and silently swap-donates on conflict).
- Assigning a key already bound anywhere raises a **warning** with an
  explicit confirm; confirming steals the key from its old slot; an action
  can never be left with zero bindings (the remap-cannot-strand rule from
  M13 stands).
- New defaults: **Confirm alt2 = Z**, **Cancel alt2 = X** (both keys free).
- Settings persistence stays version-1 defensive: existing binding lists
  load as-is; absent slots fall back to defaults.
- Prompts/help remain binding-derived everywhere (the M13 contract).

### Defaults & achievements

- `Settings.musicVolume` default **1.0 → 0.7** (shown 7/10);
  `Settings.ambienceVolume` default **0.5 → 0.3** (shown 3/10). New-file
  defaults only — existing settings.json values are the player's and stay.
- `decisive` achievement: **≤20 → ≤15 turns** (condition + description).

## D. Schema, save & version implications

- No rules/generation/save version motion; settings schema stays v1
  (binding lists already serialize as arrays; slot semantics are code).

## E. Out of scope

New gameplay actions beyond cycling; gamepad remap redesign (the existing
flow stays); the scoreboard cycling *screen* itself (M82).

## F. Dependencies

None (independent of M75–M78).

## G. Acceptance criteria

- Q/E, Ctrl/Alt and shoulder buttons cycle members in both screens with
  correct portraits and no phase loss; hints match bindings.
- Each of the three keyboard slots remaps directly; conflicts warn and
  resolve without ever stranding an action; Z confirms and X cancels on a
  fresh profile.
- Fresh-profile volumes are 7/10 music, 3/10 ambience; Decisive fires at 15.

## H. Automated validation

Binding resolution with three slots (serialization round-trip, conflict
detection/steal, no-unbound invariant); Decisive threshold test; defaults
test. Full suite green; capture of the remap screen.

## I. Owner manual validation

A keyboard-only and a gamepad-only session across both shops; remap all
three slots of two actions incl. a deliberate conflict; confirm the warning
reads well; check fresh-profile volume feel.
