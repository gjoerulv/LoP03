# M78 — Inventory caps & shop UX

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Re-audit this note against the
then-current checkout before implementation begins.

## A. Status

☐ planned

## B. Goal (owner brief)

Consumables get held-quantity caps enforced at purchase, the premium tonics
leave the town shelves, and the item shop's owned-count column finally
lines up.

## C. Scope (planned)

### Cap model

- Per-item caps: **Potion 9, Hi-Potion 6, every other consumable 2**.
  Equipment/relics/scrolls exempt (effectively the old 99 — "redundant" per
  the owner).
- A cap helper beside `game/Inventory` exposing
  `capFor(itemId)` / `canBuy(inventory, itemId)`; checks are **`>=` at
  purchase time only**. A party already holding more than a cap (older
  saves, rewards) is **never clamped** — it is simply treated as at-max.
- Perk hook: `+capBonus` per rank (consumed by M84's town milestones) with
  a **hard ceiling of 9** on every consumable cap — Potion stays 9
  (owner decision 2026-08-05).
- Non-purchase acquisition (chests, events, rewards, relics) stays
  ungated — only buying is blocked.

### Purchase gates & stock

- Gates wired in the town item shop (`src/states/ItemShopState.cpp`), the
  in-dungeon merchant event, and audited across every other buy path
  (the black market sells gear — confirm no consumable path; the equip shop
  sells equipment only). At-cap rows show as such (disabled/marked) with an
  honest message on attempt.
- **Elixir & Hi-Ether delisted from town item shops** (the
  `itemShopBuyIds` window). The **in-dungeon merchant may still offer
  them, but at full value** — exempt from its usual pricing (owner decision
  2026-08-05).

### Item shop alignment fix

- The owned-count/price suffix (`TextFormat("x%-3d%5dg")`,
  `ItemShopState.cpp:48`, drawn via `drawMenuScrolled`'s suffix path in
  `src/ui/UiDraw.cpp`) is re-laid so the quantity column aligns vertically
  (fixed columns, consistent baseline for the smaller suffix font). The
  equip shop uses the same suffix shape — keep both consistent.

## D. Schema, save & version implications

- No rules/generation/save version motion. No content-schema change
  expected (caps are code policy keyed by item type/id); if a data-driven
  `maxHeld` field proves cleaner (the Evil Duckling already wants max 1),
  it lands as an optional validated field and is noted here.

## E. Out of scope

The town-milestone perk itself (M84 — only the `capBonus` hook lands here);
CrystalForge descriptors for any new field (M86).

## F. Dependencies

None hard; M76's Evil Duckling (max 1) simply plugs into the same helper.

## G. Acceptance criteria

- Buying is blocked at cap with a clear message in every shop, including
  the dungeon merchant; nothing ever deletes an overage.
- Elixir/Hi-Ether unbuyable in town; merchant sells them at exactly full
  value while its other wares keep the current pricing.
- The owned-count column is visually aligned in both shops (capture).

## H. Automated validation

Cap math + gate unit tests (at-cap, over-cap tolerated, bonus ceiling 9);
stock-window tests (tonics absent in town, priced full at the merchant);
capture lint over the shop scenes. Full suite green.

## I. Owner manual validation

Buy to each cap in a town shop and at a dungeon merchant; load an old save
holding more than a cap and confirm nothing is removed; eyeball the aligned
columns at both shops.
