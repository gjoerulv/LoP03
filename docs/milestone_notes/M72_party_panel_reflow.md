# M72 — Party panel reflow

Owner-reported clipping (2026-07-30, with Lv.99 screenshots): a maxed
member with three chosen milestones overflowed the M64/M67 detail panel —
the long Martyr description ran off the right edge and the skills list
truncated mid-legend. Implemented 2026-07-30.

## A. Status

**☑ complete (approved by the owner 2026-08-05)** — implemented 2026-07-30. Evidence in §D.

## B. As implemented

Not a smaller font — a reflow into previously dead space:

- **Vitals column**: HP/MP and the four derived stats (each with its
  "(+N gear)" share) moved into the empty 128×66 block **under the member
  roster**, in their own frame. They pair naturally with the selected
  member and free ~34px of the right panel.
- **Right panel** now holds XP, gear, the equipped passive + description,
  the milestone choices + descriptions, and the skills — and the
  **descriptions wrap to up to two lines** (actual-advance via
  `drawTextWrapped`'s return) instead of force-fitting one line. The
  skills block's computed line budget grows to 4–5 lines in the maxed
  case, so a full Lv.99 list ends with its "(* from a scroll)" legend
  intact.
- Fonts unchanged (9/8) — readability kept; nothing clips at the true
  worst case.

## C. Files changed

`states/PartyState.cpp` (layout only — no data or input changes),
`capture/CaptureRunner.cpp` (scene `79_party_panel` upgraded to the
owner's reported worst case: **level cap, all three cleric milestones
chosen incl. Martyr's longest description**, an equipped passive, a
scroll-learned skill), docs. Nothing else.

## D. Automated validation (2026-07-30)

- Debug build: zero project warnings.
- `--capture` **84/84 scenes clean** — scene 79 now lint-pins the exact
  configuration that clipped in the owner's screenshots; visually
  verified: Martyr wraps to two lines, the skills legend survives, the
  vitals block fits its frame.
- Closing verification: **607/607 Debug and 603/603 Release tests green**
  (counts unchanged — layout-only, no unit-test changes); `--capture`
  **84/84 scenes clean**; zero project-code warnings.

## E. Manual owner checklist

1. Re-check the two reported members (a maxed Cleric with
   Devotion/Purifying Light/Martyr; a Guardian with a long skill list):
   every description fully visible, the skills list complete with its
   legend, nothing touching the frame edges.
2. The vitals block under the roster reads cleanly and tracks the
   selected member; the gear shares still show.
3. A low-level member (few milestones, short skills): the panel does not
   look empty or misaligned.
4. Feel: whether stats-under-roster reads naturally — owner judgment.

## F. Known limitations

- A milestone/passive description longer than two wrapped lines (~110
  chars) would still elide; the current longest is 50 chars, and
  CrystalForge authors should keep descriptions within two lines.

## G. Final status

`complete (approved 2026-08-05)`
