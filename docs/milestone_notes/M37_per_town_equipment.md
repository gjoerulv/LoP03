# M37 — Per-Town Equipment

## A. Status and authority

- **Status:** complete (approved) — approved by the owner 2026-07-21 after manual
  testing; committed. Authored / re-audited and implemented 2026-07-21 against HEAD
  `98a11e0` ("M36"). As-implemented record in §K.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. Third milestone of the
  M35–M42 endgame program. First milestone that changes **generation** (not battle
  rules): `dungeon::kGenerationVersion` **6 → 7**.

## B. Problem statement (verified at re-audit, HEAD `98a11e0`)

- **Gear does not track the ladder.** Every equip-shop item is available from
  town 1; the seven-town difficulty climb (enemy stats +0…+200 %) has no matching
  gear progression. `equipShopBuyIds(content, slot)`
  ([EquipShopFilter.hpp:31](src/states/EquipShopFilter.hpp:31)) is a pure slot
  filter over the whole non-legendary roster — the single place a town gate slots
  in. `ItemDef` ([Definitions.hpp:107](src/content/Definitions.hpp)) has
  `rarity/slot/value/statBonus`; adding `minTown` (default 1) is the established
  optional-field pattern.
- **The dungeon merchant is a tax, not a reward.** The Merchant room event prices
  its item at `value * 130 / 100`
  ([DungeonGenerator.cpp:466](src/dungeon/DungeonGenerator.cpp:466)) — a markup.
  The owner decision is **75 % of value** (a discount that rewards exploration).
- **Generation is town-aware but pools are not.** `generate(seed, depth, db,
  themeId, town)` and `makeTeam(..., town)` already thread `town`; `buildPools`
  ([DungeonGenerator.cpp:33](src/dungeon/DungeonGenerator.cpp:33)) builds `items`
  (all non-legendary) and `consumables` **without** `town`, and `makeChest` draws
  a reward from `items`. Gating chest gear by town = filtering `items` by
  `minTown ≤ town` (one new `town` parameter to `buildPools`). Legendaries are
  already excluded from chests/merchant (M34) and the equip shop (M34).
- **The merchant price is baked into the generated `RoomEvent.goldCost`**, so
  changing the formula changes generated output for the same seed. Both the
  discount and the town-gated chest pool ride **one** `kGenerationVersion` bump.
  This means **town-1 generation is no longer byte-identical to M36** (the
  merchant price changed) — expected, and covered by the version tag; the M32
  byte-identical invariant is superseded by the owner-approved bump.
- **Shop UI** is category-split (M31) and scrolling (M12); the larger stock needs
  a max-stock capture check. **Baseline:** 334/334 tests, 29/29 capture,
  `kGenerationVersion` 6, `kBattleRulesVersion` 3, `kSaveVersion` unchanged.

## C. Goals

- `ItemDef.minTown` (default 1); `equipShopBuyIds` gains a `town` parameter; the
  Equip Shop stocks only `minTown ≤ currentTown`.
- **Content:** new per-town equipment for towns 2–7 with a sim-validated
  price/power curve, plus **≥ 3 legendary weapons** (black-market/drop pool only —
  they feed M39's drops).
- **Merchant discount:** Merchant event price `value * 75 / 100`; prompt/docs
  updated.
- **Chest pools:** the new town-gated gear enters chest candidates gated by the
  dungeon's town (**note-time decision: yes** — the bump is cheap and displayed).
- `dungeon::kGenerationVersion` **6 → 7** (merchant discount + town-gated chest
  pool); the scoreboard already tags it.

## D. Owner decisions & routine choices

Locked at program planning (not re-opened): the **merchant discount = 75 % of
value**; the generation bump to 7 if new items enter chest pools; per-town gear
shape (3–5 pieces per town, ≥ 3 legendary weapons).

Note-time / routine choices taken here (owner validates feel/balance at approval):

- **Chests include the new gear (yes),** gated by the dungeon's town — so the
  merchant discount and the town-gated chest pool ride one bump (the plan's
  recommendation). Flagged for owner veto; declining would make the new gear
  shop-only and still require the bump for the merchant discount.
- **Roster (24 regular + 3 legendary):** 4 pieces per town for towns 2–7 — a
  physical weapon, a magic weapon, an armor, and an accessory each — so every
  character type gets a rung at every town. Power rises with town but **stays
  below the legendary tier** (so M34's legendaries remain the peak); prices rise
  with town as a soft gate, sim-validated for affordability against depth-driven
  income (gold does not scale with town, M32). Rarity: towns 2–3 `rare`, towns
  4–7 `epic`. **3 new legendary weapons** (`rarity: legendary`, `minTown` 1) a
  clear step above the epic town-7 gear and the existing M34 legendaries; they are
  black-market/drop-only (excluded from chests and the shop, as M34 established).
- **Legendary gating:** legendaries keep `minTown` 1 — the black market already
  gates its spawn to town ≥ 2 and M39 will gate drops to town ≥ 3; per-legendary
  town gating is out of scope here.
- **No battle/save change:** `minTown` is content; `kBattleRulesVersion` 3 and
  `kSaveVersion` unchanged. A stored gear id the content no longer knows is already
  dropped on load.

## E. Proposed design & slices

1. **Schema + filter.** `ItemDef.minTown` (parse `optIntMin("minTown", 1, 1)`);
   `equipShopBuyIds(content, slot, town)` filters `minTown ≤ town`; `EquipShopState`
   passes `currentTown`. Headless `[equipshop]` town-filter tests.
2. **Generation.** `buildPools(db, theme, town)` filters `items` by `minTown ≤
   town`; `generate` passes `townIdx`; Merchant price → `value * 75 / 100`;
   `kGenerationVersion` **6 → 7**. Determinism tests updated (same seed+town → same
   dungeon; the new gen version recorded).
3. **Content.** `items.json`: 24 per-town pieces (towns 2–7) + 3 legendary weapons,
   with the curve above; economy sim (`[economy-report]`) validates
   affordability/power; the town × depth clearability grid re-checked assuming the
   new gear is reachable.
4. **UI/docs.** Merchant prompt wording ("a bargain" not "markup"); a capture scene
   of the Equip Shop at max stock (town 7); `game_design.md`/`technical_design.md`/
   README updated.

## F. Determinism & save compatibility

- **`kGenerationVersion` 6 → 7** (owner-approved): the merchant discount and the
  town-gated chest pool change generated output. Same seed + town + version still
  reproduces the same dungeon; the scoreboard tags the version for comparability
  (M19). Town-1 output is **no longer** identical to M36 (merchant price) — a
  deliberate, versioned change.
- **No `kSaveVersion` bump.** `minTown` is content, not save. Old scoreboard
  entries read their stored `generationVersion`; entries without it are legacy.
- **No `kBattleRulesVersion` change** (gear stats fold into the existing derived
  stats; battle resolution is unchanged).

## G. Out of scope

Per-town enemies/bosses (M38); boss drops (M39); the castle (M40); any battle-rule
change; per-legendary town gating; a broader crafting/vendor system.

## H. Balance / acceptance

Each town stocks only its unlocked gear; the price/power curve is sim-validated
(affordable against depth-driven income, powerful enough to matter, below the
legendary tier); the merchant is now a reward (75 %); the town × depth grid stays
clearable with the new gear assumed; town-1/other determinism holds under the new
version; the equip shop at max stock is overflow-clean; the black-market
no-trivialization check still holds with the 3 new legendary weapons.

## I. Manual validation for the owner

Whether the per-town gear feels like a meaningful climb reward; whether prices
gate without grinding; whether the merchant discount reads as a bargain; equip
shop legibility at max stock.

## J. Required documentation updates on completion

This note (§K); `docs/milestones.md` (status); `docs/game_design.md` (per-town
gear + merchant discount); `docs/technical_design.md` (`ItemDef.minTown`,
`equipShopBuyIds` town param, `buildPools` town filter, merchant price,
`kGenerationVersion` 7); README (how-to-play gear line); `docs/manual_test_matrix.md`
rows; a manual checklist per `docs/milestone_completion_template.md`.

## K. As-implemented record (2026-07-21)

- **Status:** complete (approved 2026-07-21). Implemented / audited against
  HEAD `98a11e0`. The §D routine choices were taken as written (chests include the
  town-gated gear; 24 regular + 3 legendary pieces; per-town gear below the
  legendary tier; flat merchant 75 %).
- **Schema + filter.** `ItemDef.minTown` (parsed `optIntMin("minTown", 1, 1)`);
  `equipShopBuyIds(content, slot, town)` stocks only `minTown ≤ town`;
  `EquipShopState` passes `currentTown`.
- **Generation.** `buildPools(db, theme, town)` filters chest candidates by
  `minTown ≤ town` (legendaries still excluded); `generate` computes `townIdx`
  before pooling and passes it; the Merchant event price became
  `value * 75 / 100`; `dungeon::kGenerationVersion` **6 → 7**. Town-1 output is no
  longer identical to M36 (the merchant price changed) — a deliberate versioned
  change; the room-local seed folds the new version so all realizations shift too.
- **Content.** `items.json` gained **24 per-town pieces** (towns 2–7: a physical
  weapon, a magic weapon, an armor, an accessory each; towns 2–3 `rare`, 4–7
  `epic`; attack/magic/defense rising with town but staying below the legendary
  tier; prices rising as a soft gate) and **3 legendary weapons**
  (`worldbreaker_axe` atk 24, `voidpiercer_rod` mag 22, `kingsbane_spear` atk 22
  spd 3) — black-market/drop pool only.
- **Balance note (for the owner).** The M34 legendaries (dawnforged atk 20,
  stormcaller mag 18, aegis def 18) now sit only slightly above the town-7 epic
  ceiling (atk 18 / mag 17 / def 17). The new legendary weapons (atk 22–24)
  restore a clear top; the legendary **tier** (20–24) stays above the epic tier
  (≤ 18). Flagged for the owner's balance read; the M34 legendaries were left
  unchanged (their tuning is owner-approved).
- **UI/docs.** Merchant prompt reads as a bargain; a `30_equip_shop_max` capture
  scene shows the town-7 weapon buy list (max stock).
- **Validation (this session, VS 2022 / MSVC 14.44, `build-msvc`).** Debug build
  clean; full suite **336/336** (334 M36 baseline + 2 new `[equipshop]` town-gating
  cases; updated the item-count guard 41→68, the merchant-event assertion to the
  75 % price, and the legendary-vs-epic balance test to the new best-epic/best-
  legendary gear). `--capture` **30/30** overflow-clean (new max-stock scene).
  Release build + Release suite **336/336** clean. `[economy-report]` grid
  **unchanged** (1/4/3/3/5/8/8/10) — the sim party equips no per-town gear and the
  enemy roster is unchanged, so the merchant/chest/room-realization changes do not
  shift clearability; the black-market no-trivialization balance test still holds.

### Deviations from the plan / note

1. **Merchant/room-realization change touches town 1 too** (the discount applies
   at every town, and the `kGenerationVersion` bump reshuffles every room's
   realization) — so the M32 "town-1 byte-identical" property is intentionally
   retired under the owner-approved bump, not just for towns 2–7.
2. **The M34 legendaries were not re-tuned** despite the new epic ceiling nearing
   them; the 3 new legendary weapons provide the clear top instead (flagged
   above for owner veto).

### Known items for owner validation

- Whether the per-town gear feels like a meaningful climb reward and prices gate
  without grinding; whether the merchant bargain reads well; whether the M34
  legendaries should be nudged up now that epics reach atk 18 (or whether the new
  legendary weapons suffice).
