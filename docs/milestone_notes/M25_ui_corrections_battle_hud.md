# M25 — UI Corrections and Battle HUD

## A. Status and authority

- **Status:** planned
- **Last reviewed repository commit:** `aa5a8aa` + uncommitted documentation
  changes (2026-07-20). Every code reference below was read at that commit; it
  is still a plan, not an as-implemented record.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`.
- **Authorization:** the polish program (M25–M30) was authorized on 2026-07-20.
  **Beginning implementation of this milestone still requires the owner's
  explicit go-ahead.**

## B. Problem statement

Four independent readability defects reported by the owner. All are presentation
only: none of them touches the battle simulation, dungeon generation, scoring,
or save data.

1. **Title screen text overlaps.** The version stamp is drawn at `(4, h-12)` at
   font size 8; the content-summary line is drawn at `(6, h-16)` at font size
   10 (`src/states/MainMenuState.cpp`). Their rectangles intersect on both axes.
   The content line is also drawn unconditionally — it is developer diagnostic
   information that should not appear in a Release build at all.
2. **Battle name labels bury the sprites.** `BattleState.cpp` draws
   `c.name` above every combatant on both sides, every frame, unconditionally.
   With a full party and five enemies this is ten labels competing with the art
   the player is trying to read.
3. **MP has no numerals.** Party members get an MP *bar*, but the only numeric
   readout is HP (`%d/%d` of `shownHp`/`maxHp`). MP is the resource that gates
   skill use, so its exact value matters more than a bar conveys.
4. **Guild values are detached from their controls.** `GuildState::render`
   draws Theme at y=78, Depth at y=96, and Seed at y=114, while the menu whose
   rows adjust them is drawn at y=140. The player reads a value roughly 60px
   above the row they are changing.

## C. Goals

- Remove every unintended text overlap on the title screen, and keep developer
  diagnostics out of Release builds.
- Make battle sprites the primary visual element, with identifying information
  available on demand rather than permanently overlaid.
- Show MP with the same numeric clarity as HP.
- Put adjustable values next to the controls that adjust them.

## D. Non-goals

- Any change to targeting *rules*, enemy AI, or battle resolution — that is M28.
  This milestone changes what is drawn, never what is decided.
- New art or assets (M26/M27).
- Restyling screens beyond the four defects above.
- Changing the 426×240 virtual resolution.

## E. Dependencies

None. M25 can begin immediately.

**M28 depends on this milestone.** An enmity model is unreadable if the player
cannot inspect a target, so the target-info panel introduced here is a
prerequisite for the AI work, not a parallel nicety.

## F. Proposed slices (provisional — refine before starting)

1. **Title screen.** Give the version stamp and content line non-overlapping
   rectangles using measured bounds (`ui::measureText`), not adjusted constants.
   Gate the content line behind the existing `CRYSTAL_SHIPPING_BUILD` guard so
   Release omits it. Decide with the owner whether Release keeps the version
   stamp — recommendation: **yes**, players reporting bugs need a version.
2. **Battle target info.** Remove the unconditional name label. Add a target
   panel showing name plus the basic stats needed to judge a target, rendered
   only for the currently targeted unit. Must reuse the M12 fitted/wrapped text
   helpers so it participates in overflow diagnostics. Placement must not
   collide with the existing status-effect lines, which already differ by side
   (party statuses below, enemy statuses to the right).
3. **MP numerals.** Add current/max MP text for party members alongside the
   existing HP numerals, within the same layout budget.
4. **Guild inline values.** Compose Theme/Depth values into their menu labels on
   rebuild, following `SettingsState::volumeLabel`. Note that `ui::MenuItem`
   holds only `label` and `enabled` — there is no value field, and adding one is
   a wider UI change that should be escalated rather than assumed. Seed is not
   an adjustable row and can stay a separate readout.

## G. Expected systems affected

- `src/states/MainMenuState.cpp` — title layout, Release gating.
- `src/states/BattleState.cpp` — label removal, target panel, MP numerals.
- `src/states/GuildState.cpp` — label composition.
- `src/capture/CaptureRunner.cpp` — capture scenes likely need a targeting state
  so the new panel is covered by the overflow check.
- Possibly `src/ui/Menu.hpp` **only if** the owner approves a value field;
  default is to avoid it.

## H. Data, schema, and save compatibility

No changes. No content schema, save format, settings field, or scoreboard field
is touched. No generation-version or battle-rules-version implication.

## I. Automated validation

- Full suite green (current baseline: 252 tests).
- `--capture` run stays overflow-clean, extended to cover a targeting state.
- Presentation lint continues to pass.

## J. Manual validation for the owner

- Title screen: no overlap; version legible; content line present in Debug and
  absent in Release.
- Battle with a full party and five enemies: sprites readable; target info
  appears only for the targeted unit and is legible; status lines uncollided.
- MP numerals correct and readable, including at max and at zero.
- Guild: Theme/Depth values sit beside their rows and update as you adjust.

## K. Acceptance criteria

- No unintended text overlap in any capture scene at 426×240.
- Content diagnostics absent from a Release build.
- No permanent name labels above combatants; target information reachable.
- Current/max MP visible as numerals for every party member.
- Guild values adjacent to the controls that change them.
- No change to any battle outcome: existing battle and simulator tests pass
  **unmodified**.

## L. Risks and failure modes

- **Target panel crowding.** The battle screen is already dense. If the panel
  cannot fit without collision, escalate rather than shrinking text below the
  established minimum size.
- **Removing names loses information.** If targeting alone proves insufficient
  to identify enemies in play, the fallback is a name shown on the acting unit
  as well — an owner call after seeing it, not a preemptive hedge.
- **Scope creep into restyling.** Four specific defects. A screen that merely
  looks dated is not in scope.

## M. Required documentation updates on completion

- This note: as-implemented record, deviations.
- `docs/milestones.md`: status → `implemented, awaiting manual approval`.
- `docs/ui_style_guide.md`: target-panel and inline-value conventions.
- `docs/control_standard.md`: only if prompt or action semantics change.
- `docs/manual_test_matrix.md`: rows for the four defects.
