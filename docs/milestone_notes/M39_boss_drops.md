# M39 — Boss Legendary & Token Drops

## A. Status and authority

- **Status:** complete (approved) — approved by the owner 2026-07-21 after manual
  testing; committed. Authored / re-audited and implemented 2026-07-21 against HEAD
  `c9e78a4` (the working tree held the approved-and-committed M38). Owner authorized
  beginning M39 after approving M38. As-implemented record in §K.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. Fifth milestone of the
  M35–M42 endgame program. A **post-battle reward roll only**: no change to how a
  battle resolves or to what a seed generates → `battle::kBattleRulesVersion` 3,
  `dungeon::kGenerationVersion` 8, `kSaveVersion` unchanged.

## B. Problem statement (verified at re-audit)

- **Boss kills pay only XP + gold.** `DungeonState::completeDungeon()`
  ([DungeonState.cpp:542](src/states/DungeonState.cpp:542)) is the single boss-kill
  path (reached only from the `EncounterKind::Boss` branch of the victory handler,
  [:537](src/states/DungeonState.cpp:537)). It scores the run, advances town/stakes,
  maybe spawns the black market, records a `ScoreEntry`, and pushes
  `DungeonResultState`. There is **no equipment/token reward** for beating a boss.
- **The seeded reload-proof-roll pattern already exists.**
  `blackMarketHash(seed, salt)` (SplitMix64 finalizer,
  [BlackMarket.hpp:52](src/game/BlackMarket.hpp:52)) turns the run's dungeon seed +
  a salt into a well-mixed value; the black market rolls its 20 % spawn, tile, and
  item entirely from `dungeon_.seed`, so reloading the entry autosave and replaying
  the same run cannot reroll a miss into a hit. M39 reuses this exact primitive.
- **`dungeon_.seed / dungeon_.town / dungeon_.depth` are all fixed for a run** and
  in scope at `completeDungeon()` — a drop keyed off them is a pure function of the
  run, i.e. reload-proof by construction.
- **The legendary pool is enumerated inline in the black-market spawn**
  ([DungeonState.cpp:584](src/states/DungeonState.cpp:584)–:592):
  `rarity == Legendary && (type == Equipment || type == Relic)`, sorted by id. The
  shipped roster has ≥ 5 legendaries (M34) plus M37's 3 legendary weapons
  (`test_black_market` asserts ≥ 5 and that legendaries never drop from chests).
- **Rewards apply to the live party and save with the next save/autosave**, exactly
  like the run's gold/XP and the black-market offer (see the comment at
  [DungeonState.cpp:566](src/states/DungeonState.cpp:566)). `Party.legendaryTokens`
  (int) and `Party.inventory.add(id, n)` are the sinks.
- **Baseline:** 338/338 tests, 31/31 capture scenes, `kBattleRulesVersion` 3,
  `kGenerationVersion` 8, `kSaveVersion` unchanged.

## C. Goals

- On a boss kill with **town ≥ 3 AND depth ≥ 4**, two **independent, seeded,
  reload-proof** rolls (off the run's dungeon seed): a **token** roll and a
  separate **legendary-equipment** roll.
- Rates ramp linearly with a combined town+depth progress from a low floor at
  t3/d4 to the **owner-fixed caps at t7/d20: 75 % token / 30 % legendary**; deeper
  or later than the anchors never exceeds the caps. A **town-7 token drop pays 2
  tokens** (owner rule).
- The legendary drop pool = the **same** legendary set the black market draws from
  (all `Legendary` equipment + relics, sorted) — the pool is shared, not
  duplicated, so the two systems can never diverge.
- Drops are shown in the victory/result flow and reflected in the run's result
  record. An `[economy-report]` drop-rate table proves the drip does not trivialize
  the black market or breach the M19 no-farm bar.
- No battle- or generation-version implication.

## D. Owner decisions & routine choices

Locked at program planning (not re-opened): the t3/d4 gate; the t7/d20 caps
(75 %/30 %); town-7 double tokens; seeded and reload-proof; no version bump.

Routine choices taken here (owner validates feel/balance at approval):

- **Ramp shape: linear in a combined town+depth progress, integer per-mille math.**
  Progress `p ∈ [0, 1000]` is the average of a town progress (0 at town 3, 1000 at
  town 7) and a depth progress (0 at depth 4, 1000 at depth 20), each clamped so
  neither exceeds its anchor. Chance = floor + (cap − floor)·p/1000, integer
  arithmetic only (no floating point in the deterministic path). **Token** floor
  **15 %** → cap **75 %**; **legendary** floor **5 %** → cap **30 %**. At t3/d4:
  15 %/5 %; at t7/d20 (and beyond): 75 %/30 %.
- **Two independent rolls, distinct salt namespace.** The token roll, the legendary
  roll, and the legendary item pick each use their own large, distinct salt
  constant, so they are independent of one another and of the black-market stream
  on the same seed. Same primitive (`blackMarketHash`), so the roll discipline is
  identical and already tested.
- **No de-duplication of legendary drops.** A dropped legendary is added to the
  inventory even if the party already owns one. De-duping would make the outcome
  depend on inventory state, which would **break the pure seed → drop property**
  and thus reload-proofing; a duplicate legendary is a minor, harmless surplus
  (equippable on another member). Determinism wins.
- **Fires on any boss kill meeting the town/depth gate**, independent of score. A
  boss kill is always a real completion (the branch is only reached on boss
  defeat); the reward is for beating the boss, not for a high score, and keying it
  off the seed (not turns) is what makes it reload-proof.
- **"Recorded in run stats" = shown in the result flow.** There is no persistent
  `RunStats` yet (that ships in M42); the run's outcome record is the
  `DungeonResultState`. Drops are surfaced there. No new persisted field, no
  `kSaveVersion` bump.

## E. Proposed design & slices

1. **Pure drop module — `game/BossDrops.hpp` (header math) + `game/BossDrops.cpp`
   (content pool).** Header: eligibility (`bossDropEligible`), the per-mille
   progress, the two chance functions, the three seeded rolls (token / legendary /
   item index), and the token count — all header-only, content-free, unit-testable
   (mirrors `BlackMarket.hpp`). `.cpp`: `legendaryDropPool(content)` (the sorted
   enumeration) and `rollBossDrops(seed, town, depth, content) -> BossDropResult`.
2. **Share the pool.** Refactor the black-market spawn in `completeDungeon()` to
   call `legendaryDropPool(context_.content)` instead of enumerating inline —
   behaviour-identical, and now both systems draw the one pool.
3. **Hook the drop.** In `completeDungeon()`, after the score/stakes/market work,
   `rollBossDrops(dungeon_.seed, dungeon_.town, dungeon_.depth, context_.content)`;
   apply `legendaryTokens += result.tokens` and `inventory.add(result.legendaryId)`;
   pass the result to `DungeonResultState`.
4. **Result UI.** `DungeonResultState` gains an optional `BossDropResult`
   parameter (default empty → old call sites and the no-drop path unchanged); when
   present it renders a "Boss drops" block (tokens line, legendary line with the
   item's display name), inside the panel budget (panel height already grows with
   line count). Capture scene at max drop (2 tokens + longest legendary name).
5. **Tests + report + docs.** `test_boss_drops.cpp` (determinism/reload-proof,
   eligibility gate, monotonic ramp, exact t3/d4 + t7/d20 values, town-7 double,
   shared-pool equality, drop-rate distribution bands); an `[economy-report]`
   drop-rate table; `game_design.md` / `technical_design.md` / README / manual test
   matrix; a first-drop tutorial prompt is **out of scope** (no new interactive
   mechanic — the result screen already explains itself; revisit if the owner wants
   one).

## F. Determinism & save compatibility

- **Reload-proof:** every drop is a pure function of `(dungeon_.seed, town, depth)`;
  replaying the same run reproduces the same drops, and no reload rerolls them
  (identical to the M34 black-market guarantee). No wall-clock or unseeded RNG.
- **No `kBattleRulesVersion` change** (battle resolution untouched) and **no
  `kGenerationVersion` change** (a seed still generates the same dungeon — the drop
  is a post-battle reward, not generated content). **No `kSaveVersion` change**
  (tokens and inventory are existing fields; no new persisted state).

## G. Out of scope

The castle and the King (M40); story (M41); enrichment incl. persistent `RunStats`
(M42); any battle-rule, generation, or scoring change; de-duplication or "smart"
drop targeting; a sell economy for surplus legendaries.

## H. Balance / acceptance

Same seed → same drops (reload cannot reroll); the t7/d20 caps equal the owner
rule (75 %/30 %, 2 tokens); the ramp is monotonic in town and in depth and never
exceeds the caps; the token/legendary rolls are independent; the black-market and
boss-drop pools are identical; an `[economy-report]` table shows the token drip is
bounded and does not trivialize the black market or the M19 no-farm bar (which
governs **score**, untouched here); full suite green from the 338 baseline; capture
overflow-clean with the new drop scene; lint green (no new assets).

## I. Manual validation for the owner

Whether boss drops feel like a meaningful reward for deep, high-town clears without
trivialising the black market; whether the drop lines read clearly on the result
screen; whether the rates feel right across the ladder (a manual-feel item).

## J. Required documentation updates on completion

This note (§K); `docs/milestones.md` (status); `docs/game_design.md` (boss drops as
a player-facing rule); `docs/technical_design.md` (the `BossDrops` module + the
shared legendary pool); README (drops line in How-to-play / known limitations);
`docs/manual_test_matrix.md` rows; a manual checklist per
`docs/milestone_completion_template.md`.

## K. As-implemented record (2026-07-21)

- **Status:** complete (approved 2026-07-21). Implemented / audited against
  the post-M38 checkout. The §D routine choices were taken as written (linear
  per-mille ramp; floors 15 %/5 %; distinct-salt independent rolls; no
  de-duplication; fires on any eligible boss kill; drops surfaced on the result
  screen).
- **Pure module.** `game/BossDrops.hpp` (header math) + `game/BossDrops.cpp`
  (content pool + `rollBossDrops`), registered in `CMakeLists.txt`. Reuses the M34
  `blackMarketHash` SplitMix64 primitive with three distinct salt constants
  (token / legendary / item index). Gate `town ≥ 3 && depth ≥ 4`; combined
  town+depth per-mille progress; token chance **15 → 75 %**, legendary chance
  **5 → 30 %**, capped beyond t7/d20; **town-7 pays 2 tokens**.
- **Shared pool.** `legendaryDropPool(content)` returns the sorted `Legendary`
  equipment/relic ids (**8** in the shipped roster). `DungeonState::completeDungeon`
  was refactored to call it for the M34 black-market spawn too, so both systems
  draw the one identical pool (behaviour-identical to the previous inline
  enumeration).
- **Hook + UI.** `completeDungeon` rolls `rollBossDrops(dungeon_.seed, town, depth,
  content)`, applies `legendaryTokens += tokens` and `inventory.add(legendaryId)`
  to the live party (saved on the next save/autosave), and passes the result to
  `DungeonResultState` (new optional `BossDropResult` param, default empty → old
  call sites unchanged). The result screen renders a "Boss drops" block (tokens
  line + full-width legendary line with the item's display name). The panel's line
  pitch now tightens when the fullest breakdown + drop block would exceed the
  virtual height, keeping the footer on-screen (see the deviation below).
- **Validation (this session, VS 2022 / MSVC, `build-msvc`).** Debug build clean;
  full suite **348/348** (+9 `[bossdrops]` + 1 `[economy]` guard; a new
  `[.][economy-report]` drop-rate table is hidden/on-demand). `--capture` **32/32**
  overflow-clean (new `32_result_drops` scene: fullest breakdown + a maximal
  2-token + longest-legendary drop). Release build + Release suite **348/348**
  clean. The `[economy-report]` drop table confirms the ramp: t3/d4 15 %/5 % →
  t7/d20 75 % ×2 / 30 %, monotonic in town and depth; pool size 8. Determinism +
  reload-proof, eligibility gate, exact anchors, monotonicity, town-7 double,
  shared-pool equality, and distribution bands all covered by tests.
- **Versions.** `kSaveVersion`, `battle::kBattleRulesVersion` (3), and
  `dungeon::kGenerationVersion` (8) all **unchanged** — a post-battle reward roll.

### Deviations from the plan / note

1. **Result-panel line pitch is now dynamic.** The fullest score breakdown (all 10
   conditional rows) plus the M39 drop block would have made the panel taller than
   the 426×240 virtual screen, pushing the footer off-screen — a vertical clip the
   capture overflow lint does **not** catch (it checks horizontal text width only).
   Caught by eyeballing the new `32_result_drops` capture. Fix: when the computed
   panel height would exceed the screen, tighten the per-line pitch just enough to
   fit; the common case keeps pitch 13 unchanged. Flagged for owner veto.
2. **"Recorded in run stats" = shown on the result screen.** There is no persistent
   `RunStats` yet (M42); the run's outcome record is `DungeonResultState`, where
   the drops are surfaced. No new persisted field.
3. **No first-drop tutorial prompt.** Boss drops add no new interactive mechanic
   (the result screen is self-explanatory), so no `tutorial.json` beat was added —
   revisit if the owner wants one.

### Known items for owner validation

- Whether boss drops feel like a meaningful reward for deep, high-town clears
  without trivialising the black market; whether the drop lines and the tightened
  fullest-panel read clearly; whether the rates feel right across the ladder.
