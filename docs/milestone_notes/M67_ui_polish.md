# M67 — UI polish: portraits, panel descriptions, load-screen fixes, boss-return bug

Owner feedback batch from the M62–M66 manual pass (2026-07-28), plus the
boss-victory bug the owner reported the same day. Implemented 2026-07-28.

## A. Status

**☑ complete (approved by the owner 2026-08-05)** — implemented 2026-07-28. Evidence in §E.

## B. Scope (owner brief, verbatim intents)

1. A graphical representation of the character in the party menu, the
   Training Hall, the milestone selection, and the Equipment Shop.
2. In the party panel, milestones and passive skills get **descriptions**
   in another font colour.
3. The Load screen's King title ("Breaker of the Hollow Throne") hung half
   outside the selection rect — fixed inside.
4. The Load screen's constant "party 4" text — removed (the party is always
   four; it said nothing).
5. (Reported bug, same session) After a dungeon boss kill the player could
   be left standing in the dungeon instead of returning to town.
6. (Reported bug, same session) The M66 dungeon chart's "[Enter] Read the
   weathered map" prompt could show while Enter did nothing.

## C. As implemented

- **Class portraits** — one shared helper, `ui::drawActorPortrait`
  (UiDraw): the character's existing `actor.<classId>.battle` sprite in a
  small Inset frame, point-crisp at 1× or 2×. No new assets. Where it
  shows: the **party panel's member list** leads every row with the raw
  24×24 sprite; the **Training Hall** shows the highlighted member (member
  list) and the selected member (character menu + passives) at 2×; the
  **milestone modal** carries a 1× portrait in its header corner; the
  **Equipment Shop** shows the member being outfitted at 2× in the free
  column right of the list (cursor-following while choosing, then pinned
  through the slot/item phases).
- **Party panel descriptions** — the equipped passive's description and
  each chosen milestone's description render in the **hint colour** under
  their names; unchosen reached tiers compress to one line ("Milestone
  unchosen: Lv.20, Lv.30"). Pitches tightened and the detail frame grew
  2px; the skills block's wrap budget is now computed from the space the
  descriptions actually left (min 2 lines), so the overflow lint stays
  honest. Feedback messages moved from the panel floor (now occupied) to
  the equip-shop overlay-banner idiom, error/success coloured.
- **Load/Save slots** — the row label is now `Lv.N Ng`; a titled cursor
  row's selection slab covers both lines (25px) and the title dropped to
  font 8 at y+12, fully inside. `SaveSummary.partySize` stays (tested
  metadata); only the display dropped it.
- **Boss-return fix** — the real cause: `DungeonResultState` returned to
  town with two blind pops, and the M63 level-up modal that a boss kill
  pushes (XP is granted before `completeDungeon()`) wedged between the
  dungeon and the result, absorbing the second pop — the modal died unseen
  and the player stayed in the dungeon (where the boss marker was even
  re-fightable, risking a double score submit). Fix inverts the unwind:
  `completeDungeon()` sets `runComplete_`, the result pops **only
  itself**, and `DungeonState::onResume` pops itself once `runComplete_`
  is set. Anything wedged between now gets its turn on top first — a boss
  kill's milestone choice **shows after the result screen**, then the
  dungeon unwinds to town. No battle-rules or generation change.
- **Chart/buried latch fix** — `DungeonState::recomputeInteraction` reset
  `onChest_`/`onMapPiece_` every step but never the M66 `onChart_` /
  `onBuried_` flags, so one step across a chart or X tile latched them for
  the rest of the run: the "Read the weathered map" / "Dig up" footer
  prompt stuck everywhere, and — because `interact()` checks those flags
  first — Confirm was silently swallowed from then on (chests and battles
  included; the room-guarded `readChart`/`digBuried` just returned). Both
  flags now reset with the others, so the prompt and the action exist only
  while actually standing on the tile.

## D. Files changed

`ui/UiDraw.{hpp,cpp}` (drawActorPortrait + portraitBox),
`states/PartyState.{hpp,cpp}`, `states/TrainingHallState.cpp`,
`states/MilestoneChoiceState.cpp`, `states/EquipShopState.cpp`,
`states/SlotMenuState.cpp`, `states/DungeonState.{hpp,cpp}`
(runComplete_; the onChart_/onBuried_ reset),
`states/DungeonResultState.cpp` (single pop),
`capture/CaptureRunner.cpp` (scene 79 exercises the description lines),
docs. No data, schema, save-format, battle-rules, or generation changes.

## E. Automated validation (2026-07-28)

- Debug build: zero project warnings.
- `--capture`: **82/82 scenes clean** — the reworked party panel, both
  slot screens, the training hall (members + passives), the milestone
  modal, and the equip-diff scene all render the new layouts inside the
  overflow lint; scene `79_party_panel` now also pins a chosen milestone's
  description and an equipped passive's description.
- Full suites: **593/593 Debug and 589/589 Release tests green** (the
  4-case gap is the debug-only god-mode battery), re-run in full after the
  chart-latch fix landed (capture re-ran too: 82/82). No test changes were
  needed — no rules, generation, schema, or save behaviour moved.
- Not covered by automation: the state-stack unwind itself (render states
  need a live window; no headless harness exists for them) — manual
  checklist row 1 below is the verification.

## F. Manual owner checklist

1. **The reported bug:** clear a dungeon boss with a member about to cross
   Lv.10/20/30 (or a postponed choice pending). After the result screen's
   "Return to Town", the milestone choice appears — and after choosing
   (or postponing) you land in **town**, never back in the dungeon. Also
   clear a boss with nothing pending: result → town directly.
2. Party panel: every member row leads with its class sprite; the equipped
   passive and each chosen milestone show a hint-coloured description; a
   fresh party shows "Milestones: none yet"; nothing clips at 426×240.
3. Training Hall: the portrait follows the cursor on the member list and
   pins on the character/passives screens.
4. Milestone modal: the chooser's portrait sits in the header corner.
5. Equipment Shop → Equip: the portrait follows the member list cursor and
   stays through slot/item; Buy phases show no portrait.
6. Load screen: rows read `Slot N - Lv.X Ng` (no "party 4"); the King
   title sits fully inside the highlighted slab on every row.
7. Feel: whether the 2× portraits read well and the party panel's denser
   detail column stays comfortable — owner judgment.
8. **The chart bug:** walk ACROSS the M66 chart tile (or the buried X)
   without pressing Confirm, then step off — the footer prompt must vanish
   immediately, chests and battles must still answer Confirm, and stepping
   back onto the tile must read/dig normally. Before this fix the prompt
   stuck for the rest of the run and swallowed Confirm everywhere.

## G. Known limitations

- The party panel's skills block wraps into whatever room the descriptions
  leave (min 2 lines); a maxed member with three chosen milestones AND a
  very long skill list may elide the tail of the skills line (logged by
  the overflow counter, never clipped mid-glyph).
- Portraits reuse the battle sprites by design (placeholder-to-final art
  policy unchanged); no new art was generated.
- The state-stack unwind has no headless test (render states); it is
  covered by checklist row 1.

## H. Final status

`complete (approved 2026-08-05)`
