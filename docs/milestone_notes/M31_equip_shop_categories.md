# M31 — Equip-Shop Category Split

## A. Status and authority

- **Status:** complete (approved) — approved by the owner 2026-07-21 after
  manual testing. Authored / re-audited 2026-07-21 against the then-current
  checkout (HEAD `4be4d43`, working tree clean).
  As-implemented record in §K.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. This is the first
  milestone of the **M31–M34 expansion program** (ledger + program section in
  `docs/milestones.md`; direction in `docs/completion_roadmap.md` §13).
- **Authorization:** the owner authorized the expansion program and beginning
  M31 on 2026-07-20/21. M30 is `complete (approved)` (2026-07-20), so the
  program's opener may proceed. M32–M34 are **direction only** and must not be
  started without explicit owner go-ahead.

## B. Problem statement (verified at re-audit, HEAD `4be4d43`)

- **One flat buy list.** `EquipShopState` is a phase machine
  `Phase { Menu, Buy, EquipChar, EquipSlot, EquipItem }`
  ([EquipShopState.hpp:26](src/states/EquipShopState.hpp:26)). `Phase::Buy`
  lists **every** equippable item — `isEquippable` accepts
  `ItemType::Equipment` **and** `ItemType::Relic`
  ([EquipShopState.cpp:84](src/states/EquipShopState.cpp:84)) — sorted by id
  ([EquipShopState.cpp:105](src/states/EquipShopState.cpp:105)). The shipped
  roster is **25** equippable pieces (8 weapons, 8 armor, 5 accessories, 4
  relics — [data/items.json](data/items.json)), and the manual-test matrix
  already flags the buy list as an overflow/scroll risk (rows 14–15,
  [docs/manual_test_matrix.md](docs/manual_test_matrix.md)). As the roster grows
  across M34 (legendaries) a single 25→30+ item list is hard to browse.
- **Slots already exist.** `EquipSlot { None, Weapon, Armor, Accessory }`
  ([Enums.hpp:33](src/content/Enums.hpp:33)); every relic in the data carries
  `"slot": "accessory"` ([items.json:37](data/items.json:37)), so a slot filter
  files relics under Accessories **for free** — no data change, no special case.
- **The equip flow already filters by slot.** `Phase::EquipItem` builds its list
  with `it->slot == slotEnum(selectedSlot_)`
  ([EquipShopState.cpp:146](src/states/EquipShopState.cpp:146)); the buy split
  reuses exactly that predicate, so the two lists stay consistent.
- **Capture coverage.** `09_equip_shop` pushes `EquipShopState`, which opens in
  `Phase::Menu` ([CaptureRunner.cpp:312](src/capture/CaptureRunner.cpp:312)); no
  scene exercises a buy list. The overflow check therefore never sees a filled
  gear list. 23 capture scenes at baseline.
- **Lint.** `test_presentation_lint.cpp` checks item names/descriptions fit but
  has no equip-shop menu logic to guard (menu build is inside the raylib-linked
  state). Baseline 279 tests.

## C. Goals

- Split the single "Buy Gear" list into **Buy Weapons / Buy Armor / Buy
  Accessories**, with **relics under Accessories**, so each list is short and
  browsable and stays that way as the roster grows.
- **Zero risk to everything else:** no pricing, stock, equip-flow, content, save,
  scoreboard, generation, or battle change. Cancel remains a single, predictable
  step back at every level.
- Make the category filter **pure and headless-testable**, add a **capture
  scene** for the new category menu, and keep the full suite green from the 279
  baseline with **battle and simulator tests unmodified**.

## D. Material design decisions

No owner-gated forks — this milestone changes no player-facing rule, schema, or
dependency, and adds no architecture. The decisions below are routine,
reversible, and documented per `CLAUDE.md`'s minor-details clause.

### D.1 — Menu shape → *a `BuyCategory` phase, not three top-menu rows*

The top menu keeps its three rows (`Buy Gear`, `Equip Party`, `Back`).
Selecting **Buy Gear** enters a new `Phase::BuyCategory` listing **Weapons /
Armor / Accessories**; selecting one sets a slot filter and enters the existing
`Phase::Buy`. Rationale: the plan specifies "insert a `BuyCategory` phase
feeding a filtered `Phase::Buy`", and a dedicated category step keeps the top
menu stable, keeps Cancel one-step-per-level (Buy → Category → Menu → pop), and
leaves room for a future "all gear" affordance without reworking the top menu.
Each category row stays enabled even when empty; an empty filtered `Buy` list
is not reachable with the shipped roster, and a disabled row would hide a
category rather than show it empty.

### D.2 — Filter location → *a pure free function in a raylib-free header*

The predicate "equippable items of slot S, sorted by id" moves into a pure
function `equipShopBuyIds(content, slot)` in a small raylib-free header
(`states/EquipShopFilter.hpp`) so it is unit-testable headlessly (the state
class itself pulls in raylib via render). `Phase::Buy` calls it; the test calls
it directly against the shipped `ContentDatabase`. This is the M31 analogue of
how `restCost` was extracted to be testable in M30.

### D.3 — Category labels & hints → *slot names reused verbatim*

The category rows use the existing `kSlotNames` ("Weapon"/"Armor"/"Accessory")
pluralised to "Weapons"/"Armor"/"Accessories". The Buy-phase hint gains the
category name so the player always knows which list they are in.

## E. Proposed design

- **`states/EquipShopFilter.hpp`** — `std::vector<std::string>
  equipShopBuyIds(const content::ContentDatabase&, content::EquipSlot)` returns
  the sorted ids of equippable items (`Equipment` or `Relic`) whose `slot`
  matches. Raylib-free; reused by the state and the test.
- **`EquipShopState`** — insert `Phase::BuyCategory` between `Menu` and `Buy`.
  `rebuild()` gains a `BuyCategory` case (3 rows) and its `Buy` case calls
  `equipShopBuyIds(content, buyCategory_)` instead of scanning all items.
  `buyCategory_` (an `EquipSlot`) is set on category selection. `confirm()` and
  the Cancel handler gain the extra step; Cancel walks Buy → BuyCategory →
  Menu → pop. Buy-phase hint/footer include the category name. Nothing about
  pricing, stock, or the equip flow (`EquipChar/EquipSlot/EquipItem`) changes.
- **Capture** — a capture-only `captureEnterBuyCategory()` hook (mirroring
  `BattleState::captureEnterTargeting()`, `#ifdef CRYSTAL_CAPTURE`) drives the
  state into `Phase::BuyCategory`; a new scene renders it so the category menu
  is overflow-checked. (The filled per-category Buy list is short — 8/8/9 rows —
  and already within the list viewport that `09_equip_shop`'s roster feeds; the
  category menu is the new UI surface, so it is the scene that matters.)
- **Tests** — `test_equip_shop_filter.cpp`: each slot returns exactly its
  items against the shipped data (weapons/armor all `Equipment`; accessories
  include the 4 relics and exclude weapons/armor), the list is sorted, and a
  consumable/scroll never appears. A guard that the three category counts sum to
  the total equippable count (no item lost or double-listed).

## F. Determinism & save compatibility

- **No save/scoreboard/settings field added or changed.** Nothing to migrate.
- **No content schema change.** `items.json` is untouched.
- **`kGenerationVersion` unchanged (6); `battleRulesVersion` unchanged (1).**
  This milestone touches only town-service UI. Battle and simulator behaviour is
  byte-identical, so those tests pass **unmodified** (a hard constraint).
- **Capture:** 23 → 24 scenes (one added). Overflow policy unchanged.

## G. Out of scope

Pricing, stock, or affordability changes; any equip-flow change; new gear or
content; the town ladder / stakes / black market (M32–M34); any art (the shop
reuses its M27 `bg.equip_shop` background). A search/sort/filter-by-stat UI is
explicitly not built — categories are the whole scope.

## H. Tests

- **New `[equipshop]` unit tests** over the pure filter (§E), against the real
  shipped `data/`.
- **Full Catch2 suite** green from the 279 baseline (grows by the new file's
  cases); **battle and simulator tests unmodified**.
- **`--capture`** overflow-clean at 24/24 with the new category-menu scene.
- **Presentation lint** green (unchanged; the item-name/description checks still
  hold).

## I. Manual validation for the owner

Category-menu navigation feel with keyboard and gamepad: Buy Gear → pick a
category → browse → Cancel steps back to the category list → Cancel again to the
top menu; relics appear under Accessories and nowhere else; the Buy hint names
the current category; gold and Compare/Details still behave as before.

## J. Required documentation updates on completion

This note (as-implemented + deviations); `docs/milestones.md` (M31 status →
`implemented, awaiting manual approval`, as-implemented summary); the M31 program
section already added; `docs/technical_design.md` (§12 equipment — the
`EquipShopState` category step + `EquipShopFilter` note; §UI states list if it
enumerates phases); `docs/game_design.md` if it describes the equip shop flow
(it does not, beyond "gear comparison in the equip shop" — check and leave if
unaffected); `docs/manual_test_matrix.md` (equip-shop rows updated for the
category flow + a category-menu row).

## K. As-implemented record (2026-07-21)

- **Status:** complete (approved 2026-07-21). No owner-gated decisions
  arose (§D); the routine choices there were taken as written.
- **Pure filter.** New raylib-free header `src/states/EquipShopFilter.hpp`:
  `isEquippableItem(ItemDef)` (Equipment or Relic) and `equipShopBuyIds(db,
  slot)` (sorted ids of equippable items in `slot`). `EquipShopState` now uses
  both; the old anonymous-namespace `isEquippable` was removed so the state and
  the equip flow share one definition.
- **Phase machine.** `Phase` gained `BuyCategory` between `Menu` and `Buy`.
  "Buy Gear" → `BuyCategory` (rows Weapons / Armor / Accessories) → sets
  `buyCategory_` (an `EquipSlot`) → filtered `Buy`. Cancel walks
  `Buy → BuyCategory → Menu → pop`, one step per level. The Buy hint names the
  active category ("Buy Weapons   Confirm: Buy   Cancel: Back"); Compare/Details
  and the Gold display are unchanged. The equip flow
  (`EquipChar/EquipSlot/EquipItem`) is untouched.
- **onEnter → constructor.** Initial `rebuild()` moved from `onEnter` (removed)
  into the constructor, so the capture hook can force a phase that `onEnter`
  would otherwise reset — mirroring `BattleState`'s capture pattern. Behaviour
  for normal play is identical: `phase_` already defaults to `Menu`, and
  `onEnter` only ever fires on push (pops call `onResume`, a no-op here).
- **Capture.** Capture-only `EquipShopState::captureEnterBuyCategory()`
  (`#ifdef CRYSTAL_CAPTURE`) plus a new `24_equip_categories` scene in
  `CaptureRunner.cpp` render the category menu for the overflow check.
- **Tests.** New `tests/test_equip_shop_filter.cpp` (`[equipshop]`, registered
  in `tests/CMakeLists.txt`): the shipped roster partitions cleanly into the
  three categories (8 weapons / 8 armor / 9 accessories = 25 equippable, none
  dropped or double-listed), every id lands only in its slot's category, the 4
  relics file under Accessories and nowhere else, and hand-built consumables/
  scrolls are never buyable gear.
- **Validation (this session, VS 2022 / MSVC 14.44, `build-msvc`).** Debug build
  clean (exe + tests + capture); full suite **283/283** (`crystal_tests.exe`,
  30680 assertions), incl. the 4 new `[equipshop]` cases — battle and simulator
  tests **unmodified and green**. `--capture` **24/24** overflow-clean (new
  `24_equip_categories` scene verified rendering Weapons/Armor/Accessories).
  Release build clean (capture correctly excluded). Presentation lint green.

### Deviations from the plan / note

1. **`onEnter` was removed, not merely edited** (init moved to the constructor)
   so the capture hook survives the push lifecycle — a routine local decision,
   behaviour-preserving for normal play (§K bullet above). Not a scope change.
2. **The new capture scene is numbered `24_equip_categories`** (next free
   number) and placed after `09_equip_shop` in the scenario list, rather than
   renumbering the existing 23 scenes — the established convention (scene `23`
   was likewise appended out of order).
