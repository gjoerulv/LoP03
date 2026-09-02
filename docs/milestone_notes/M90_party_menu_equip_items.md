# M90 — Party menu: Equip Party & Items

**Status:** complete (approved 2026-08-16)
**Program:** M88–M97 (authorized 2026-08-14). No version bumps; no
save/schema changes.

## Scope (owner item 10)

Both pause menus gain **"Equip Party"** (the equip-shop flow with no
"Equip shop" label) and **"Items"** (inspect and use healing items outside
battle; all items visible incl. equipment; maps only on the Maps screen).
The owner acknowledged this softens the original in-battle-only design —
a deliberate call; M23 playtests judge the economy.

## What was built

- **Equip Party:** `EquipShopState` gained a `partyMode` ctor flag — the
  M31 phase machine reused whole (member → slot → item, stat-diff colours,
  M79 cycling, M81 icons): opens in EquipChar, shop phases unreachable,
  Cancel leaves, plain canvas + "Equip Party" header. The plan sketched a
  component extraction; the flag achieves the same reuse with zero
  duplication and far less churn (deviation, routine).
- **Items:** new `InventoryState` — the bag in three bands (consumables in
  the M88 shelf order → gear → scrolls), counts + icons, two-line detail
  preview, member picker with live HP/MP columns. Use rules in the pure
  `game/ItemUse.hpp`: M43 gating out of battle (heal/MP only living with
  room, revive only fallen at the authored %, refusal reasons — never a
  wasted use); **cures refuse outright** (statuses are battle-scoped;
  Characters carry none — recorded design point). Spends on success only.
  Gear/scroll/Royal-Relic rows refuse with pointers to Equip Party / the
  Party panel / battle. Map pieces/curios never enter the bag; a footer
  line points at Maps.
- **Pause menus:** Town menu now 9 rows (18px rows so the box + debug row
  + plaque clear the 240px frame), dungeon menu 7 rows. Synergy notes: the
  M89 carry-out is answerable from the road (Phoenix Tear via Items), and
  the M88 team inspection pairs with mid-run re-gearing.

## Compatibility

Saves, settings, schemas, seeds, scores: untouched. No battle-rules
change (out-of-battle use never touches the Simulator).

## Automated validation

- Debug + Release builds clean (VS2022 shell); full suite **753/753**
  (new: `[itemuse]` gating/application cases in `test_inventory.cpp`).
- Capture **109/109** clean, incl. new `108_items_bag` (seeded bag,
  longest description) and `109_equip_party`.

## Manual owner checklist

Matrix rows **173–174**. Feel judgments: pause-menu density (9 rows),
whether out-of-battle healing feels right for the economy, Equip Party in
a dungeon with a just-inspected team.

## Known limitations

- Royal Relics show in the bag as battle-use rows (by design).
- The Items screen offers no bulk use; one sip per Confirm.

## Documentation updated

`docs/milestones.md` (M90 row) · `docs/game_design.md` (§5 M90 paragraph)
· `docs/technical_design.md` (§43) · `docs/manual_test_matrix.md`
(rows 173–174).

## Final status

`implemented, awaiting manual approval`
