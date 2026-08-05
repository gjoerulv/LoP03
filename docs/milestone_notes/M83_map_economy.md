# M83 — Map economy: 4-floor completions pay map pieces (and IOUs)

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Re-audit this note against the
then-current checkout before implementation begins.

## A. Status

☐ planned

## B. Goal (owner brief)

Completing a 4-floor dungeon in town 2 or later has a rising chance to pay
a puzzle map piece. While the party already holds all four pieces, earned
drops are banked as "owed" (max 3) and paid out after the Map-boss
(treasure-dig guardian) falls — confirmed flow, owner Q&A 2026-08-05.

## C. Scope (planned)

- **Completion roll**: on completing a 4-floor dungeon in town ≥ 2, roll a
  map-piece drop with chance linear in town — **15 / 27 / 39 / 51 / 63 /
  75 %** for towns 2–7 (+12 pts per town). 1-floor runs never roll.
- **Determinism / save-scum resistance**: the roll derives from committed
  run state (a pure hash of the run seed + the fixed salt), never from a
  fresh RNG draw at result time — reloading cannot reroll it (the
  black-market/M65 discipline).
- **Grant path**: a successful roll awards a piece through the existing
  M65 flow (announced on the result screen; Maps screen quadrant fills).
  The existing in-dungeon piece finds are **unchanged** and coexist.
- **The owed bank**: if the party already holds all 4 pieces (a treasure
  stands revealed, not yet dug), a successful roll banks into a new
  optional save field `mapPiecesOwed` (hard cap 3; rolls beyond a full
  bank are simply lost). Winning the treasure-dig guardian fight pays the
  owed pieces out immediately after the piece-consuming dig resolves —
  jump-starting the next map cycle — with a result line saying so.
- **Perk hook**: a `+5% map chance` modifier point (consumed by M84's town
  milestones T2/T5).

## D. Schema, save & version implications

- New optional save field `mapPiecesOwed` (int 0–3, defensive default 0,
  dropped-if-invalid). No rules/generation/score version motion (the roll
  is new logic on the M82 completion event, not a change to what a seed
  generates).

## E. Out of scope

The perk itself (M84); any change to the M65/M66 map, dig, or reward
mechanics.

## F. Dependencies

M82 (4-floor completion must exist).

## G. Acceptance criteria

- Chance table exact per town; town 1 and 1-floor runs never roll;
  reload-before-result cannot change the outcome.
- With 4 pieces held, drops bank up to 3 and pay out after a dig victory;
  the Maps screen and result lines tell the story honestly.

## H. Automated validation

Hash-roll determinism tests; chance-table tests (statistical over seeds);
owed-bank cap/payout/save round-trip tests. Full suite + [treasure]
battery green.

## I. Owner manual validation

Clear 4-floor dungeons across towns until a piece drops; fill the map,
bank an owed piece, dig, and confirm the IOU pays out; judge whether the
drop pacing feels right for the ladder.
