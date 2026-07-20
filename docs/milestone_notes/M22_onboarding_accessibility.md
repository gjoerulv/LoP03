# M22 — Onboarding and Accessibility

## A. Status and authority

- **Status:** complete (approved) — approved by the owner on 2026-07-20
  after manual testing (see §N).
- **Last reviewed repository commit:** M21 approval HEAD (2026-07-20).
  Re-audit: several §C items already exist from M13/M18 — Settings and
  Controls are on the title menu (reachable before New Game),
  `MessageSpeed` (Slow/Normal/Fast) with `messageDurationScale` is
  persisted and honored via `scaledMessageTime`, battle flash/shake levels
  exist and are honored, danger/status/rarity/selection already carry
  text/glyphs (not color alone), and M21 removed sound-only information by
  construction. Real gaps: no tutorial system of any kind; no Details
  action (`InputAction` has 11 actions, 7 remappable; keyboard C and
  gamepad Y are unbound); `ui::style` colors are compile-time constants
  (no theme variant); save-overwrite and quit-to-title (which drops
  progress since the last save) have no confirmation.
- **Owner decisions (2026-07-20):**
  1. **Contextual one-time prompts** — ~9 beats, small dismissible overlay
     panels on first encounter; never re-shown; disable-all + reset in
     Settings; no forced sequence.
  2. **Own `tutorial.json`** — versioned `{version, enabled, seen[]}`
     beside settings.json in the user data dir; defensive parse; reset =
     rewrite. Settings schema untouched by tutorial state.
  3. **Both additive changes approved:** remappable `Details` action
     (keyboard C, gamepad Y; additive input schema, no version bump)
     opening contextual help panels; high contrast as a switchable UI
     palette behind a `style::palette()` accessor (single writer: the
     settings-apply path; default palette byte-identical to today).
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M22 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Planning fidelity:** bounded specification. Tutorial beats and
  accessibility gaps must be derived from the game as it exists after M12–M21;
  refresh this note against the then-current repository before implementation
  begins.

## B. Problem statement

The game teaches nothing in play: onboarding is a static Help/controls page
plus the README. A new player must infer danger tiers, gates, turn-efficiency
scoring, retreat consequences, equipment, and the Training Hall unaided.
Accessibility work so far is structural (M12 contrast/focus conventions, M13
option stubs, M18 effect intensity settings) but incomplete and unproven
against real barriers. The roadmap requires teaching through play and
removing avoidable barriers without removing tactical challenge.

## C. Goals

- Progressive contextual onboarding covering, in play: town movement and
  interaction; party preparation and equipment; dungeon danger and gates;
  battle command/target selection; turn-efficiency scoring; retreat/failure
  consequences; XP, shops, and deeper runs.
- Tutorial prompts can be disabled, revisited, and reset; tutorial state
  persists defensively (malformed state never crashes or re-locks progress).
- Contextual **Details** help for stats, statuses, danger tiers, score
  components, and equipment comparisons (using the M13 Details action).
- Accessibility options completed and honored: high-contrast UI; reduced
  motion/flash/shake; configurable text/message pacing; no essential
  color-only or sound-only information; destructive-action confirmation and
  clear error recovery.
- Settings reachable before starting a game.

## D. Non-goals

- No difficulty redesign or balance changes (M19/M23 own those).
- No new mechanics; onboarding teaches what exists.
- No forced tutorial that blocks experienced players (skippable by design).
- No formal WCAG/platform certification claims — the documented targets are
  engineering bars.

## E. Dependencies

- M13 settings/actions (Details, pacing, toggles) and M12 layout (help
  panels).
- M18 effect-intensity settings (reduced motion/flash/shake become fully
  honored here).
- M21 audio (no-sound-only rule verified against final audio).
- **Owner approval required for:** the tutorial flow (player-experience
  decision) and the tutorial-state persistence location/shape (new persisted
  data).

## F. Proposed slices (provisional — refine before starting)

1. **Tutorial framework** — trigger/prompt system with persistent, resettable
   state; disable/revisit controls.
2. **Onboarding content** — the progressive beats above, written and placed;
   owner review of tone/pacing.
3. **Details help** — contextual panels for stats/status/danger/score/
   equipment.
4. **Accessibility completion** — high-contrast theme, motion/flash/shake and
   pacing options honored everywhere, color/audio-alternative audit,
   destructive-action confirmations.
5. **Fresh-tester validation** — an uncoached new player attempts a depth-1
   run.

## G. Expected systems affected

- New tutorial module + persisted tutorial state (location decided with the
  owner; likely beside settings in the user data dir).
- `src/states/` — prompt/Details surfaces across town/dungeon/battle.
- `src/ui/` — high-contrast theme variant; pacing-aware text presentation.
- `src/input/`/settings — new option fields (reviewed settings-schema
  revision).
- `tests/` — tutorial-state persistence/reset, option-honoring logic.

## H. Data, schema, and save compatibility

- **New persisted tutorial state** (owner-gated shape; versioned, defensive,
  resettable). Settings schema gains accessibility fields (reviewed
  revision).
- Saves, content schemas, seeds, scoring: **no impact.**

## I. Automated validation

- Tutorial-state round-trip, malformed-state fallback, and reset tests.
- Option-honoring tests at the policy level (e.g. pacing values applied,
  high-contrast role table selected).
- Content lint: every tutorial/Details string passes the M12 overflow rules.
- Full suite green.

## J. Manual validation for the owner

- Observe an uncoached new tester: does a depth-1 run complete without
  external instruction? Where do they stall?
- Play with each accessibility option: high contrast, reduced motion/flash/
  shake, slow and fast message pacing.
- Verify settings are reachable before New Game; tutorial disable/revisit/
  reset all work.
- Spot-check color-only/sound-only audits (danger, status, selection,
  telegraphs, rarity).
- Attempt destructive actions (overwrite save, quit mid-run): confirmations
  are clear and recoverable.

## K. Acceptance criteria

- A new tester completes a depth-1 run without external instruction.
- Every critical mechanic is explained in-game (tutorial or Details).
- Keyboard-only and gamepad-only operation remain complete.
- Critical text meets the documented contrast target; accessibility cases
  pass the manual matrix.
- Tutorial state persists defensively and can be reset.
- Settings are reachable before starting a game.

## L. Risks and failure modes

- **Tutorial fatigue** — over-prompting punishes the roguelite's repeated
  runs; prompts must be once-per-concept and dismissible.
- **Stale tutorials** — text drifting from later balance/UI changes; M23/M24
  matrices must include tutorial screens.
- **Option combinatorics** — high-contrast × reduced-motion × pacing
  combinations multiply the test surface; the manual matrix must name the
  covered combinations honestly.
- **Accessibility theater** — options that exist but are not honored
  everywhere; the audit slices are the control.
- **Second-order:** onboarding changes perceived difficulty and pacing —
  fresh-tester evidence feeds M23 balance work.

## M. Required documentation updates on completion

- `docs/milestones.md`; this note (as-implemented + deviations).
- `docs/game_design.md` — onboarding flow and accessibility commitments.
- `docs/control_standard.md` — Details/help integration.
- `docs/manual_test_matrix.md` — accessibility and tutorial rows.
- README — player-facing accessibility/options summary.
- Completion report per `docs/milestone_completion_template.md`.

## N. As-implemented record (2026-07-20)

**Tutorial framework (owner decisions 1–2).** `src/tutorial/Tutorial.*`
(raylib-free): a constexpr table of **9 beats** with stable ids — town
welcome, guild preparation/scoring, first dungeon room, first battle,
first guarded chest, first event, first non-boss victory, first result
screen, first return to town (fires only after the result beat, so it
lands post-run) — and `TutorialStore` over a versioned `tutorial.json`
beside settings.json. `takeBeat` marks-and-saves on first fire (at most
one showing, even across crashes); malformed/foreign-version files start
fresh with a report, never a crash; unknown seen ids survive round trips
(downgrade-safe). `TutorialPromptState` is a transparent overlay: the
scene freezes and dims, the panel height is measured from the wrapped
text, one Confirm dismisses. Settings gained "Tutorial Prompts: On/Off"
and "Reset tutorial prompts". Beats trigger from onEnter/onResume/update
only — a constructor trigger would queue the overlay underneath the state
being created.

**Details help (owner decision 3a).** `InputAction::Details` (keyboard C,
gamepad Y, remappable — appears in both Remap screens automatically;
additive bindings schema, old settings files keep the default) opens
`DetailsOverlayState` panels: battle (focused unit's stats/statuses +
status-shorthand and turn-order/Guard/Escape reference; targets the
highlighted unit while aiming), dungeon (danger-tier derivation + the
faced team's name/tier/count), result + scoreboard (one shared
score-components text in `ScoreDetailsText.hpp`, so the surfaces cannot
drift), and equip-shop Buy (item stats + per-member deltas vs currently
equipped gear). Footers advertise Details wherever it works.

**High contrast (owner decision 3b).** `style::palette()` +
`setHighContrast` in `src/ui/UiStyle.*`: the standard table is
byte-identical to the legacy constants (test-enforced); the high-contrast
table uses pure-white text and brightened dim/hint/disabled/accent roles.
63 color-constant use sites across 7 state files migrated mechanically.
`settings.highContrast` is optional (absent = off, pre-M22 files load
unchanged); the Settings row applies instantly. The single mutable table
pointer is documented in `ui_style_guide.md` §4 as the deliberate
exception (one writer: the settings-apply path).

**Confirmations & already-satisfied items.** Save-overwrite and
quit-to-title arm on first Confirm (visible warning; cursor move/Cancel
disarms) and execute on the second. The re-audit found M13/M18 had
already delivered: Settings/Controls before New Game, MessageSpeed
pacing, flash/shake levels, and the no-color-only/no-sound-only rules —
verified rather than rebuilt.

**Evidence.** 244/244 tests (9 new across `test_tutorial.cpp` and
`test_accessibility.cpp`): progress round-trip, malformed fallback,
non-string seen entries, take-once store semantics on a real temp file,
beat-table uniqueness + wrap lint (conservative 7px/char measure against
the prompt panel width), palette equivalence and switching, highContrast
settings round-trip incl. wrong-type reporting, Details action schema +
default bindings on both devices. Smoke launch clean.

**Deviations / notes:**
1. The "Guide"-style long-form reference stayed out of scope: Details
   panels are per-context, and the existing Help screen still covers
   controls. If the fresh-tester observation (§J) shows gaps, M23 can add
   pages cheaply through the same overlay.
2. The M12 UI-DESIGN wish for equipment comparison shipped here as the
   equip-shop Details panel (per-member deltas), not as an inline list
   column.
3. Message pacing and reduced motion were **verified** as delivered by
   M13/M18 rather than reimplemented; the acceptance criterion "options
   honored everywhere" rides on those existing paths plus the new
   palette.
4. The Settings menu grew to 15 rows: spacing tightened from 14px to
   12px starting at y=30 to stay inside 240px with the message line and
   footer intact.
