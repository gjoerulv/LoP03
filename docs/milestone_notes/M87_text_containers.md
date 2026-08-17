# M87 — Translation-ready text containers & scrollable prose

Authorized 2026-08-11 by the owner (direct brief, full implementation
authorized in the same message). Implemented 2026-08-11 on the post-M86
checkout (branch `oyb08`).

## A. Status

**☑ complete (approved 2026-08-16)** — owner batch-approved M86–M97 on
2026-08-16; see §F for evidence.

## B. Goal (owner brief)

The game is functionally complete, but its prose-heavy UI is still sized
around the current English content: fixed line budgets, shrink-to-fit,
truncate-and-log, and panels that grow with their body. Before translations
arrive, prose containers must behave like proper bounded text areas — text
wraps to the container, the container caps its height, scrolling reaches
the rest — so translated text can expand naturally without authors
hand-inserting layout newlines and without silent clipping. The bitmap
font must also cover the Latin characters those translations need.

## C. The four text-layout policies (the contract)

Every text-bearing UI element uses exactly one of these policies. This
section is the authority future work binds to (summarized in
`docs/ui_style_guide.md` §7).

- **A. Single-line bounded text** — menu rows, stat rows, prices, chips,
  labels, editor rows. Real measurement, explicit width, no spill;
  authored short labels where the design demands one line. Genuine
  overflow logs `[ui-overflow]` and clips (unchanged M12 behavior).
- **B. Fixed wrapped preview** — compact decision-time summaries: the
  battle skill/item description while selecting, the party panel's
  passive/milestone description lines. Wraps to a small fixed budget, no
  scrolling; **intentional** truncation is explicit (`hasMore`), shows a
  more-indicator, does **not** count as a layout overflow, and the full
  text is one Details press away.
- **C. Scrollable wrapped prose** — reading surfaces: the Details
  overlay, the storyteller, tutorial prompts, bestiary flavor, dungeon
  event flavor and outcomes, curio lore, party full details. A bounded
  content rectangle; wrapped to its width with real metrics; visible
  height capped by the widget; Up/Down scrolling, clamped; more-above/
  more-below indicators; scissor-clipped as the final safety boundary;
  titles, trade-off/consequence lines, and control hints stay outside
  the scrolling body. Authored `\n` is semantic (paragraphs), never
  layout.
- **D. Scrollable row lists** — shops, inventories, rosters, save slots,
  scoreboard: the existing `ScrollWindow` + `drawMenuScrolled` machinery,
  unchanged.

## D. As implemented

### The pure model (`src/ui/TextViewport.hpp`, header-only)

`ui::TextViewport` — the smallest scrollable wrapped-prose model:
`setContent(text, width, fontSize, measure)` wraps once (via the M12
`wrapText`, so UTF-8/long-token/paragraph behavior is shared, not
duplicated) and caches the lines with their (text, width, font) key so
per-frame re-set is free; `setVisibleLines`, `scrollBy` (returns whether
the view moved; clamped), `scrollToTop`, `top`, `lineCount`,
`visibleLines`, `visibleCount`, `moreAbove`, `moreBelow`, `line(i)`.
Scrolling composes the existing pure `ScrollWindow` internally — prose
viewports and row lists share one clamping model (the M12 primitive, per
the owner's "small shared primitive" allowance).

`ui::previewText(text, width, fontSize, measure, maxLines)` (in
`TextLayout`) returns `{lines, hasMore}` — the pure core of policy B.

### The adapters (`src/ui/UiDraw`)

- `drawTextViewport(vp, x, y, w, fontSize, color)` — draws only the
  visible window, scissored to the content rect, and paints the chunky
  stepped more-above/more-below arrows (the `drawMenuScrolled` shapes) at
  the rect's right edge. Scrollable overflow is **not** an overflow
  event.
- `drawTextPreview(text, x, y, w, fontSize, color, maxLines, markMore)` —
  draws at most `maxLines` wrapped lines; when more exists it draws the
  stepped down-arrow at the block's bottom-right and returns
  `{hasMore, bottom}` **without** touching the `[ui-overflow]` counter.
- `drawTextWrapped`/`drawTextFitted` keep their M12 contract: exceeding a
  hard budget is a defect, logged and counted. After M87 that counter
  means an actual layout defect — never "there is more to scroll" and
  never "a preview intentionally summarized".

### Screens converted to policy C (scrollable prose)

- **`DetailsOverlayState`** — the canonical reading overlay. Capped
  panel (screen safe area), fixed title, scrolling body viewport, Up/Down
  scroll with indicators, Confirm/Cancel/Details closes, scroll hint in
  the footer line when scrollable. All body text reachable; the M22
  truncation-to-cap is gone.
- **`StoryDialogState`** — the panel no longer grows unbounded with the
  wrapped body: height caps inside the safe area (fixed title + speaker,
  scrolling body, fixed Continue line). Story JSON needs no layout
  newlines; all text reachable.
- **`TutorialPromptState`** — same treatment (it was the story panel's
  twin); beats keep fitting without scroll at authored length, but a
  longer translation scrolls instead of growing the panel off-screen.
- **`BestiaryState`** — the selected entry's flavor no longer shrinks to
  the small font and truncates: it renders at the body font in a bounded
  viewport filling the panel's remaining room. When it overflows, the
  **Details action toggles read focus** — focus brackets move to the
  flavor region, Up/Down scrolls it (roster browsing is untouched
  outside read focus), Cancel/Details/Confirm exits. Footer hints swap
  accordingly ("Read" appears only when there is something to scroll).
- **`DungeonState` event flavor panel** — fixed title, **scrolling
  flavor viewport** (up to 4 visible lines, more via Up/Down), then the
  fixed gold trade-off line and the fixed Step-away hint — cost/risk
  stays visible before commitment regardless of flavor length. The M80
  "authored body must fit 4 lines" rule is repealed (see §E tests).
- **`DungeonState` outcome panel** — same: scrolling body viewport (up
  to 4 visible lines), fixed title and Continue hint.
- **`MapsState` curio lore panel** — body viewport with scroll (the 7-line
  hard budget is gone).
- **`TreasureFightState` / `CastleChallengeState` result panels** — the
  result body scrolls beyond its previous hard budget (5/…-line caps
  removed); fixed title/prompt rows unchanged.

### Battle (policy B + context-sensitive Details)

- The skill/item descriptions during `ChooseSkill`/`ChooseItem` stay a
  compact 2-line preview — **no scrolling in the battle panel** — but
  truncation is now explicit: the preview draws the more-indicator and
  no longer logs `[ui-overflow]`.
- **Details is context-sensitive:** during skill selection it opens the
  selected skill's full sheet (name, MP cost, category/target, element,
  full description) in the scrollable Details overlay; during item
  selection, the selected item's full sheet (name, held count, effect,
  full description); in command/target phases it opens the unit/battle
  details exactly as before. No battle mechanics changed.

### Party (compact overview + full text via Details)

The panel keeps its M72 compact layout; the passive/milestone
description lines become policy-B previews (no `[ui-overflow]` on a
long translation). The **Details action** opens the member's full sheet
in the scrollable overlay: XP, gear, equipped passive with full
description, every milestone with full description, and **every known
skill with its full description** (which the panel never had room for).
Footer advertises it. The M72 known limitation ("descriptions longer
than two wrapped lines would elide") is closed.

### Localization readiness (font + coverage)

- `tools/asset_gen/generate_font.ps1` grows from printable ASCII (95
  glyphs) to **161 glyphs**: all 62 Latin-1 Supplement letters
  (`À..ÿ` minus `×`/`÷`, including `æ ø å Æ Ø Å ä ö ü Ä Ö Ü ß ç é è ê ë
  à â í ì î ï ó ò ô õ ú ù û ñ ð þ ý ÿ`), the Spanish `¡ ¿`, and the
  guillemets `« »` — the set stops there: no CJK, no combining marks.
  Same 5×7 cell, same deterministic pipeline, same three descriptors
  (`.fnt` diff: header + 66 appended char entries only); accents live in
  the cell's top two rows, accented capitals use the classic compressed
  pixel forms, and the existing ASCII glyph rects/advances are
  byte-identical.
- **`src/ui/GlyphCoverage.hpp`** (pure, raylib-free) is the single
  authority for the supported codepoint set: `kSupportedCodepoints`,
  `isSupportedCodepoint`, `firstUnsupportedCodepoint(utf8)` (returns the
  offending codepoint or 0), and a UTF-8 decoder shared by the tests.
- Tests bind the three layers together: the generated `.fnt` files must
  cover the required set; every user-facing string in the shipped
  `data/*.json` must decode as valid UTF-8 and contain only supported
  codepoints (and no tabs/control characters) — so a future translation
  with an unsupported glyph fails the suite instead of rendering `?`.
- `wrapText` whitespace audit: spaces break, runs collapse, `\n` is a
  paragraph break, **NBSP (U+00A0) glues** (correct for `250 g`-style
  figures), tabs are rejected by the content lint rather than half-
  supported by the engine. Documented in `ui_style_guide.md`; no
  Unicode line-breaking engine was added (Latin-script scope only).

### Authored-text rules (now in `docs/ui_style_guide.md` §11)

Prose never uses manual line breaks to fit a panel; newlines are
paragraph semantics only; wrapping belongs to the UI, scrolling to the
container; one-line UI uses authored short labels; battle previews may
summarize with full text behind Details; glyph coverage is part of
localization validation. The audit found **no** layout-only newlines in
shipped story/flavor/lore content (all bodies are single-paragraph
strings), so no prose was rewritten.

## E. Files changed

- **UI core:** `src/ui/TextLayout.{hpp,cpp}` (`previewText`),
  `src/ui/TextViewport.hpp` (new), `src/ui/UiDraw.{hpp,cpp}`
  (`drawTextViewport`, `drawTextPreview`, shared indicator helper),
  `src/ui/GlyphCoverage.hpp` (new).
- **States:** `DetailsOverlayState.{hpp,cpp}`,
  `StoryDialogState.{hpp,cpp}`, `TutorialPromptState.{hpp,cpp}`,
  `BestiaryState.{hpp,cpp}`, `PartyState.{hpp,cpp}`,
  `BattleState.{hpp,cpp}`, `DungeonState.{hpp,cpp}`,
  `MapsState.{hpp,cpp}`, `TreasureFightState.cpp`,
  `CastleChallengeState.cpp`.
- **Assets:** `tools/asset_gen/generate_font.ps1`,
  `assets/fonts/font_atlas.png`, `assets/fonts/font_atlas_2x.png`,
  `assets/fonts/font_small.fnt`, `font_main.fnt`, `font_title.fnt`
  (regenerated), `assets/credits.md` (row updated).
- **Capture:** `src/capture/CaptureRunner.cpp` (pseudo-localization
  transform + 8 new scenes, `98`–`105`; the set is **105 scenes**).
- **Tests:** `tests/test_text_viewport.cpp` (new),
  `tests/test_glyph_coverage.cpp` (new), `tests/test_text_layout.cpp`
  (preview cases), `tests/test_event_flavor.cpp` (the ≤4-line English
  fit rule replaced by container-behavior checks),
  `tests/CMakeLists.txt`.
- **Docs:** this note, `docs/milestones.md` (row + section),
  `docs/technical_design.md` (§15 M87 subsection; capture count),
  `docs/ui_style_guide.md` (§7 rewritten as the four-policy table, new
  §11 authored-text rules, §2 font coverage),
  `docs/asset_pipeline.md` (font paragraph),
  `docs/manual_test_matrix.md` (M87 block; the battle-Details KNOWN GAP
  marked resolved), `docs/game_design.md` (M80 panel paragraph),
  `docs/control_standard.md` (Details row + semantics),
  `docs/milestone_notes/M72_party_panel_reflow.md` and
  `M80_event_flavor.md` (superseded limitations annotated),
  `.claude/skills/crystal-dungeons/SKILL.md` (gotcha 14 + status).

## F. Automated validation (all run in this session, 2026-08-11)

- Font regeneration: `tools/asset_gen/generate_font.ps1` — 161 glyphs,
  atlas 883×7; the `.fnt` diff is exactly the header lines (scaleW,
  count, `unicode=1`) plus 66 appended char entries — no ASCII entry
  moved.
- Debug build (`cmake --build --preset debug`): clean, zero project-code
  warnings.
- **Full Debug suite: 740/740 tests green** (`ctest --preset debug`;
  15 new cases: 10 `[viewport]`, 5 `[glyphs]`, plus the rewritten
  `[flavor]` containment case). The new content lint swept 14 shipped
  JSON files / 1000+ strings with zero offenders.
- **Capture lint: 105/105 scenes clean**
  (`build-msvc\CrystalDungeons.exe --capture docs\screenshots\m87_captures`)
  — including the 8 new long-prose scenes; the two intermediate failures
  during development were (a) the capture-stretch skill's NAME
  overflowing the policy-A name column (renamed "Winter Saga" — the
  stress belongs in the description) and (b) two scene-setup issues
  (the guild-perk modal re-offering over the story scene; the party
  overlay queued before its panel), all fixed and re-run clean.
- Release build (`cmake --build --preset release`): clean.
  **Full Release suite: 736/736 tests green**
  (`ctest --test-dir build-msvc-rel`; the 4-test delta vs Debug is the
  pre-existing capture/debug-gated set).
- Visual spot-check of the new scenes (in `docs/screenshots/m87_captures/`):
  the Latin pangram renders as pixel glyphs (no `?` fallbacks), scroll
  indicators and hints appear exactly when scrollable, the event panel's
  gold trade-off line stays fixed under a scrolling body, read focus
  brackets the bestiary prose, and the battle preview shows its
  more-arrow without any `[ui-overflow]`.

## G. Manual owner checklist

1. **Details overlay** — open "How Scoring Works" from the scoreboard
   (Details key) and the battle Details in any fight: long bodies show
   the more-below arrow; Up/Down scrolls, clamped at both ends;
   Confirm/Cancel/Details closes; every line is reachable.
2. **Storyteller** — talk through storyteller beats (town 1 inn-side NPC,
   the castle Jester): the panel never leaves the screen, the Continue
   hint stays fixed at the bottom, long beats scroll.
3. **Battle** — in ChooseSkill/ChooseItem: 2-line description previews
   show a small down-arrow when more exists (no `[ui-overflow]` in the
   log); pressing Details opens the full skill/item sheet (scrollable);
   in command/target phases Details still shows the unit sheet.
4. **Bestiary** — select the King (longest flavor): flavor renders at
   the body font; if the down-arrow shows, press Details — focus
   brackets move to the flavor, Up/Down scrolls it, roster stays put;
   Cancel returns to roster browsing. Verify Up/Down browses the roster
   normally outside read focus.
5. **Party** — on a leveled member press Details: the full sheet
   (passive, milestones, every skill with description) opens and
   scrolls; the compact panel is unchanged otherwise.
6. **Dungeon events** — trigger an event (Peddler is the longest): title
   and gold trade-off line always visible, flavor scrolls if long;
   outcomes (chest, dig, refusals) scroll if long; Confirm/Cancel
   semantics unchanged.
7. **Curio lore** — inspect a curio on the Maps screen; long lore
   scrolls.
8. **Font** — set a save/party name with `æøå ÄÖÜ ß é ñ ¿` (the name
   editor permitting); letters render as crisp pixel glyphs, not `?`.
   Check `docs/screenshots/m87_captures/99_details_long.png` for the
   full new-glyph pangram.
9. **Regression feel** — normal menus, shops, scoreboard, battle flow
   look and behave exactly as before (only the prose containers
   changed).

## H. Known limitations

- The battle boss telegraph keeps its authored 2-line budget (policy A/B
  boundary): telegraphs are single-beat theatre, lint-checked at
  authored length; translators must keep them within two lines (the
  lint fails otherwise — loudly, not silently).
- Choice modals (milestone/perk descriptions, ~2-line budgets) keep
  authored budgets; their full texts are reachable in the party panel /
  guild screens after the choice. A translation exceeding the modal
  budget fails the capture lint and needs authored tightening.
- The pseudo-localization transform is capture-only stress, not a
  translation system: no language files, no runtime language switch
  (out of scope by design).
- Glyph coverage is Western/Northern-European Latin only (Latin-1
  letters + `¡ ¿ « »`); Latin Extended (`œ Š ž ő`), CJK, Cyrillic and
  Greek are out of scope. `Œ/œ` in French text must be written `Oe/oe`.

## I. Final status

`implemented, awaiting manual approval`
