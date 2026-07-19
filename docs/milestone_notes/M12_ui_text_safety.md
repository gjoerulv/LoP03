# M12 — UI Layout and Text-Safety Foundation

## A. Status and authority

- **Status:** planned
- **Last reviewed repository commit:**
  `a316f244e870718aa27d9995dc871e11572ad429` (2026-07-19).
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M12 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- This note must be **re-audited and refreshed against the then-current
  repository before implementation begins** — in particular against the M11
  defect register and the approved M12-a recommendation, which may adjust the
  slice order below.

## B. Problem statement

At the reviewed commit, text placement is prototype-level:

- `src/ui/UiDraw.cpp` contains the only `MeasureText` call in the codebase
  (`drawTextCentered`); everything else positions text with fixed coordinates;
- states call raw `DrawText` at ~48 sites across `src/states/` with hand-tuned
  x/y values and no bounds;
- there is no wrapping, scrolling, pagination, clipping, or overflow
  diagnostic of any kind;
- long dynamic content (item/skill descriptions, enemy team names, save
  metadata, score breakdowns) can silently overflow panels or the 426×240
  viewport;
- `BattleState.cpp` (601 lines) and `DungeonState.cpp` (533 lines) carry the
  densest fixed-coordinate text/UI logic.

The CLAUDE.md completion targets require that every required text element fit
its region or use an explicit wrap/scroll/page/details mechanism, measured
with real font metrics — none of which the current helpers can guarantee.

## C. Goals

- A small, game-specific, pure (headless-testable) text measurement and layout
  foundation in `src/ui/`.
- Every migrated screen family renders required text with measured bounds and
  one documented overflow policy (wrap, scroll, paginate, authored short
  label, or Details-backed ellipsis).
- Overflow becomes detectable: a debug diagnostic reports any text rendered
  outside its assigned rectangle.
- Zero unintended overflow across the manual test matrix for migrated screens,
  including long/empty/maximum-data variants.
- The 426×240 baseline is either confirmed viable or a resolution change is
  escalated to the owner with evidence — never decided inside this milestone.

## D. Non-goals

- No final art direction, colors, fonts, frames, or motifs (M15).
- No control remapping or settings work (M13).
- No room-system changes (M16).
- No final content/audio.
- No browser-style general-purpose UI framework — build only the primitives
  this game needs.
- No virtual-resolution change; that is a separate owner decision requiring
  evidence from this milestone.

## E. Dependencies

- M11 `complete (approved)`: the defect register drives migration priorities;
  the initial `docs/ui_style_guide.md` provides the provisional spacing/role
  conventions this milestone makes real.
- Owner approval of the M12-a slice recommendation from M11.
- No new dependencies: layout math is plain C++; raylib supplies font metrics
  at the adapter boundary only.

## F. Proposed slices

### M12-a — Text measurement and wrapping

- Font-aware measurement behind a pure interface (e.g. a `TextMeasure`
  callback/adapter so wrapping logic is testable without raylib, mirroring the
  existing `InputQuery` injection pattern).
- Explicit line height and paragraph spacing.
- Bounded word wrapping with long-token handling (break oversized tokens
  rather than overflowing).
- Overflow diagnostics: a debug-only report/log when text exceeds its assigned
  rectangle (behind `CRYSTAL_ENABLE_DEBUG_OVERLAY` or equivalent).
- Migrate `HelpState` plus one representative modal as proof.
- Tests: wrapping against a fake measurer — normal text, empty text, single
  oversized token, exact-fit boundaries, multi-paragraph.

### M12-b — Layout primitives

- Rectangles/insets/alignment helpers extending `core/Geometry.hpp`.
- Vertical/horizontal stacks; padded panels (wrapping the existing
  `drawPanel` visual).
- List viewport with a scroll model (cursor keeps selection visible; scroll
  indicators when content exceeds the viewport).
- Safe-area and footer/control-hint reservation per the M11 style guide.
- Focus-visibility contract: selection can never scroll out of view or be
  drawn under other content.
- Tests: pure layout arithmetic; scroll-window behavior at boundaries (first/
  last item, single-page lists, oversized lists).

### M12-c — Typography roles and UI conventions

- Named text roles (e.g. heading, body, hint, value, danger) mapped to size/
  style in one place instead of per-call-site constants.
- Spacing scale from the M11 style guide.
- Focus/disabled/danger conventions that do not rely on color alone
  (marker + color, per CLAUDE.md).
- Contrast validation helper (pure luminance/contrast math) or a documented
  manual check.
- Tests: role lookup, contrast-ratio math.

### M12-d…f — Screen-family migrations (reviewable families, one at a time)

Suggested order (refine against the M11 defect priorities):

1. title/help and the main-menu shell;
2. party creation, save/load slots (long-name and empty-slot cases);
3. town services (Inn, Item Shop, Equip Shop, Training Hall, Guild,
   Scoreboard);
4. dungeon HUD, encounter/chest prompts, dungeon menu;
5. battle (commands, skill/item lists with longest descriptions, targets,
   status display) — coordinate with the M18 plan so migration here stays
   layout-only;
6. result/score breakdown and scoreboard.

Each family migration includes long/empty/maximum-data test variants and a
manual-matrix pass before moving on. Each slice keeps the game buildable and
playable.

## G. Expected systems affected

- `src/ui/` — extend `UiDraw`; add new pure layout/text modules (names to be
  fixed at implementation, e.g. text layout + layout primitives + theme
  roles).
- `src/core/Geometry.hpp` — possible small additions (insets/alignment).
- `src/states/*.cpp` — every migrated screen family; heaviest in
  `BattleState.cpp`, `DungeonState.cpp`, shop states, `ScoreboardState`.
- `tests/` — new pure test files for wrapping, layout, scroll, contrast.
- `docs/ui_style_guide.md` — updated from provisional to implemented
  conventions.

## H. Data, schema, and save compatibility

**No impact** on JSON content schemas, asset manifests, save files, settings
files, deterministic seeds, scoring, or public content identifiers. Rendering-
only changes.

## I. Automated validation

- New Catch2 tests for wrapping/layout/scroll/contrast (pure, headless).
- Long/empty/maximum-data content cases per migrated screen family.
- Full existing suite stays green (baseline 125 tests at the reviewed commit).
- Build in Debug and Release configurations.
- Overflow diagnostics produce zero reports across a scripted pass of migrated
  screens (manual observation until M23 automates capture).

## J. Manual validation for the owner

- For each migrated family: view the screens at native 426×240 scale, default
  1278×720, 1920×1080, a narrow window, and a near-square window.
- Check the maximum-content cases: longest skill/item descriptions, maximum
  name lengths, full save slots, five-enemy battles, status-heavy battles,
  full scoreboard.
- Confirm focus/selection is always visible and readable, with keyboard and
  gamepad.
- Confirm no required text is clipped, overlapped, or silently ellipsized.
- Report whether 426×240 readability feels acceptable — this feeds the
  separate resolution decision.

## K. Acceptance criteria

- Zero unintended text overflow in the manual matrix for migrated screens.
- Every variable-length list scrolls or paginates; selection stays visible.
- No required information is silently ellipsized (Details/authored-short-label
  paths exist where truncation is intentional).
- Disabled actions explain why when the reason is not obvious.
- Long-content tests cover every migrated screen family.
- The 426×240 baseline remains unless a separate resolution decision is
  approved by the owner.

## L. Risks and failure modes

- **Framework creep** — building a general UI toolkit instead of the minimal
  primitives; reject via slice review.
- **Big-bang migration** — migrating all states at once creates an
  unreviewable diff; the family-by-family slices are the control.
- **426×240 pressure** — measured text may prove some screens cannot fit;
  the failure mode is silently changing resolution or font size project-wide.
  Escalate with evidence instead.
- **Per-frame allocation** — naive wrapping allocates strings every frame;
  cache wrapped lines per content change (CLAUDE.md hot-path rule).
- **Behavior drift during migration** — restyling a screen must not change
  its logic; keep migrations layout-only and lean on existing state tests.
- **Test blind spots** — pure tests can pass while real-font rendering
  differs; the manual matrix at multiple window shapes is mandatory.

## M. Required documentation updates on completion

- `docs/milestones.md` — status transitions.
- This note — actual implementation, deviations, final slice list.
- `docs/ui_style_guide.md` — implemented roles/spacing/policies.
- `docs/technical_design.md` — new UI-layer architecture section.
- `docs/manual_test_matrix.md` — updated rows for migrated screens.
- `.claude/skills/crystal-dungeons/SKILL.md` — layout-usage gotchas.
- Completion report per `docs/milestone_completion_template.md`.
