# M54 — Arms of the ladder (equipment rebalance)

> Program: **Adjustment program (M53–M56)**, authorized 2026-07-24. Second
> milestone; runs after M53 so its buffed prices are the baseline M55's events
> are tuned against.
>
> Authority: `CLAUDE.md` > this note > `docs/milestones.md` > `docs/game_design.md`.
> §J is the as-implemented record.

## Goal

A substantial, **owner-approved** buff to non-trivial equipment so gear feels
like it climbs with the seven-town ladder. **`data/items.json` only** — no schema
change, no code change, no version bumps.

## The rule (owner-approved)

For non-legendary equipment, scale factor `f = 1 + (value − 80) / 1370` (weakest
non-legendary, `travel_cloak`@80g → ×1.0; strongest non-legendary, the 1450g
town-7 epics → ×2.0). Legendary `f = 2.5`. Applied to **positive
ATK/MAG/DEF/SPD** and to **price** (rounded to the nearest 10).

**Never scaled:** HP bonuses (owner rule), stat penalties (doubling a drawback is
not "stronger"), pure-HP items (fully untouched, price included), and legendary
prices (never shop-sold; the black-market price is a separate flat formula kept
at 5000–9000). The ten weakest pieces are unchanged.

The plan's spreadsheet is authoritative and was verified two ways before applying:
every "old" value matches the current `data/items.json`, and the "new" values are
internally consistent with the formula (spot-checked across ~20 rows). **Applied
exactly as the spreadsheet lists** (owner instruction: "implement it exactly").

## ⚠ SPD note for the owner (from the plan)

SPD scales with the same factor (only HP was exempted), which lifts top-end speed
(Swiftwind Ring +7→+10, Kingsbane +3→+8, Regalia +4→+10). If the balance battery
shows turn-order dominance distorting fights, the fallback is half-rate SPD
scaling — this would be escalated with data before deviating. §J reports what the
batteries showed.

## Scope of edits

Changed (stat and/or price): 22 weapons, 11 armor, 15 accessories. Unchanged (the
ten weakest / pure-HP): `iron_sword`, `short_bow`, `oak_staff`, `travel_cloak`,
`leather_armor`, `mage_robe`, `oak_shield`, `vitality_charm`, `titan_heart`,
`vigor_pendant`. Four accessories are **price-only** (`swift_boots`,
`guard_pendant`, `power_ring`, `mana_band`): their stat rounded back to itself, so
only the price moved.

## Verification plan

- The rarity-ordering assertions in `tests/test_balance.cpp` (legendary > epic on
  ATK/DEF/maxHp) hold by construction of the table.
- The two difficulty-tuned cases — town-7 clear rate and "legendaries don't
  trivialize" — are **re-run and reported** (§J), plus the full balance/economy
  batteries (`[balance]`, `[economy-report]`, `[castle-report]`, `[king-report]`).
  Enemies are **not** silently retuned to compensate; any shift is reported and
  escalated if a case breaks.
- Capture: equip-shop scenes re-verified (wider prices like "2900g" in the buy
  list's `x%-3d%5dg` column; the Gear Details overlay with the biggest numbers).

## Documents to update

`docs/game_design.md` (rebalance rationale + table pointer), `docs/milestones.md`
(status), and this note. `data/items.json` is the content of record.

## Acceptance criteria

`data/items.json` matches the approved spreadsheet exactly; HP bonuses, penalties,
pure-HP items, and legendary prices untouched; balance/economy batteries re-run
with before/after reported; full suite green from 505 (Debug) / 501 (Release),
capture clean; status set to `implemented, awaiting manual approval`.

## J. As-implemented record

Implemented 2026-07-24 on base checkout `07a13bb` + the M53 changes. **Content
only** (`data/items.json`) — no schema, no code (except one test correction,
below), **no version bumps**.

**Applied exactly.** All 48 changed pieces (22 weapons, 11 armor, 15
accessories) now match the plan's "new" column; re-read and verified line by
line. HP bonuses (12/20/25/30/40/90), stat penalties (war_hammer/plate_armor
SPD −2), pure-HP items (vitality_charm, titan_heart, vigor_pendant), and
legendary prices (5000/6000/6500/9000) are untouched, as are the ten weakest
pieces. Consumables and scrolls are untouched.

### ⚠ Balance shift for the owner (reported, not silently absorbed)

The doubled endgame gear makes the Hollow King **substantially more beatable by
the scripted sim**. The `[king-report]` battery (maxed party at the shipped
500 % King) before vs after M54:

| Counterplay | Pre-M54 turns/outcome | Post-M54 turns/outcome |
|---|---|---|
| nothing (unaided) | 18 / **loss** | 21 / **loss** |
| 30 snacks only | 25 / loss | 18 / **win** (3 alive) |
| 1 sheet + 1 goose + 20 snacks ("plan") | 22 / loss | 15 / **win** (2 alive) |
| 2 + 2 + 30 snacks | 26 / loss | 19 / **win** |
| 3 + 3 + 30 snacks | 21 / win | 20 / win |
| + Crown | 20 / win | 18 / win |
| + Crown + Spoon (max) | 12 / win | 11 / win |

A fully **unaided** maxed party still loses, but a *modest* counterplay that used
to lose now wins. This is the direct, foreseeable consequence of an
**owner-approved** ×2/×2.5 gear buff, not a bug or an enemy change. Per the plan
("report shifts and escalate if a case breaks") and the owner's instruction
("implement it exactly, and note the SPD flag"), it is **reported here for the
owner's judgment at manual approval** rather than compensated for by retuning
enemies. King winnability remains an owner manual-validation item (matrix row
125). **Owner decision point:** is the King's lowered preparation bar acceptable,
or should the buff be tempered (e.g. the plan's half-rate SPD fallback, or a
King re-scale under a future milestone)?

**No turn-order dominance.** The SPD half of the buff (Swiftwind +10, Kingsbane
+8, Regalia +10) did **not** produce degenerate blowouts — post-buff wins take
11–21 rounds, not sub-10 turn-order stomps — so the plan's half-rate-SPD fallback
trigger ("turn-order dominance distorting fights") did **not** fire. The shift is
broad power (ATK/MAG damage + DEF survivability + more turns), not a speed
exploit.

**One test correction (not an enemy retune).** `tests/test_royal_relics.cpp`'s
`relics: the counterplay measurably changes the King fight`
(`[relics][king][balance]`) asserted `aided.rounds > unaided.rounds` — valid only
while *both* fights were losses (so "matters" = "survives longer"). The buff
turns the aided fight into a win, so that inequality (15 > 21) is false while the
intent is satisfied *more* strongly (a loss became a win). The assertion was
updated to the durable intent — the counterplay wins outright OR (if a future
retune makes both lose again) lasts strictly longer — with a comment recording
the shift. No enemy or gear value was changed to make it pass.

**Batteries re-run and reported.**
- `[balance]` → **15/15 test cases pass** (78 assertions), including the two
  plan-named tuned cases: town-7 clear rate and "legendaries don't trivialize"
  both still hold. Rarity-ordering (legendary > epic on ATK/DEF/maxHp) holds by
  construction.
- `[economy-report]` → **unchanged** by M54: it measures consumable affordability
  from clear gold and drop chances, neither of which the gear rebalance touches.
  The only economic effect is a larger equipment gold sink (top-end gear ~doubled
  in price), which is a reasonable sink for doubled-power gear, not a distortion.
- `[king-report]` / `[castle-report]` → the King table above; the castle-report
  reflects the same broad party power increase.

**Validation (this session).**
- Build: `cmake --build --preset debug` / `--preset release` — OK, no warnings.
- Debug tests: `ctest --preset debug` → **505/505 passed**.
- Release tests: `ctest --preset release` → **501/501 passed**.
- Capture: `CrystalDungeons.exe --capture` → **65/65 scenes clean** with the new
  prices (the equip-shop scenes carry the widened values, e.g. 2900g).
