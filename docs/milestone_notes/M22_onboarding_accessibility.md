# M22 — Onboarding and Accessibility

## A. Status and authority

- **Status:** planned
- **Last reviewed repository commit:**
  `a316f244e870718aa27d9995dc871e11572ad429` (2026-07-19).
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
