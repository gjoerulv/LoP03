# M34 — Black Market & Legendary Gear

## A. Status and authority

- **Status:** in progress — authored / re-audited 2026-07-21 against HEAD
  `5b751e7` ("M33"). Owner authorized beginning M34 on 2026-07-21 after approving
  M33. As-implemented record in §K.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. **Final milestone of the
  M31–M34 expansion program.** Depends on M32 (town ladder) + M33 (its spawn
  trigger is M33's stakes-raise event). After this, M23 → M24 run, re-audited.

## B. Problem statement (verified at re-audit, HEAD `5b751e7`)

- **No rare, aspirational reward for optional risk + climbing.** `Rarity::Legendary`
  exists in the enum and loads through the pipeline
  ([ContentLoader.cpp:176](src/content/ContentLoader.cpp:176)), but **zero**
  legendary items ship. Optional elite fights (`RoomEventKind::EliteChallenge`,
  resolved as `EncounterKind::Challenge` at
  [DungeonState.cpp:524](src/states/DungeonState.cpp:524)) pay only double danger
  score — no lasting prize.
- **The hooks this milestone needs all exist.** The elite-challenge victory path
  is a clean insertion point for a token award; `completeDungeon`
  ([DungeonState.cpp:541](src/states/DungeonState.cpp:541)) already knows the
  run's `(town, depth)`, whether it raised the stakes (M33
  `stakesRaised`/`stakesPenaltyPct`), and the dungeon seed — everything the
  spawn roll needs. `Party` already carries optional session currencies/state
  (`gold`, `restTokens`, `currentTown`, `stakes`) saved with no format bump, and
  `TownState` already renders sprites and building-door prompts.

## C. Goals

- A **new Legendary-token currency** won from **optional elite fights**
  (+1 per `EliteChallenge` victory), shown in the event result and where it can
  be spent.
- A **rare black-market NPC** that appears **after a stakes-raising completed run
  in town ≥ 2** (20 %, **seeded from the run so a reload can't reroll it**),
  selling **one legendary piece** for **≥ 5000 gold or 3 legendary tokens**;
  declining never blocks anything.
- **~5 legendary items** clearly above Epic but **sim-validated** so they don't
  trivialize runs (extend the M19 exploit analysis against the M32 town-7
  battery).
- No battle-rule change (`battleRulesVersion` 1), no generation change
  (`generationVersion` 6 — the market spawn is a post-run town event, not a
  generation input), every new save field optional with a safe default.

## D. Owner decisions (locked at planning) & routine choices

Locked at program planning (not re-opened): new Legendary-token currency (not
M30 rest tokens), +1 per elite-fight victory; market appears after a
stakes-raising completion in town ≥ 2, 20 % chance, one legendary piece; price
≥ 5000 gold or 3 tokens.

Routine, reversible choices taken here (owner validates feel/balance at
approval):

- **Legendary roster (~5).** New `items.json` rows at `Rarity::Legendary`, one or
  two per slot, budgets a clear step above the Epic pieces, **sim-validated**
  against the town-7 battery to confirm they help without trivializing.
- **Gold price curve.** `5000 + (town − 2) × 750` (town 2 = 5000 … town 7 = 8750)
  — modest town scaling off the 5000 floor; token price fixed at 3. Tuned with
  the economy sim's attainability evidence.
- **Spawn determinism.** The 20 % roll is `hash(dungeonSeed, town, depth) % 100 <
  20` — deterministic per run and reload-proof (the seed is fixed at the Guild),
  and folding in `(town, depth)` keeps a single "lucky" seed from guaranteeing a
  market across different stakes.
- **Market lifetime.** The offer **persists until purchased**; a later
  stakes-raising hit **replaces** it with a fresh offer; a miss leaves an
  existing offer untouched. (Interpreting the plan's "stays until purchased or
  until the next completed run re-rolls it".) Flagged for owner veto.
- **Placement.** A seeded pick among a few known-free plaza tiles; the chosen
  tile is stored in the offer so rendering/interaction are trivial and stable.
- **Storage:** a `BlackMarketOffer` + `legendaryTokens` on `Party` (the
  established optional-save-field home); rules in a pure `game/BlackMarket.hpp`.
  New Game clears them (clean slate, with the M32/M33 reset).

## E. Proposed design & slices

1. **Legendary tokens.** `Party.legendaryTokens` (optional save field, old → 0);
   +1 on `EncounterKind::Challenge` victory with a message; shown in the black
   market screen (the only spender). New Game resets it.
2. **Legendary content.** ~5 `items.json` rows at `Rarity::Legendary`; loader/
   validator already accept the rarity; presentation lint already checks item
   names/descriptions.
3. **Pure spawn rules.** `game/BlackMarket.hpp`: `BlackMarketOffer { present,
   town, itemId, priceGold, tileX, tileY }`, `blackMarketRolls(seed, town,
   depth)`, `blackMarketPriceGold(town)`, `blackMarketTileIndex(seed)` +
   candidate plaza tiles, constants (`kBlackMarketChancePct=20`,
   `kBlackMarketBasePrice=5000`, `kBlackMarketTokenPrice=3`). Headless-tested.
4. **Spawn wiring.** In `completeDungeon`, after the M33 stakes update, if the run
   raised the stakes in town ≥ 2 and `total > 0` and `blackMarketRolls(...)`,
   build an offer (seeded legendary from the sorted legendary ids, priced,
   placed) into `party.blackMarket`.
5. **NPC + purchase.** `TownState` shows the NPC (sprite + "Black Market" label)
   when `party.blackMarket.present` and it belongs to the current town; standing
   on its tile prompts, Confirm opens `BlackMarketState` — the offered item's
   stats, price (gold **or** 3 tokens), Buy-with-gold / Buy-with-tokens
   (disabled if unaffordable) / Leave. A purchase grants the item, deducts, and
   clears the offer; declining never blocks. NPC sprite via `tools/asset_gen/` +
   manifest + `credits.md`; lint requires it. `kFirstMarket` M22 beat on first
   encounter.
6. **Tests, capture, sim.** Roll determinism + price + tile pure tests; token
   award/round-trip; offer save round-trip + backward compat; purchase paths
   (gold / tokens / insufficient); a capture scene with the NPC in town and the
   market purchase screen; presentation lint for the sprite; an economy/
   attainability sim (tokens-per-run, price reachable) + a legendary-balance
   check against the town-7 battery.

## F. Determinism & save compatibility

- **`generationVersion` 6 / `battleRulesVersion` 1 unchanged.** The market is a
  post-run town-state event; it does not touch generation or battle. Battle,
  simulator, and generation tests stay unmodified.
- **No `kSaveVersion` bump.** `legendaryTokens` and the `BlackMarketOffer` fields
  are optional (old saves → 0 / absent). Scoreboard untouched.
- **Save-scum resistance.** The spawn is a pure function of the persisted run seed
  + stakes; the autosave-on-entry and the offer persisting in the save mean a
  reload restores the same outcome — a miss cannot be rerolled into a hit.

## G. Out of scope

Any new dungeon mechanic; changing the elite-fight rule beyond the token award;
a general vendor/currency framework (the token is one typed counter, the market
one typed offer); M32 town or M33 stakes changes; a scoreboard change; M23/M24.

## H. Balance / acceptance

Legendaries are a clear step above Epic yet **sim-validated** not to trivialize a
town-7 run (the town-7 battery still requires a strong, well-geared party); the
5000-floor price (or 3 tokens) is attainable but meaningful given the economy
sim's per-run income and token rate; declining the market is never a soft-lock;
the 20 % seeded spawn cannot be save-scummed.

## I. Manual validation for the owner

Discovery excitement (does stumbling on the market feel like a reward?); price
feel (gold vs tokens); whether the legendaries feel powerful without breaking the
game; that declining costs nothing and the market persists until bought.

## J. Required documentation updates on completion

This note (as-implemented + deviations); `docs/milestones.md` (status);
`docs/game_design.md` (legendary tokens, black market, legendary rarity);
`docs/technical_design.md` (`BlackMarket`, `Party.legendaryTokens`/`blackMarket`
save fields, spawn wiring, `BlackMarketState`); `assets/credits.md` +
`docs/asset_pipeline.md` for the NPC sprite; `docs/manual_test_matrix.md` rows;
and a note that the expansion program is complete and M23 → M24 are next.

## K. As-implemented record (2026-07-21)

- **Status:** implemented, awaiting manual approval. No new owner decisions arose
  (the currency, 20 %, town ≥ 2, and ≥ 5000 g / 3 tokens were locked at planning);
  the §D routine choices were taken as written.
- **Legendary tokens.** `Party.legendaryTokens` (optional save field, old → 0);
  +1 on `EncounterKind::Challenge` victory ([DungeonState.cpp:524]) with a
  message; shown on the black-market screen. New Game resets it.
- **Legendary content.** 5 `items.json` rows at `Rarity::Legendary` — 2 weapons
  (`dawnforged_blade`, `stormcaller_rod`), 1 armor (`aegis_eternal`), 1 accessory
  (`crown_of_ages`), 1 relic (`titanforged_heart`) — a clear step above Epic.
  They are **excluded from the equip shop** (`equipShopBuyIds` skips
  `Rarity::Legendary`) so the market is their only source, but stay equippable
  once owned.
- **Pure spawn rules.** `game/BlackMarket.hpp`: `BlackMarketOffer`,
  `blackMarketRolls(seed, town, depth)` (SplitMix64-hashed 20 %),
  `blackMarketPriceGold(town)` (5000 + (town−2)×750), `blackMarketTileIndex`/
  `blackMarketItemIndex`, candidate plaza tiles, constants. Headless-tested.
- **Spawn wiring.** `completeDungeon`, after the M33 stakes update, spawns the
  offer into `party.blackMarket` when the run raised the stakes, town ≥ 2,
  `total > 0`, and `blackMarketRolls` hits — seeded from the run so a reload
  can't reroll it; persists until purchased, a later hit replaces it.
- **NPC + purchase.** `TownState` draws the NPC (`actor.market.overworld`, a
  hooded dealer generated by `generate_textures.ps1`) and its "Black Market"
  label when an offer belongs to the current town; standing on its tile prompts,
  Confirm opens `BlackMarketState` — item stats + description, Buy-with-gold /
  Buy-with-3-tokens (disabled if unaffordable) / Leave; a purchase grants the
  item, spends the currency, clears the offer. `kFirstMarket` M22 beat on first
  open. New Game clears `legendaryTokens` + `blackMarket`.
- **Save.** All M34 fields optional (old saves → 0 / no offer); a stored offer
  whose item the content no longer knows is dropped on load. No `kSaveVersion`
  bump.
- **Validation (this session, VS 2022 / MSVC 14.44, `build-msvc`).** Debug build
  clean; full suite **307/307** (31165 assertions), incl. 8 new M34 cases
  (`[blackmarket]` spawn/price/tile/roster, save round-trip + defensive drop,
  legendary stat-upgrade + no-trivialization balance, equip-shop legendary
  exclusion); battle & simulator tests **unmodified and green**. `--capture`
  **27/27** overflow-clean (new `27_black_market` purchase screen; the NPC shown
  in `25_town_ladder`). Presentation lint green (NPC required). Release build
  clean. Generators byte-identical apart from the one new NPC sprite.

### Deviations from the plan / note

1. **Legendaries are hidden from the equip shop** (an M34 change to the M31
   `equipShopBuyIds` filter) so the black market is their sole source; they stay
   equippable once owned. The M31 filter tests were updated accordingly.
2. **The upgrade balance check is a deterministic stat comparison**, not a sim
   HP-fraction comparison — combat outcomes carry enmity/turn-order noise that
   made a fraction comparison flaky, while the stat step-up is the real claim.
3. **Market lifetime = persist-until-purchased, replace-on-new-hit** (§D.4
   interpretation); flagged for owner veto.

### Post-implementation fix (owner testing, 2026-07-21)

The owner reported a legendary dropping from a chest, and difficulty triggering
the market. Investigation:

1. **Legendary chest leak — real bug, fixed.** `buildPools` put *every* item id
   (all rarities) into `Pools::items`, and `makeChest` draws its reward from that
   pool — so a chest could roll a legendary. Fixed at the source:
   `buildPools` now skips `Rarity::Legendary`, so legendaries never drop from a
   chest. (The merchant event draws from `Pools::consumables`, the item shop
   stocks consumables only, the equip shop already excludes legendaries, and the
   starting inventory is fixed — so the black market is now the sole source.)
   Bonus: excluding the 5 legendaries returns the chest pool to exactly the
   pre-M34 (M33) 36-item set, so a seed yields the same dungeon as under M33 and
   **`generationVersion` stays 6** legitimately. A generation-sweep test now
   guards that no chest ever holds a legendary.
2. **Trigger — verified correct, not changed.** The spawn conditions (a scoring
   completion **and** a stakes-raising run **and** town ≥ 2 **and** a seeded 20 %
   roll) were reviewed end to end (spawn → persist in the live party → NPC
   renders on return to town → interact) and found sound. The condition was
   extracted into a pure, testable `blackMarketShouldSpawn` (behaviour-preserving)
   and pinned by a test. Practical guidance: the market only appears after a run
   that **raises the stakes** (higher town or deeper than the last cleared run)
   in **town 2+**, at 20 % per such run — so it favours climbing, and a handful
   of escalating town-2+ clears will surface it. **Known persistence note** (as
   for the M32 unlock): a fresh spawn lives in the party and is written on the
   next save/autosave, so it shows immediately in-session but a quit-without-save
   then reload drops it — say the word if you'd like the spawn to autosave.
