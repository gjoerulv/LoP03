# M78 — Inventory caps & shop UX

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Implemented 2026-08-06 on the
post-M77 checkout (`3b6d816`).

## A. Status

**☑ complete (approved)** — implemented 2026-08-06; approved and
committed by the owner 2026-08-06 (`97c74d2`). Evidence in §F.

## B. Goal (owner brief)

Consumables get held-quantity caps enforced at purchase, the premium tonics
leave the town shelves, and the item shop's owned-count column finally
lines up.

## C. As implemented

No rules/generation/save version motion. Two optional, validated content
fields; one pure helper header; gates at the two consumable tills; one
shared-UI column fix.

### The cap model (`src/game/ItemCaps.hpp`, pure)

- **Every consumable caps at 2 held**; authored exceptions ride the new
  optional `ItemDef::maxHeld` (1..9, consumables only): **Potion 9,
  Hi-Potion 6**. Equipment, relics and scrolls are uncapped.
- `capFor(def, capBonus)` / `canBuyMore(inventory, def, capBonus)`:
  **`>=` checks at purchase time only.** Chests, events, rewards and
  relics grant freely; a party already over a cap (an older save) is
  never clamped — it is simply refused another sale.
- The **`capBonus` hook** (M84's town-milestone perk): +1 per rank on
  every consumable cap, against a **hard ceiling of 9** — Potion simply
  stays 9. Nothing passes a bonus yet.
- The **Evil Duckling needs no cap row**: its only source, the Duckling
  Peddler, enforces one-per-customer at the till (M76) — duck rules
  outrank shop rules, and giving it a bonus-eligible cap would have
  contradicted them.

### Purchase gates

- **Town item shop** (`ItemShopState`): an at-cap row wears `x9 MAX` in
  its count column and stays selectable (so its description is still
  readable); attempting the buy refuses with "You cannot carry more of
  Potion (max 9)". The cap refusal outranks the gold refusal.
- **Dungeon merchant** (`DungeonState`): the same refusal — and it does
  NOT resolve the event, so the party can drink something and come back.
- **Audited, no other consumable till exists:** the equip shop sells
  equipment/relics (uncapped by design), the black market sells legendary
  gear only, the Armory Ghost / chests / relic events / boss drops /
  starting kit are grants, not purchases.

### The premium tonics

- New optional `ItemDef::notSoldInTown` (consumables only): **Elixir and
  Hi-Ether** carry it. `itemShopBuyIds` excludes them, so no town item
  shop stocks them at any town.
- The **in-dungeon merchant still offers them — at exactly full value**
  (`merchantPriceFor`): the M37 street discount (75%) keeps applying to
  everything else. Priced at INTERACTION time, so the generated
  `goldCost` — and with it generation determinism and
  `kGenerationVersion` — is untouched (the M76 peddler precedent, §E).
  The prompt says "(full price)" where it used to say "(dungeon
  prices)"; chest pools and the merchant's item pool are unchanged
  (`availableAtTown` untouched).

### The column fix

- `drawMenuScrolled` (shared UI): a suffix containing `\t` now renders as
  **two fixed right-aligned columns** (owned count | price), each sized
  to the menu's widest entry — measured over the whole menu, not the
  visible window, so scrolling cannot shift them. The old
  `x%-3d%5dg` space padding never aligned in a proportional font. Both
  the item shop and the equip shop's buy list use the tab shape;
  single-string suffixes elsewhere keep the old path bit-for-bit.

## D. Files changed

- **Content model:** `src/content/Definitions.hpp` (`maxHeld`,
  `notSoldInTown`), `src/content/ContentLoader.cpp` (read + the semantic
  rules), `src/editor/CategoryDescriptors.cpp` (two rows — see §E).
- **Game:** `src/game/ItemCaps.hpp` (new, pure).
- **States:** `src/states/ItemShopFilter.hpp` (delisting),
  `src/states/ItemShopState.cpp` (gate, MAX marker, tab suffix),
  `src/states/DungeonState.cpp` (merchant gate + pricing + prompt),
  `src/states/EquipShopState.cpp` (tab suffix).
- **UI:** `src/ui/UiDraw.cpp` (the two-column suffix path).
- **Data:** `data/items.json` (potion `maxHeld: 9`, hi_potion
  `maxHeld: 6`, elixir + hi_ether `notSoldInTown: true`).
- **Tests:** `tests/test_item_caps.cpp` (new, 7 cases, `[caps]`),
  `tests/CMakeLists.txt`.
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md`,
  `docs/technical_design.md` §32, `docs/manual_test_matrix.md`.

## E. Plan deviations

- **Merchant pricing is interaction-time, not generation-time.** The
  merchant's `goldCost` is baked into the generated dungeon; changing the
  formula for tonics would have made v14-labelled seeds generate
  different bytes with the program's one generation bump reserved for
  M82. The till therefore computes `merchantPriceFor` when you buy (and
  the prompt quotes it), leaving every generated dungeon byte-identical —
  the same reasoning as the M76 peddler, recorded here for the owner.
- **The editor descriptors landed now, not in M86.** The planned note
  deferred them, but the M59 descriptor-completeness sweep fails the
  suite the moment a loader key lacks its row — two rows, landed with the
  fields.
- **The planned "cap helper beside `game/Inventory`" became data-driven
  `maxHeld`** (the note's own anticipated alternative): Potion/Hi-Potion
  author their exceptions instead of the code branching on ids.
- The Evil Duckling deliberately carries no `maxHeld` (see §C) — the
  planned "max 1 held" is enforced by the peddler alone, as since M76.

## F. Automated validation (all run in this session, 2026-08-06)

- Debug build: clean, zero project-code warnings;
  `CrystalForge --canonicalize`: 0 rewritten, 0 errors (the data edits
  were authored canonical).
- New `[caps]` cases (7) green: cap math (defaults, exceptions, bonus
  ceiling, uncapped gear), `>=` purchase semantics with an untouched
  overage, merchant pricing, the loader rules (misplaced/over-ceiling
  fields rejected), the shipped exceptions, and the town delisting
  (elixir/hi_ether absent at every town; potion/ether untouched).
- **Full Debug suite: 661/661 tests green. Capture lint: 85/85 scenes
  clean. Release build + suite: 657/657 tests green.**

## G. Manual owner checklist

1. Town item shop: buy Remedies to 2 — the row shows `x2 MAX`, a third
   buy refuses politely; buy Potions toward 9 and Hi-Potions toward 6
   likewise. The count and price columns sit in clean vertical lines
   (equip shop too).
2. Elixir and Hi-Ether are gone from every town's item shop shelf.
3. In a dungeon, find a merchant ("M"): ordinary wares still show
   "(dungeon prices)"; an Elixir/Hi-Ether offer says "(full price)" and
   charges exactly 400g/500g. At cap, the merchant refuses but STAYS —
   drink one and come back.
4. Load a pre-M78 save holding piles of consumables: nothing is removed;
   over-cap stacks just refuse further buying.
5. On any failure: the shop, the item, the message shown, and a save.

## H. Known limitations

- No perk passes `capBonus` yet — the hook waits for M84.
- The battle/pause inventory list is untouched (it never showed prices);
  only the two shops draw the two-column suffix today.

## I. Final status

`complete (approved)` — owner approval 2026-08-06.
