# UI Style Guide — Baseline Contract (M11)

> Status: **implemented (M12)** for measurement, wrapping, overflow policy,
> list scrolling, and typography roles — the roles/colors/spacing live in
> `src/ui/UiStyle.hpp`, the policies in `src/ui/TextLayout` +
> `src/ui/UiDraw` (see `docs/technical_design.md` §15). Still *not* an art
> bible — final colors, fonts, frames, and motifs are decided in M15.
> Not yet migrated to the system: party creation, save slots, Guild, pause
> menus (they fit today); prompt strings become binding-derived in M13.
> Any bounded draw that cannot fit logs `[ui-overflow]` and clips — a clean
> log during the manual matrix is part of screen acceptance.

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
- Top-left corner (x<160, y<16) is currently contested by HUDs and the debug
  overlay (defect UI-LAYOUT-009); M12 must assign it to exactly one occupant
  per screen.

## 2. Text roles found in the current game

Raylib default font at these sizes (to be formalized as named roles in M12-c
and replaced by real fonts via M14/M15):

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

**Palette accessor (M22, owner-approved).** Shared UI colors are now read
through `style::palette()` (`src/ui/UiStyle.*`): a standard table
byte-identical to the legacy constants, and a high-contrast table (pure
white text, brightened dim/hint/disabled/accent roles) selected by the
Settings "High Contrast" toggle. The palette is the one deliberate
mutable-global exception: a single table pointer, written only by the
settings-apply path (Application startup and the Settings row), single
threaded. New code uses `palette()`; the legacy constants remain only for
compile-time contexts and as the standard table's definition. The
high-contrast mode brightens roles — it never removes the marker+color
double-signal rules above.

## 5. Focus and selection

- Current convention: `>` cursor glyph + yellow tint `(240,220,120)`. Keep
  both signals: **marker + color** — color alone is prohibited for selection,
  danger, rarity, status, and availability (CLAUDE.md).
- Focus must always be visible: selected rows may never scroll out of the
  viewport or render under other content (M12-b contract).
- Exactly one focused element per screen; modals take exclusive focus.

## 6. Disabled state

- Current: gray tint, cursor skips disabled rows (`Menu::step`).
- M12 rule: disabled rows the cursor can *reach* must be able to explain why
  (e.g. "(no saves)", "(max)", "(need 120g)") when the reason is not obvious;
  Training Hall's "(max)" is the model. Unreachable-but-visible disabled rows
  keep the gray + reason suffix.

## 7. Overflow policies

Every text-bearing element must use measured bounds and exactly one policy:

| Policy | Use for |
|---|---|
| **wrap** | descriptions, messages, telegraphs, multi-line help |
| **scroll** (list viewport, indicators, selection kept visible) | any variable-length list (shops, skills, items, scoreboard, save slots) |
| **paginate** | scoreboard-like ranked lists if scroll reads poorly |
| **authored short label** | fixed labels that must fit one row |
| **ellipsis + Details** | only when the full value is one action away; never for critical instructions, errors, costs, or confirmations |

Silent truncation and guessed character widths are prohibited. Overflow in a
debug build must produce a visible/logged diagnostic (M12-a deliverable).

## 8. Confirmation and error conventions

- Transient success/info messages: bottom message line, auto-clearing
  (current town/shop pattern) — must not permanently replace control hints.
- Errors: explicit, readable, never silent (`Load failed: …` is the model).
- Destructive actions (overwrite save, quit with unsaved progress, retreat):
  a consequence line at minimum today; a confirm step required by M22
  (defect CTRL-011). Layouts from M12-d must leave room for it.

## 9. Panels

- Current identity: double-border 16-bit frame (`drawPanel`: fill + border +
  darkened inner line). Keep as the placeholder identity until M15 restyles
  it through the asset/theme pipeline.
- Overlay modals dim the background (`0,0,0,150` — pause menus). Keep.

## 10. Unresolved decisions (owned by later milestones)

- **M12:** final role set and row-height table; scroll vs paginate for the
  scoreboard; where the encounter preview (UI-INFO-005) lives; whether 8px
  survives as a role; description-line pattern in lists (UI-INFO-004).
- **M13:** prompt-label grammar ("Confirm/Cancel" vs key names) and
  device-specific hint layout in the footer.
- **M15:** fonts, palette, frame art, iconography, motif language — all
  final visual identity. Nothing in this guide pre-decides it.
- **Owner decision pending:** default-off for the debug overlay
  (UI-LAYOUT-009) and removal of the stale title label (UI-TEXT-010) —
  recommended to bundle with M12-a.
