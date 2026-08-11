# M83 — Map economy: 4-floor completions pay map pieces (and IOUs)

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Implemented 2026-08-06 on the
post-M82 checkout (`53ab02f`).

## A. Status

**☑ complete (approved)** — implemented 2026-08-06; approved and
committed by the owner 2026-08-06 (`d466e5b`). Evidence in §F.

## B. Goal (owner brief)

Completing a 4-floor dungeon in town 2 or later has a rising chance to pay
a puzzle map piece. While the party already holds all four pieces, earned
drops are banked as "owed" (max 3) and paid out after the Map-boss
(treasure-dig guardian) falls — confirmed flow, owner Q&A 2026-08-05.

## C. What was built

All the pure rules live in `game/TreasureMap.hpp` beside the M65 machinery
they extend (headless-tested; the states only speak).

- **`mapDropChancePct(town, bonusPct = 0)`** — the owner's table exactly:
  0 below town 2, then **15 / 27 / 39 / 51 / 63 / 75 %** for towns 2–7.
  `bonusPct` is the M84 town-perk hook (+5 % points at T2/T5), inert at 0;
  the base caps at 75, the total at 100, and no bonus opens town 1.
- **`mapDropRolls(runSeed, town, bonusPct = 0)`** — a pure hash of the
  **run seed** (`blackMarketHash`, fresh salt): committed the moment the
  run is entered, so reloading the entry autosave replays the same
  outcome. Statistically proven near its table (±4 pts over 4000 fixed
  seeds).
- **`grantMapPiece(...)`** — the M65 grant rule **extracted and shared**:
  one piece joins the pouch; the FOURTH converts into the reveal (town /
  seeded roster guard / the yielding run's boss scale) and the pouch
  resets. `DungeonState::takeMapPiece` (the in-dungeon pickup) was
  refactored through it — behavior-identical — so the two grant paths can
  never drift.
- **The IOU bank** — `Party::mapPiecesOwed` (new optional save field,
  0–3, defensive clamp on load): while a treasure stands revealed (the
  precise meaning of "holds all 4" — the fourth piece converts
  immediately, so pieces never sit at 4), a successful completion roll
  **banks** via `bankMapDebt` (cap 3; a roll beyond a full ledger is lost
  and the result line says so). After the dig guardian falls,
  `payMapDebt` pays the IOUs into the pouch — **clamped so the pouch
  never exceeds 3** (a fourth piece must arrive with a run's context to
  fire a reveal); anything that does not fit **stays banked** for the
  next dig rather than vanishing.
- **Wiring**: `completeDungeon` rolls only for `floorCount >= 4` in town
  ≥ 2 and hands the one-line story to **DungeonResultState** (new
  optional `mapLine` ctor param, drawn gold below the boss drops, wrapped
  ≤2 lines and counted in the panel's height math);
  `TreasureFightState::finish` appends the payout sentence to the dig
  result; the **Maps screen** states the standing debt ("The guild owes
  2 pieces, payable after the dig.").

## D. Schema, save & version implications

- New optional save field `mapPiecesOwed` (int 0–3, default 0, clamped on
  load — tampering degrades, never crashes or overpays). No
  rules/generation/score version motion: the roll is new logic on the
  M82 completion event, not a change to what a seed generates.

## E. Deviations & decisions

1. **Payout clamps instead of overfilling** (the note said "paid out
   immediately"; unqualified that could stack the pouch past 3 when
   pieces were also collected during the reveal window, and the next
   pickup would then silently eat the excess). Rule shipped: pay what
   fits, keep the rest banked for the next dig — no IOU is ever lost at
   payout. The only loss point remains the owner's own cap rule (a roll
   against a full 3-IOU ledger), and the result line says it happened.
2. The in-dungeon pickup on a 4-floor run still keys its guard off the
   FLOOR's seed (each floor is its own dungeon — M82 §E.2); the
   completion drop keys off the RUN seed. Both deterministic and
   reload-proof.

## F. Automated validation (evidence)

- Debug build: **passed** (VS2022 dev shell).
- `[mapdrop]` suite (new, 5 cases, 58 assertions): the exact chance
  table + bonus/clamp behavior; roll commitment + the ±4 pt statistical
  band; the shared grant firing the reveal on the fourth piece (seeded
  guard verified against the roster); bank cap / clamped payout / the
  full-pouch standstill; save round-trip incl. a hand-tampered value
  clamping to 3.
- Full Debug suite: **698/698 passed**. Release build + suite:
  **694/694 passed**. Capture sweep: **91/91 scenes clean**, zero
  overflow — `91_result_map` (the fullest breakdown + drops + the
  longest banked-IOU line, wrapped gold below the drops) and
  `80_puzzle_map` (now carrying the guild-debt sentence) both visually
  spot-checked.

## G. Owner manual validation

1. Clear 4-floor dungeons across towns 2+ until a piece drops — the gold
   line on the result screen is the announcement; judge whether the drop
   pacing feels right for the ladder (the real §I question).
2. Fill the map (any mix of in-dungeon finds and completion drops),
   then — with the treasure revealed — clear another 4-floor run until a
   drop banks; check the Maps screen states the debt.
3. Dig. The guardian falls, the IOU pays out in the result text, the Maps
   screen shows the pieces in the pouch.
4. Confirm a 1-floor run and a town-1 4-floor run never pay.
5. Reload the entry autosave after a 4-floor clear and re-clear:
   the same drop outcome (the committed-roll proof, played by hand).

## H. Known limitations

- The +5 % perk point exists as an argument, not a consumer (M84 wires
  it).
- The result screen announces the drop; the celebration (M71) does not
  mention it — deliberate, the celebration is about the score.
- A roll against a full ledger is lost by owner rule; the line says so,
  but nothing prevents it (that is the cap's meaning).

## I. Final status

`complete (approved)` — owner approval 2026-08-06, committed as
`d466e5b`.
