# M55 — Theme rites (per-theme dungeon events)

> Program: **Adjustment program (M53–M56)**, authorized 2026-07-24. Third
> milestone; the **only** one that changes generation (`kGenerationVersion`
> 10 → 11) — the single planned version bump in the whole program.
>
> Authority: `CLAUDE.md` > this note > `docs/milestones.md` > `docs/game_design.md`
> > `docs/technical_design.md`. §J is the as-implemented record.

## Goal

One unique room event per theme, each **guaranteed exactly once per dungeon** and
never appearing outside its theme:

- **The Armory Ghost** (Ruined Keep) — trade one *inventory* (not equipped)
  equipment piece for a seeded random piece one rarity tier up, **same slot**,
  sight unseen. Common→…→Epic→Legendary (an epic can yield a legendary — a
  designed third legendary source; the traded piece is consumed). Legendary
  input: the ghost declines. No gold, no turns, no score effect.
- **Miner's Cache** (Crystal Mine) — a rockfall guards a rich cache. Cost: a
  **one-third max-HP wound** to each standing member (never fatal, clamp to 1;
  deliberately slightly worse than the trapped chest's 25 %). Reward: gold
  **strictly above a trapped chest** at the same depth (~1.5×) **plus a guaranteed
  item** from the dungeon loot pool.
- **The Elder Root** (Hollow Forest) — pay town-scaled gold (disabled/refused when
  short); the whole party gains XP comparable to **one elite battle** at that
  town/depth. The inverse trade of fighting for XP: zero battle turns (score
  preserved) for gold.

## Baseline / re-audit (against HEAD `07a13bb` + M53/M54)

- `RoomEventKind` at [DungeonModel.hpp:21-23](../../src/dungeon/DungeonModel.hpp):
  None, Shrine, HealingSpring, Merchant, EliteChallenge, ScoreWager, RestToken,
  RoyalRelic. Adding ArmoryGhost, MinersCache, ElderRoot.
- The event loop is [DungeonGenerator.cpp:439-542](../../src/dungeon/DungeonGenerator.cpp);
  the RoyalRelic rare-replacement draw is at 486-492; the trapped-chest gold add
  is `25*depth + 15` at 544-551 (base chest gold `rng(10,30)*depth`).
- `kGenerationVersion` = 10 ([RoomLayout.hpp:35](../../src/dungeon/RoomLayout.hpp)).
- Resolution: `resolveEvent` / `eventPromptText` in
  [DungeonState.cpp:327-467](../../src/states/DungeonState.cpp); the event marker
  render switch is at 970-1011.
- `grantPartyXp(party, xp, db)` grants `xp` to **each** member (not split); battle
  XP is flat (not depth-scaled), so "one elite battle" ≈ 120–180 XP/member.
- Empty-theme generation is **byte-identical** (`themeEventKind("") == None`, so no
  forcing and no relic-skip) — only the three real themes change, which is why the
  version bump is warranted and the empty-theme determinism tests still pass.

## Mechanism

- **Generator guarantee.** The **first** created event slot (`made == 0`) is forced
  to the active theme's event, and the RoyalRelic replacement draw **skips that
  slot** (a relic never displaces the rite; relic behaviour otherwise unchanged).
  Theme events never appear in any other slot, so exactly one per dungeon. The
  ElderRoot's `goldCost` and the MinersCache's guaranteed `itemId` are set at
  generation (deterministic, reload-proof); the cache gold and the ghost's upgrade
  are computed at resolution from a seeded hash (the RoyalRelic precedent).
- **`kGenerationVersion` 10 → 11** (theme events change the event roll for a
  themed seed).
- Pure helpers live in `src/dungeon/ThemeEvents.hpp/.cpp` (raylib-free, tested):
  `themeEventKind(themeId)`, `nextRarityUp(Rarity)`, `armoryGhostUpgrade(db,
  tradedId, hash)` (same-slot, next-rarity, legendary-refusal), `minersCacheWound`
  (never fatal), `minersCacheGold(depth)` (> max trapped chest), `elderRootPrice`,
  `elderRootXp`.
- **The Armory Ghost needs a picker:** `resolveEvent` pushes `ArmoryGhostState`
  (the equip-shop list idiom over eligible inventory pieces); on confirm it applies
  the seeded upgrade, consumes the traded piece, and marks the event resolved.
  Miner's Cache and the Elder Root resolve directly in `resolveEvent`.

## Tests & capture

- `tests/test_theme_events.cpp`: generator — theme event present **exactly once**
  per seed sweep, correct theme mapping, never cross-theme, relic interplay,
  `kGenerationVersion == 11`. Pure helpers — tier-up table (incl. legendary refusal
  + same-slot guarantee), cache wound (never fatal), cache gold > trapped, root
  pricing/XP (comparable to an elite battle, affordable mid-run). Determinism:
  same seed ⇒ same placement and resolution outputs.
- Capture: three event-prompt scenes (worst-case text) + the event markers in an
  exploration scene.

## Documents

`docs/game_design.md` (the three events + guarantee rule), `docs/technical_design.md`
(generation v11 history line, ThemeEvents, ArmoryGhostState), `docs/milestones.md`,
and one-line tutorial/flavor mentions.

## Acceptance criteria

Each theme guarantees its event exactly once; effects match the approved designs
(ghost tier-up trade, cache 33 % wound + >trap reward + item, root gold→XP);
generation v11; determinism preserved; full suite green from 505 (Debug) / 501
(Release), capture clean with the new scenes.

## J. As-implemented record

Implemented 2026-07-24 on base checkout `07a13bb` + M53/M54. **`kGenerationVersion`
10 → 11** is the only version change (save 1, rules 10, settings 1, achievements 1
unchanged).

**As built.**
- `RoomEventKind` gains `ArmoryGhost` / `MinersCache` / `ElderRoot`. Pure helpers
  in `src/dungeon/ThemeEvents.hpp/.cpp` (themeEventKind, nextRarityUp,
  armoryGhostUpgrade, minersCacheWound/Gold, elderRootPrice/Xp, themeEventHash).
- Generator forces the rite onto the first event slot (`made == 0`) and the relic
  draw skips it (`themeSlot` guard). Empty-theme generation is byte-identical
  (verified: `themeEventKind("") == None`).
- ElderRoot `goldCost` and MinersCache `itemId` baked in at generation; cache
  gold, root XP, and the ghost upgrade derived at resolution.
- Resolution: MinersCache and ElderRoot resolve directly in
  `DungeonState::resolveEvent`; the Armory Ghost pushes `ArmoryGhostState` (the
  equip-shop list idiom); `DungeonState::onResume` rebuilds the room when a pushed
  sub-state resolves the event. Event markers use a distinct glyph+colour per rite
  (glyph+colour fallback — no bespoke art yet, placeholder policy; the presentation
  lint's event-sprite allow-list is unaffected).

**Calibration.** MinersCache gold `68*depth + 25` exceeds the biggest possible
trapped chest (`55*depth + 15`) at every depth. MinersCache wound is `maxHp/3`,
clamped to 1 (never fatal, deliberately harsher than the trapped chest's 25 %).
ElderRoot price `60 + 40*depth + 50*town` (150g @ town 1 depth 1 → 1210g @ town 7
depth 20; a fraction of a run's clear gold). ElderRoot XP `90 + 6*depth + 6*town`
(per member; battle XP is flat, so this sits in the one-elite-battle band — tested
against the shipped top enemy xpReward).

**Deviation (routine, local test correction).** `tests/test_events.cpp`'s
`events: generated event rooms are well-formed dead ends` asserted a ruined_keep
sample shows exactly the **6** pre-M55 event kinds. The guaranteed Armory Ghost
adds a 7th, so the assertion was updated to `== 7` and now also verifies the rite
appears (town defaults to 1 there, so no Royal Relic leaks in). No generation
value was changed to satisfy it — it reflects the guarantee.

**Validation (this session).**
- Configure/build debug + release — OK, no warnings.
- Debug tests: `ctest --preset debug` → **515/515 passed** (505 + 10 new
  theme-events cases). `[theme-events]` alone: 10 cases, 1258 assertions.
- Release tests: `ctest --preset release` → **511/511 passed**.
- No new compiler warnings (verified by recompiling the M55 translation units).
- Capture: `CrystalDungeons.exe --capture` → **68/68 scenes clean** (65 + three
  event-prompt scenes: `66_armory_ghost`, `67_miners_cache`, `68_elder_root`;
  two prompts were shortened to fit the footer's ~418px budget).
