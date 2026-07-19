# M12 — UI Layout and Text-Safety Foundation

## A. Status and authority

- **Status:** implemented, awaiting manual approval (implemented 2026-07-19;
  see section N for the as-implemented record and deviations).
- **Last reviewed repository commit:**
  `5311ef9d671e4d4b301dcb4e1a872219f61d3d08` (2026-07-19). Re-audit result:
  the note's baseline observations hold; defect IDs referenced below are from
  `docs/presentation_audit.md`.
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

## N. As-implemented record (2026-07-19)

Base commit `5311ef9`; owner authorized M12 with the M11 approval (including
the approved M12-a slice: measurement/wrapping/diagnostics + Help + battle
panel + the two one-line fixes UI-TEXT-010 / UI-LAYOUT-009).

**Slices delivered:**

- **M12-a** — `src/ui/TextLayout.{hpp,cpp}` (pure, injectable measurement,
  greedy wrap, UTF-8-safe long-token breaking, `lineHeight`); overflow
  diagnostics (`[ui-overflow]` log once per site/text + scissor clip) in
  `src/ui/UiDraw`; Help migrated (measured columns, wrapped footer, and the
  false "Left Stick" claim corrected to "D-Pad" in Help and the README —
  CTRL-006's honest-docs half; the input half stays M13).
- **M12-b** — `src/ui/ScrollWindow.hpp` (pure follow/free-scroll window with
  more-above/below); `drawMenuScrolled` with arrow indicators;
  `drawTextRight`/`drawTextFitted`/`drawTextWrapped` adapters.
- **M12-c** — `src/ui/UiStyle.hpp` (7 font roles, spacing, shared colors);
  adopted by every migrated file (unmigrated files keep their literals until
  touched).
- **M12-d** — migrated: battle bottom panel (command menu fits — fixing
  newly-found **UI-TEXT-024**, see below; skill/item lists scroll 4 rows with
  the selected entry's description; message/telegraph wrapping; 5-enemy
  columns start at y=20 with the turn counter moved top-right and enemy
  statuses beside units — UI-LAYOUT-003); equip shop (9-row scroll +
  slot/stat/description detail line — UI-TEXT-001, UI-INFO-013); item shop
  (10-row scroll + description); scoreboard (right-aligned numeric columns,
  depth column, free scroll + range indicator — UI-TEXT-012 display parts);
  town/dungeon HUDs (measured backdrops — UI-TEXT-008); dungeon fight prompt
  now includes the team name (UI-INFO-005); title (stale label removed —
  UI-TEXT-010; menu centered — UI-020); result panel pitch (UI-LAYOUT-018);
  Inn/Training Hall padding (UI-TEXT-017); debug overlay default-off
  (UI-LAYOUT-009).

**Discovery:** the baseline battle *command* menu drew "Guard" clipped and
"Escape" fully off-screen (y=246 > 240) while still selectable — registered
retroactively as **UI-TEXT-024** (High) and fixed by the panel redesign.

**Deviations from this note:**

1. Not all screen families were migrated: party creation, save slots, Guild,
   and the pause menus keep their current (fitting) fixed layouts; they adopt
   the system when M13 touches their prompts. Acceptance is judged on
   migrated screens; the audit register tracks the rest.
2. The M12-c "contrast validation helper" was not built; the documented
   manual check in `docs/ui_style_guide.md` §4 stands in. Rationale: the
   palette is placeholder until M15; a helper would validate throwaway
   colors.
3. Long/empty/maximum-data coverage is in the pure tests (wrap/scroll edge
   cases) plus the manual matrix rows; no per-screen automated content tests
   (screens are raylib-side — M23 owns automated capture).
4. UI-INFO-005 was resolved via the prompt line (the design contract needs
   name/danger/count/tags only — no separate preview panel was added).

**Validation:** MSVC/Ninja Debug build clean, zero project warnings;
**144/144 tests pass** (19 new: 11 wrap/fit, 8 scroll-window); game launched,
driven, exited cleanly (code 0) twice; post-fix captures of title/Help/town
in `docs/screenshots/m12_after/` confirm overlay-off, label removal, menu
centering, honest Help table, and the measured HUD. Battle/shop/scoreboard
visuals need the owner's manual pass (matrix rows 13–14, 19, 24–31).
