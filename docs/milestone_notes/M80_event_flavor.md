# M80 — Event flavor text: dungeon events find their voice

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Implemented 2026-08-06 on the
post-M79 checkout (`3eb9986`).

## A. Status

**☑ complete (approved)** — implemented 2026-08-06 (incl. the same-cycle
outcome-panel addendum); approved and committed by the owner 2026-08-06
(`ccb4d2a`). Evidence in §F.

## B. Goal (owner brief)

Dungeon events stop being a bare footer prompt: triggering one brings up
lore-flavored, dry-humor text in the center of the screen (then the usual
confirmation/choice), and the text is data-driven so it is editable in JSON
and, later, in CrystalForge.

## C. As implemented

No rules/generation/save version motion — presentation and text only;
event mechanics, costs and seeded rolls are untouched.

### The content (`data/event_flavor.json`, schema v1)

- **Eleven entries — one per real `RoomEventKind`** (the five classics,
  the three theme rites, the Royal Relic, the elite challenge, and the
  Duckling Peddler), each an authored **title** and **body** in the dry
  register. The Peddler's is deliberately the longest (the wrap budget's
  referee): *"A cloaked figure opens one side of its coat. Inside sits a
  single duckling, radiating malice. 'Finest quality,' the peddler
  whispers. 'One per customer.'"*
- `content::EventFlavorDef` + `kEventFlavorIds` (the content-layer
  vocabulary; `dungeon::eventFlavorId(kind)` maps the enum to it, and a
  test holds the two in lockstep). Loader `parseEventFlavor`: unknown
  ids, duplicates and missing title/body are rejected per-entry — a typo
  costs its own panel, never the file.
- **The one OPTIONAL content file**: `loadAll` skips it silently when
  absent (pure presentation with a per-event fallback can never block
  play); present-but-malformed is reported like any other file.

### The panel (`DungeonState`)

- Confirm on an authored event now opens a **centered modal**: title
  (crystal), dry-humor body (wrapped, ≤4 lines), then the **same
  trade-off line the footer used to carry** in gold — cost/risk stays
  visible BEFORE commitment (the M20 bar), including the cannot-pay and
  one-per-customer variants — and the step-away binding at the bottom.
- **Confirm inside the panel commits** (elite challenges start their
  battle; everything else resolves exactly as before); **Cancel/Menu
  steps away** and the event keeps waiting. Movement pauses while the
  panel is up.
- **Fallback**: an event kind with no flavor entry — or the file deleted
  wholesale — keeps the classic immediate footer-prompt path untouched.

### The outcome panel (owner addition, 2026-08-06, same review cycle)

- **Results ride the same centered treatment**: every `resolveEvent`
  outcome (rewards AND refusals — the shrine's mend, the merchant's sale,
  the peddler's "Duck rules", every "you cannot pay"), every **chest
  open** (loot, the trap's bite, the empty re-check) and the **buried
  treasure dig** now raise a dismissable modal — the event's flavor title
  as its heading ("The Chest" / "The Buried Treasure" for the loot
  moments), the result body, and a Continue binding. Any of
  Confirm/Cancel/Menu dismisses.
- The transient footer line REMAINS for the incidental notices: map-piece
  pings, the chart's reveal, battle results at the door, and the
  "Guarded" nudge (where nothing happened yet). The Armory Ghost keeps
  its own interactive screen.
- All result wording is reused verbatim — only the vessel changed.

## D. Files changed

- **Content:** `src/content/Definitions.hpp` (`EventFlavorDef`,
  `kEventFlavorIds`), `src/content/ContentDatabase.hpp/.cpp` (store +
  `findEventFlavor`), `src/content/ContentLoader.hpp/.cpp`
  (`parseEventFlavor`, the optional-file wiring in `loadAll`).
- **Dungeon:** `src/dungeon/ThemeEvents.hpp` (`eventFlavorId`).
- **States:** `src/states/DungeonState.hpp/.cpp` (the panel flag, input
  gate, `confirmEventPanel`, `renderEventPanel`, the
  `captureOpenEventPanel` hook).
- **Capture:** `src/capture/CaptureRunner.cpp` — new scenes
  `86_event_flavor` (the Peddler's panel, seed-searched) and
  `87_event_outcome` (the trapped chest's widest result string); the set
  is now **87 scenes**.
- **Data:** `data/event_flavor.json` (new, 11 entries).
- **Tests:** `tests/test_event_flavor.cpp` (new, 5 cases, `[flavor]`),
  `tests/CMakeLists.txt`.
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md`,
  `docs/technical_design.md` §34, `docs/manual_test_matrix.md`.

## E. Plan deviations

- None of substance. The panel reuses `eventPromptText()` verbatim as its
  trade-off line, so the wording the owner already approved (M20/M37/…)
  is unchanged — the flavor is purely additive above it. CrystalForge
  editing stays deferred to M86 as planned (the file is not an editor
  category, so the canonical byte-stability sweep does not govern it —
  it is hand-formatted).

## F. Automated validation (all run in this session, 2026-08-06)

- Debug build: clean, zero project-code warnings.
- New `[flavor]` cases (6) green: 11/11 coverage both directions with
  non-empty text; the enum↔vocabulary lockstep (with a static_assert
  that a future RoomEventKind must take a stance); the panel wrap budget
  (title one line, body ≤4 at the lint's conservative measure); the
  loader rejections (unknown/duplicate/missing fields) and a valid
  parse; the **optional-file proof** — a copy of the shipped JSONs
  minus `event_flavor.json` loads clean with zero flavors; and a new
  **ASCII test-name lint** (below).
- **Capture lint: 87/87 scenes clean** — `86_event_flavor` renders the
  longest authored flavor panel in situ, `87_event_outcome` the widest
  dynamic result string.
- **Full Debug suite: 678/678 tests green. Release build + suite:
  674/674 tests green** (re-run after the outcome-panel addendum).
- Honesty note: the first full-suite run failed exactly one test — an
  em-dash in a new TEST_CASE name, the THIRD such incident across the
  program (ctest passes case names through the Windows codepage; a
  mangled name matches nothing and reports failure). Beyond the rename,
  a lint test now scans every test source for non-ASCII in TEST_CASE
  headers, so the class of mistake is structurally closed.

## G. Manual owner checklist

1. Enter dungeons across the three themes and trigger one of each:
   shrine, spring, merchant, wager, rest corner, elite totem, the theme
   rite, and (when found) the Peddler. The centered panel reads title →
   flavor → the familiar gold trade-off line; judge the VOICE (dry, never
   jokey-jokey), pacing, and readability over the dimmed room.
2. Cancel steps away — the event still waits; Confirm commits exactly as
   the old footer prompt did (prices, refusals, one-per-customer, the
   elite battle) — and the RESULT now arrives in its own centered box
   (your addition): open chests (plain, trapped, already-empty), take
   every event's reward and refusal, dig a buried treasure. Any button
   dismisses; judge the flow of flavor panel → outcome panel.
3. Edit a line in `data/event_flavor.json`, relaunch — the new text
   shows. Delete an entry (or the whole file) — that event falls back to
   the plain footer prompt and still works.
4. On any failure: which event, which theme, and a screenshot.

## H. Known limitations

- The outcome panel covers events, chests and the buried dig; map-piece
  pickups, the chart's reveal and battle results stay on the quick footer
  line (deliberate — they are pings, not payoffs; one word extends them).

- The pre-panel footer still shows the full classic prompt while facing
  the marker (the panel adds atmosphere on top; removing the footer
  text entirely felt like hiding the trade-off a step too far — owner
  taste welcome).
- Flavor is one authored body per kind — no per-theme or seeded variety
  yet (trivially extendable if wanted).
- CrystalForge cannot edit the file until M86.
  *(Resolved: M86 made it an editor category.)*

## I. Final status

`complete (approved)` — owner approval 2026-08-06, committed as `ccb4d2a`.
