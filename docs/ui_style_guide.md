# UI Style Guide — Baseline Contract (M11)

> Status: **implemented** — measurement, wrapping, overflow policy, list
> scrolling, and typography roles landed in M12 (`src/ui/TextLayout` +
> `src/ui/UiDraw`; see `docs/technical_design.md` §15); prompt labels are
> binding-derived since M13; the original bitmap **font** arrived in M25
> (§2, §12) and grew its Latin localization set in M87; and the **M46 procedural UI kit** delivered the final visual
> identity ("humorous 8-bit-plus fantasy micro-caricature") — palette roles
> and constructions live in `src/ui/UiStyle.hpp` / `src/ui/UiDraw.hpp` and
> are summarized in `docs/technical_design.md` §15. Every screen is
> migrated. Any bounded draw that cannot fit logs `[ui-overflow]` and
> clips — a clean log during the manual matrix is part of screen acceptance.

## 1. Canvas and safe bounds

- Virtual resolution: **426×240**, scaled aspect-preserved (nearest-neighbor)
  to the window; default window 1278×720 (3×). Changing the resolution is an
  owner-gated decision — evidence from M12, decision outside it.
- Provisional safe area: **4px** outer margin on all sides (matches current
  panel insets). Nothing required may render outside it.
- **Footer reservation:** the bottom **16px** row (y = 224..240) is reserved
  for control hints / transient messages (current town/dungeon convention).
  Screens using a bottom panel (battle: 64px) treat the panel as the
  reservation.
- Top-left corner (x<160, y<16): one occupant per screen (defect
  UI-LAYOUT-009, resolved in M12 — the debug overlay now starts hidden and
  F1-toggles, so it never contests a HUD by default).

## 2. Text roles found in the current game

**Font (M25; Latin set M87):** text now renders through an **original bitmap
font** (not the raylib default), installed by `ui::setFonts` and drawn via the
`DrawTextEx` wrappers in `src/ui/UiDraw`. One 5×7 glyph design is delivered as
three BMFont descriptors so the dominant sizes stay crisp — `font.ui.small`
(base 8), `font.ui.main` (base 10), `font.ui.title` (base 20, a 2× atlas);
intermediate sizes scale from the nearest base with point filtering. Generated
by `tools/asset_gen/generate_font.ps1` (see `docs/asset_pipeline.md`). Since
M87 the set covers **161 glyphs**: printable ASCII plus every Latin-1
Supplement letter and `¡ ¿ « »` (accents in the cell's top two rows,
compressed capitals — ASCII forms byte-identical). `src/ui/GlyphCoverage.hpp`
is the coverage contract and `tests/test_glyph_coverage.cpp` enforces it
against both the shipped `.fnt` files and all shipped content text (§11).
Missing font assets fall back to the raylib default font, so nothing crashes;
an unsupported codepoint renders the `?` fallback glyph — which the content
lint exists to prevent. The size roles below (`src/ui/UiStyle.hpp`) are
unchanged; they now select a base font by size.

| Provisional role | Size today | Where seen |
|---|---|---|
| `title.hero` | 22 | title screen name |
| `title.screen` | 16–18 | screen headings (Help, shops, results) |
| `heading.panel` | 14 | pause panels |
| `body` | 10–12 | menus, messages, most content |
| `body.small` | 9 | footer hints, secondary lines |
| `caption` | 8 | HUD lines, unit names, danger labels, statuses |

Rules for M12-c:
- Collapse to a small named set (≈5 roles); no ad-hoc numeric sizes in states.
- 8px text is at the legibility floor at 1× scale — flag every use during
  migration; owner judges which survive.

## 3. Spacing scale (provisional)

Derived from current layouts: **2 / 4 / 8 / 12 / 16 / 24** px steps. Menu row
heights in use: 11–24px depending on density; M12-b standardizes per role
(dense list / normal list / spaced menu). Panel padding: 8px minimum from
border to content (current panels vary 8–22px; standardize in M12-b).

## 4. Contrast targets

Practical engineering targets (not a formal WCAG conformance claim):

- normal text ≥ **4.5:1** against its effective background;
- large text (≥14px roles) ≥ **3:1**;
- focus/selection indicators and meaningful non-text UI ≥ **3:1** against
  adjacent colors.

Current palette generally passes on the dark backgrounds; known review items
for M12-c: disabled gray `(90,90,110)` on dark panels (borderline by design —
verify it stays above 3:1 or pair with a lock glyph), 8px danger-tier labels
over variable room tiles (needs a backing strip once art lands), and white
building labels over the mid-green town field.

Text over variable art must get a stable backing (panel strip, shadow, or
outline) — rule inherited from CLAUDE.md.

**Palette accessor (M22; re-valued and extended in M46).** Shared UI colors
are read through `style::palette()` (`src/ui/UiStyle.*`): a **28-role
semantic table** (text, surfaces, borders, accents, meters, structure) on
the owner-approved storybook foundation, and a high-contrast twin (pure
white text, brighter borders, darker fills — separation increases) selected
by the Settings "High Contrast" toggle. The palette is the one deliberate
mutable-global exception: a single table pointer, written only by the
settings-apply path, single threaded. New code uses `palette()` roles — no
ad-hoc `Color` literals outside world-space art; the legacy constants
remain only for compile-time contexts and equal the standard text roles
(test-pinned, with contrast floors, in `tests/test_ui_kit.cpp`). The
high-contrast mode never removes the marker+shape+color double-signal
rules above.

## 5. Focus and selection

- Convention (M46): a focused row carries **three signals** — the selection
  slab (`drawSelectionSlab`: rowFill + gold border + end-cap), a stepped
  chevron (with the sanctioned 1px `motionPhase()` nudge), and
  cursor-tinted text. Battle targeting uses `drawFocusBrackets`. Color
  alone is prohibited for selection, danger, rarity, status, and
  availability (CLAUDE.md).
- Focus must always be visible: selected rows may never scroll out of the
  viewport or render under other content (M12-b contract).
- Exactly one focused element per screen; modals take exclusive focus.

## 6. Disabled state

- Current: gray tint, cursor skips disabled rows (`Menu::step`).
- M12 rule: disabled rows the cursor can *reach* must be able to explain why
  (e.g. "(no saves)", "(max)", "(need 120g)") when the reason is not obvious;
  Training Hall's "(max)" is the model. Unreachable-but-visible disabled rows
  keep the gray + reason suffix.

## 7. Overflow policies (M87 contract)

Every text-bearing element uses measured bounds and exactly one of the four
M87 policies:

| Policy | Use for | Overflow means |
|---|---|---|
| **A. Single-line bounded** (`drawTextFitted`, menu rows, chips) | menu/stat rows, prices, labels, editor rows — one line is part of the design; authored short labels where needed | a DEFECT: logged `[ui-overflow]`, clipped, fails the capture lint |
| **B. Fixed wrapped preview** (`drawTextPreview`) | compact decision-time summaries: battle skill/item descriptions during selection, the party panel's passive/milestone/skill lines | INTENTIONAL: explicit `hasMore`, stepped down-arrow marks it, full text one Details press away — never counted as overflow |
| **C. Scrollable wrapped prose** (`ui::TextViewport` + `drawTextViewport`) | reading surfaces: Details overlay, storyteller, tutorial prompts, bestiary flavor, event flavor/outcomes, curio lore, treasure/castle results | EXPECTED: Up/Down reaches every line, clamped; more-above/below arrows; scissor-clipped; titles, trade-off/consequence lines, and control hints stay OUTSIDE the scrolling body |
| **D. Scrollable row lists** (`ScrollWindow` + `drawMenuScrolled`) | shops, inventories, rosters, save slots, scoreboard, battle skill/item lists | EXPECTED: selection stays visible; arrows indicate more |

A few deliberate fixed wrap budgets remain under policy A discipline (the
boss telegraph's 2 lines, the event panel's trade-off line, choice-modal
descriptions): exceeding one is an authoring defect the capture lint
catches — content, including translations, must be tightened to fit.

Silent truncation and guessed character widths are prohibited. After M87,
`[ui-overflow]` in a debug build means an actual layout defect — never
"more text below a scroll viewport" and never an intentional preview.

## 8. Confirmation and error conventions

- Transient success/info messages: bottom message line, auto-clearing
  (current town/shop pattern) — must not permanently replace control hints.
- Errors: explicit, readable, never silent (`Load failed: …` is the model).
- Destructive actions (overwrite save, quit with unsaved progress, retreat):
  a consequence line at minimum today; a confirm step required by M22
  (defect CTRL-011). Layouts from M12-d must leave room for it.

## 9. Panels

- Identity (M46): the procedural `drawFrame` construction — hard 2px offset
  shadow, ink keyline, mid border, top-left highlight, bottom-right depth,
  2px stepped ink corners — in its `FrameStyle` variants (Standard / Raised
  / Inset / Danger / Reward / Crystal / Overlay). The nine-patch
  `drawFramedPanel` path remains functional and manifest-replaceable but is
  unused by screens.
- Overlay modals dim the background via `drawModalDim` (`palette().modalDim`).
- **Class portraits (M67):** `ui::drawActorPortrait` frames the character's
  existing `actor.<classId>.battle` sprite in an Inset panel, point-crisp at
  1× (modal headers) or 2× (a screen's free side column). It is the one way
  a menu screen shows "who": party-panel member rows (raw 24px sprite,
  no frame — rows are too tight), the Training Hall, the milestone modal,
  and the Equipment Shop's equip phases. No dedicated portrait art exists;
  the battle sprite is the character's face everywhere.

## 10. Formerly unresolved decisions — all settled

The open questions this section once tracked are resolved: the role set and
scrolling policies landed in M12 (scoreboard scrolls; 8px survives as the
caption floor), prompt grammar became binding-derived in M13 (M46 renders
hints as keycap groups via `drawFooterHints`), the debug overlay defaults
to hidden (M12), the bitmap font shipped in M25, and the final visual
identity — palette, frames, iconography, motif language — is the **M46
procedural kit** (§4, §5, §9; `src/ui/UiStyle.hpp` / `src/ui/UiDraw.hpp`
are the authority).

## 11. Authored-text rules (M87) — binding for content authors and AI agents

- **Prose never uses manual line breaks to fit a panel.** `\n` in authored
  content is semantic only (paragraph/section separation). Wrapping is owned
  by the UI; scrolling is owned by the text container. A body that "needs" a
  newline to look right in English will be wrong in every other language.
- **Fixed one-line UI uses authored short labels** where the design demands
  one row (policy A). Abbreviations there are deliberate design, not
  truncation.
- **Battle previews may summarize** (policy B): the 2-line skill/item
  description during selection intentionally truncates with a visible
  more-arrow; the full text must always be reachable through Details.
- **Glyph coverage is part of localization validation.** Content may use
  printable ASCII, the Latin-1 Supplement letters, `¡ ¿ « »`, and semantic
  newlines — nothing else (no tabs, no control characters, no Latin
  Extended like `œ`, no CJK). `src/ui/GlyphCoverage.hpp` is the authority
  and `tests/test_glyph_coverage.cpp` fails on any offending string in
  `data/*.json`, so an unsupported character is caught before it renders as
  the `?` fallback.
- **The few deliberate fixed budgets** (telegraphs, the event trade-off
  line, choice-modal descriptions) are authoring constraints in every
  language; the capture lint is the referee.

## 12. HUD conventions (M25)

- **On-demand identity, not permanent labels.** Battle sprites are the primary
  visual element; a unit's name and judgment stats are shown on the
  **target-info panel** while it is being targeted (and via Details), never
  painted over every sprite. The target panel lives in the dedicated bottom
  panel so it cannot collide with the side-specific status lines (party
  statuses below their row, enemy statuses to the right). It shows name, HP
  (and MP for allies), the ATK/MAG/DEF/SPD stats a player needs to judge a
  target, and active statuses. M28's enmity model depends on this panel.
- **Numeric resources.** Resources that gate decisions (HP and **MP**) are
  shown as `label cur/max` numerals, not only bars.
- **Inline adjustable values.** A value the player adjusts sits on the row
  that changes it — either composed into the label on rebuild (the
  `SettingsState::volumeLabel` idiom) or rendered as an M46 stepper row
  (label + arrows + framed value capsule, the Guild/party-creation idiom).
  `ui::MenuItem` carries `label` + `enabled` + the optional right-aligned
  `suffix` column (M42). Non-adjustable readouts (e.g. the Guild seed chip)
  stay separate.
- **Developer diagnostics** (e.g. the title-screen content-count line) are
  gated out of Release with `CRYSTAL_SHIPPING_BUILD`; the version stamp stays
  in Release so bug reports can cite a build.
