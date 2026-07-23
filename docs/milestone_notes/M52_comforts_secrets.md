# M52 — Comforts & secrets

> Authoritative scope note for M52. Read after `CLAUDE.md`, `docs/milestones.md`
> (the M52 section + ledger row), and the approved plan. Six independent
> features in one milestone: an ambience volume slider, an in-battle battle log,
> an equip-shop QoL lift, bestiary max stats, the Dragon Crown's hidden effect,
> and a high-stakes black-market spawn.

- **Status:** ◑ implemented, awaiting manual approval (2026-07-24)
- **Authorized:** 2026-07-23 (owner), as one quality-of-life milestone before
  M23/M24; the last authorized expansion.
- **Base checkout:** `f387588` ("Merge pull request #16 … claude05" = M51
  `64d220e` + `30b1166` "docs update"), working tree clean.

## Owner decisions already taken (2026-07-23 — do not re-ask)

- The battle log **opens AND closes with the Menu/Pause action** (Tab / Start —
  unused in battle today). No new bindings or hint text.
- Bestiary max stats use the **strongest-real-context rule**: regular foes
  ×5.70, guards and the King ×5.00, bosses ×5.80; the **endless rush is
  excluded** (its scale is unbounded by design).
- The **34 % market roll needs only a beaten boss** at town 7, depth ≥ 20 —
  regardless of stakes, penalty, or a score floored to 0. Retreats/defeats never
  count. The existing 20 % stakes-raising rule stays alongside, unchanged.
- Ambience slider **default 5/10 (0.5)**.

## Versions

| Constant | Baseline | After M52 |
|----------|----------|-----------|
| `battle::kBattleRulesVersion` ([Battle.hpp:45](../../src/battle/Battle.hpp)) | 9 | **10** (Crown's hidden effect changes how identical inputs resolve) |
| `dungeon::kGenerationVersion` ([RoomLayout.hpp:35](../../src/dungeon/RoomLayout.hpp)) | 10 | 10 (unchanged) |
| `save::kSaveVersion` ([SaveSystem.hpp:18](../../src/save/SaveSystem.hpp)) | 1 | 1 (unchanged) |
| `settings::kSettingsVersion` ([Settings.hpp:20](../../src/settings/Settings.hpp)) | 1 | 1 (unchanged) |

`ambienceVolume` is an optional settings field (absent = 0.5, the `highContrast`
precedent) and the high-stakes market roll is a fresh-salt additive rule that
only adds spawns where none happened before — neither warrants a version bump.
`disablesMinionRevive` is an optional `ItemDef` field; content schema v1 is
unchanged (additive optional field, the M44 relic-field precedent).

## Re-audit against the current checkout (2026-07-23)

The plan's "verified current-state facts" were re-checked against `f387588`.
All held. Load-bearing facts:

| Fact | Verified location |
|------|-------------------|
| Ambience follows the SFX slider at two sites | [AudioManager.cpp:306](../../src/audio/AudioManager.cpp) (`startAmbience`), [:414](../../src/audio/AudioManager.cpp) (`setVolumes`), both `sfxVolume_ * fileAmbienceVolume_[idx]` |
| `setVolumes(master, music, sfx)` is 3-param | [AudioManager.hpp:54](../../src/audio/AudioManager.hpp) |
| Settings has 3 volume floats; parse in `audio` block; serialize in `audio` object | [Settings.hpp:42-44](../../src/settings/Settings.hpp), [Settings.cpp:153-158, 274-276](../../src/settings/Settings.cpp) |
| Audio submenu rows Master/Music/SFX/Background Audio/Back | [SettingsState.cpp:78-84](../../src/states/SettingsState.cpp); `stepVolume`+`applyAudio` 139-148; startup at [Application.cpp:84](../../src/core/Application.cpp) |
| Battle `message_` sites | [BattleState.cpp](../../src/states/BattleState.cpp): `executePending` 691/696/710/717, `executeEnemy` 772-795, `executeConfused` 816-824, `executeUncontrolled` 845-858, `startActorTurn` tick 459 / turnOpen 469, `onCommand` guard 614 / flee 619, outcome 510/1118. `afterAction()` (873) is the single choke point every resolved action passes through with `message_` already final. `InputAction::Menu` is **not** handled in battle. |
| Overlay precedent + scroll pieces | `DetailsOverlayState` (transparent, `rendersBelow`, dim + `FrameStyle::Raised`); `ui::ScrollWindow` (`follow`/`scrollBy`/`moreAbove`/`moreBelow`), `ui::wrapText`/`lineHeight` |
| Equip Buy rows lack owned count; EquipItem rows show `xN` | [EquipShopState.cpp:149, 187-188](../../src/states/EquipShopState.cpp) |
| `bonusDelta`/`statBonusSummary` are anon-namespace locals | [EquipShopState.cpp:36-50, 285-302](../../src/states/EquipShopState.cpp); item-shop count idiom `TextFormat("x%-3d%5dg", owned, value)` at [ItemShopState.cpp:48](../../src/states/ItemShopState.cpp) |
| Bestiary shows base `EnemyDef`/`BossDef` stats | [BestiaryState.cpp:166-178](../../src/states/BestiaryState.cpp); Entry holds base `stats`, `boss`, `id` |
| Battle scaling is `team.statScalePct` applied inline | [Battle.cpp:1166-1173](../../src/battle/Battle.cpp) (`scaled` lambda); a scalar variant at [DangerRating.cpp:95](../../src/danger/DangerRating.cpp) |
| Max scales | regular `castleFloorScalePct` = `combineTownScale(100+90, 7)` = **570 %** ([Castle.cpp:11-20](../../src/game/Castle.cpp)); `kKingScalePct` **500**, `kBossRushScalePct` **580** ([Castle.hpp:65, 85](../../src/game/Castle.hpp)); `bossOnly` marks the guards ([Castle.cpp:62](../../src/game/Castle.cpp)) |
| Dragon Crown | [items.json:16](../../data/items.json): `battleTarget: enemy`, `requiresBossId: the_hollow_king`, ATK-40/DEF-40 for 4; kept off-King, consumed on-King ([BattleState.cpp:709-717](../../src/states/BattleState.cpp)) |
| `Battle::useItem` shared path; revive clock | [Battle.cpp:1031-1111](../../src/battle/Battle.cpp) (boss guard 1038-1040); `Combatant.reviveMinionTurns`/`reviveMinionCounter` ([Battle.hpp:148-149](../../src/battle/Battle.hpp)); early-out in `beginUnitTurn` |
| `ItemDef` optional-field loader block | [ContentLoader.cpp:278-304](../../src/content/ContentLoader.cpp); semantic block 306-325 |
| Black market predicate + call site | [BlackMarket.hpp:59-73](../../src/game/BlackMarket.hpp) (`blackMarketRolls`, `blackMarketShouldSpawn`); called once from `completeDungeon` at [DungeonState.cpp:642](../../src/states/DungeonState.cpp) with `total > 0` (completeDungeon is only reached on a boss-beaten completion) |
| Counts | 485 tests under ctest; 61 capture scenes |

## Slices

### E1 — Ambience volume slider

- `settings::Settings` gains `float ambienceVolume = 0.5f;` — optional parse
  (`audio.ambience`, absent = 0.5, clamped) + serialize beside the other three
  in the `audio` object. **No settings-version bump** (the `highContrast`
  precedent).
- `AudioManager::setVolumes` gains a 4th param `ambience`; a new `ambienceVolume_`
  member replaces `sfxVolume_` at **both** ambience sites (`startAmbience`,
  `setVolumes`). Callers updated: `Application` startup and
  `SettingsState::applyAudio`.
- The Audio submenu gains an `Ambience Volume` row between SFX and Background
  Audio (`Row::AmbienceVolume`), same 0.1-step adjust.
- **Behaviour change:** ambience no longer follows SFX and is quieter by default
  (0.5 vs the old effective 1.0). The stale M27 "SFX slider moves ambience" note
  in `docs/manual_test_matrix.md` is corrected.

### E2 — Battle log

- **Buffer:** `battle::BattleLog` (pure header `src/states/BattleLog.hpp`, the
  `QuitPrompt.hpp` precedent) — a ring buffer that keeps the **last 30** exact
  `message_` strings. Owned by `BattleState`, so it is freed with the battle.
  Never touches the battle model or `rollCursor`.
- `BattleState` pushes the final shown line at the single `afterAction()` choke
  point (covering attack/skill/item incl. "(kept)", enemy/confused/uncontrolled
  turns after the `turnOpenLine_` prepend, guard, and a poison-kill tick) plus
  the flee line and the final outcome line — the two paths that bypass
  `afterAction`.
- **Overlay:** `BattleLogState` — transparent (`rendersBelow`), dim +
  `FrameStyle::Raised` panel via the M46 kit, "Battle Log" plaque, newest entry
  at the bottom, scrollable with Up/Down through `ui::ScrollWindow` over the
  wrapped lines (`ui::wrapText` at panel width), more-above/below chevrons, and
  a footer (Up/Down Scroll, Menu/Cancel Close).
- **Input:** `BattleState::handleInput` — `pressed(InputAction::Menu)` pushes the
  overlay in **every** phase (so a full Jester party, which only ever sees
  Intro/Resolve/Done, can still review the fight); inside the overlay Menu or
  Cancel closes. A `[Menu] Log` hint sits on the command menu (with Details) and
  on the resolve line for the auto-played Jester case (owner feedback, see §J).

### E3 — Equip-shop QoL

- Buy rows show owned count + price: the suffix becomes
  `TextFormat("x%-3d%5dg", inventory.count(id), it->value)` — the exact
  item-shop column idiom.
- `bonusDelta` and `statBonusSummary` are promoted into a small pure header
  `src/states/EquipDiff.hpp` (namespace `cd::equip`), testable and shared with
  `openItemDetails`.
- In `Phase::EquipItem` the info panel becomes the current slot item + the diff:
  a `Current: <name or (none)>` line with the slot's bonus summary in `textDim`,
  and a `Diff:` line for the highlighted candidate drawn **per stat** (`statDeltas`)
  — each stat in its own colour: a gain in `success` (green), a loss in
  `dangerText` (coral), no change in normal `text`. Cursor 0 (Unequip) diffs
  `StatBlock{}` vs current. The item description moves out of the EquipItem panel
  (Buy and the Details overlay keep it) — a deliberate deviation recorded below.
  (Per-stat colouring refined from a single net-sign line colour on owner
  feedback — see §J.)

### E4 — Bestiary max stats

- New pure helper `content::scaledStats(const StatBlock&, int pct)` (one home for
  the per-field ×pct/100 multiply), used by `buildBattle`'s `scaled` lambda (an
  exact no-behaviour-change dedup) and the bestiary.
  `DangerRating.cpp:95` is a **scalar** threat scale, not a per-field StatBlock
  scale; routing it through `scaledStats` would change integer rounding and move
  danger tests, so it is left untouched (recorded as a deviation).
- New pure helper `foeMaxScalePct(boss, bossOnly, isKing, castleFloorPct)`
  (`src/states/BestiaryStats.hpp`): King → 500, other boss →
  `max(castleFloor, 580)` = 580, `bossOnly` guard → 500, regular → castleFloor
  (570). The endless rush is deliberately excluded (unbounded).
- Display (known foes only): a strongest-context stat readout beside the base
  pair, fitted, in `textDim`; unknown foes keep "?". Verified against the
  worst-case bestiary panels (the King, scene 41; and scene 38) — the exact
  presentation is chosen to keep those overflow-clean (see the as-implemented
  section).

### E5 — The Crown's secret (battle rules 9 → 10)

- Schema: optional `ItemDef.disablesMinionRevive` (bool, default false; loaded
  beside `requiresBossId`). Semantic validation: meaningful only with
  `battleTarget: enemy` (reported otherwise). `dragon_crown` gets `true`. **No id
  is branched on** (the M44/M45 convention).
- Rule (shared code): in `Battle::useItem`, after the `requiresBossId` guard
  passes, if the item disables revive → `t.reviveMinionTurns = 0` and
  `t.reviveMinionCounter = 0`. `beginUnitTurn`'s early-out then keeps the court
  down forever. **Hidden:** no new message text, no description change, no float
  — the King simply never calls his guards back. Consumption unchanged (spent on
  the King, kept elsewhere).
- Rules **9 → 10** with a version-history line that stays vague in player-facing
  docs but is precise in `Battle.hpp`'s comment (code, not player-facing).
  `game_design.md` records it in the relics section as a designed hidden effect.

### E6 — The high-stakes market

- `BlackMarket.hpp`: `kBlackMarketHighStakesChancePct = 34`,
  `kBlackMarketHighStakesTown = 7`, `kBlackMarketHighStakesDepth = 20`, and a
  second roll `blackMarketHighStakesRolls(seed, town, depth)` on a **fresh salt**
  (independent of the 20 % roll's stream).
- `blackMarketShouldSpawn` gains a `completed` parameter and an OR-path:
  `completed && town >= 7 && depth >= 20 && blackMarketHighStakesRolls(...)` —
  **no score or stakes condition** on this path (owner decision). The call site
  passes `true` for `completed` (`completeDungeon` is only reached on a
  boss-beaten completion). The existing 20 % path is unchanged.
- The offer construction (item/tile/price from the same seed) is unchanged; an
  existing present offer is overwritten exactly as today.
- **No version bump:** the new path only adds spawns in cases that previously
  never spawned; seeded and reload-proof like the old one.

## Tests (`tests/test_comforts.cpp` + extensions)

- Settings: `ambienceVolume` round-trip; absent → 0.5; clamped.
- `BattleLog`: cap at 30 (31st evicts the oldest), order preserved, empty ok.
- `EquipDiff`: `bonusDelta` cases (moved code — pin behaviour), unequip diff,
  sign.
- `scaledStats` + `foeMaxScalePct`: the four-context mapping table; the
  `buildBattle` dedup still agrees with the danger/balance batteries.
- Crown: through shared `useItem` — on the King, the revive clock never fires
  afterward; off-King nothing changes and the item is kept; sim == live on the
  Crown battle; rules version == 10.
- Market: the 34 % path spawns on a penalized/score-0 town-7 depth-20 completion
  (a seed under and one over); never below t7/d20 without stakes; the 20 % path
  unchanged; the two rolls are independent.

## Capture

New scenes (61 → 64, all overflow-clean): the battle-log overlay mid-scroll, the
EquipItem diff panel, and the bestiary max-stats block (the King entry carries
the largest numbers). Existing scenes 38/41 re-verify the bestiary flavor fit.

## Acceptance criteria

1. Ambience has its own 0–10 slider (default 5), independent of SFX, applied live
   and at startup; old settings files load at 0.5.
2. Menu opens a scrollable battle log of the last 30 action results (exact shown
   text); it dies with the battle; the battle stream is untouched.
3. Equip-shop Buy rows show owned count + price; the equip flow shows the current
   slot item and the stat diff for the highlighted candidate.
4. The bestiary shows base and strongest-context stats per the four-context rule
   (endless excluded, documented).
5. The Crown, used on the King, silently disables his revive clock for the rest
   of the battle (rules 10, sim == live, schema-driven, no text).
6. A town-7 depth-20 boss kill rolls the independent 34 % market spawn regardless
   of stakes/penalty/score; the 20 % rule is unchanged.
7. Full suite green from 485 in Debug AND Release; capture clean with the new
   scenes; no generation/save/settings version change.

## §J — As-implemented record

Implemented 2026-07-24 on base `f387588`. Status set to **implemented, awaiting
manual approval**.

### Files changed

- **Source (E1):** `src/settings/Settings.hpp` (`ambienceVolume`),
  `src/settings/Settings.cpp` (parse/serialize), `src/audio/AudioManager.{hpp,cpp}`
  (`setVolumes` 4th param + `ambienceVolume_` at both sites),
  `src/core/Application.cpp` (startup call), `src/states/SettingsState.{hpp,cpp}`
  (`Row::AmbienceVolume`, row, adjust, `applyAudio`, `captureShowAudio`).
- **Source (E2):** `src/states/BattleLog.hpp` (new, pure),
  `src/states/BattleLogState.{hpp,cpp}` (new overlay), `src/states/BattleState.{hpp,cpp}`
  (`log_` member, pushes at `afterAction`/flee/two outcomes, Menu handler).
- **Source (E3):** `src/states/EquipDiff.hpp` (new, pure),
  `src/states/EquipShopState.{hpp,cpp}` (owned-count column, current+diff panel,
  promoted helpers, `captureEnterEquipItem`).
- **Source (E4):** `src/content/Stats.hpp` (`scaledStats`),
  `src/battle/Battle.cpp` (`scaled` lambda dedup),
  `src/states/BestiaryStats.hpp` (new, `foeMaxScalePct`),
  `src/states/BestiaryState.{hpp,cpp}` (`maxScalePct`, the `max` line).
- **Source (E5):** `src/content/Definitions.hpp` (`disablesMinionRevive`),
  `src/content/ContentLoader.cpp` (loader + semantic check),
  `src/battle/Battle.{hpp,cpp}` (rules 9 → 10, the shared-code rule),
  `data/items.json` (`dragon_crown`).
- **Source (E6):** `src/game/BlackMarket.hpp` (constants + `blackMarketHighStakesRolls`
  + the OR-path signature), `src/states/DungeonState.cpp` (call site).
- **Capture:** `src/capture/CaptureRunner.cpp` (scenes `62_battle_log`,
  `63_equip_diff`, `64_settings_audio`).
- **Tests:** `tests/test_comforts.cpp` (new, 12 cases), `tests/CMakeLists.txt`;
  `tests/test_black_market.cpp` + `tests/test_kings_court.cpp` (updated for the
  signature / version-floor change).
- **Build:** `CMakeLists.txt` (registers `BattleLogState.cpp`).
- **Comment-only:** `src/dungeon/DungeonGenerator.hpp:25` (stale "stays 6").
- **Docs:** `docs/milestones.md`, `docs/completion_roadmap.md`,
  `docs/game_design.md`, `docs/technical_design.md`, `docs/manual_test_matrix.md`,
  `README.md`, and this note.

### Post-implementation refinements (owner feedback, 2026-07-24)

- **Battle log always openable + hinted.** The Menu gate was widened from "any
  phase but Done" to **every phase**, so a full Jester party (which never reaches
  a command phase) can open the log during its auto-played turns. A `[Menu] Log`
  hint now shows on the command menu (beside a `[Details]` hint) and, for the
  Jester case, top-right on the resolve line. The hint was intentionally kept off
  the Skill/Item sub-menu lines: appending it there ran the `battle.skillhint` /
  `battle.itemhint` strings 1–2px past the panel (the long `[Backspace]` Cancel
  label), and those sub-menus are reached from the command menu where the hint
  already showed.
- **Per-stat equip diff colour.** The equip-flow `Diff:` line now colours **each
  stat individually** — a gain green, a loss coral, no change in normal text —
  via a new pure `equip::statDeltas`, instead of tinting the whole line one
  net-sign colour. `deltaSign` is retained (tested) but no longer drives the
  panel.

### Deviations from the plan

1. **Bestiary max-stats layout.** The plan sketched a separate three-line "At
   their strongest" block. That block overflowed the King's bestiary entry
   (scene 41), whose long flavor has no spare vertical room (the capture caught
   it: flavor 55px > 44px available). Resolved by folding the strongest-context
   values onto **one extra line** — a `max` line under the base stats,
   positionally matching the labels above — which keeps the base pair's original
   two-line footprint, so scene 41 stays overflow-clean while still showing base
   and strongest per the four-context rule. A conservative, reversible
   presentation choice made to satisfy the non-negotiable capture-clean bar.
2. **`DangerRating.cpp:95` not routed through `scaledStats`.** The plan listed it
   as a dedup site, but it scales a **summed threat scalar**, not a per-field
   `StatBlock`; rerouting it would change integer rounding and move the danger/
   balance battery. Left untouched (behaviour-preserving); `scaledStats` homes the
   `buildBattle` per-field multiply and the bestiary readout only.
3. **Equip-flow description moved out of the EquipItem panel** (kept in Buy + the
   Details overlay) so the two-line panel shows the current item + diff — the
   decision-relevant information, as the plan anticipated.

### Versions

`battle::kBattleRulesVersion` **9 → 10** (E5). No other version moved:
`kGenerationVersion` 10, `kSaveVersion` 1, `kSettingsVersion` 1 unchanged. Old
`settings.json` (no `audio.ambience`) loads at 0.5; the market OR-path only adds
spawns, seeded and reload-proof.

### Build / test / capture results

Built from a **Developer PowerShell for VS 2022** shell (per README / the
[build-toolchain memory](../../../memory/build-toolchain-vs2022.md)).

- `cmake --build --preset debug` → exit 0, no project warnings.
- `cmake --build --preset release` → exit 0, no project warnings.
- `build-msvc\crystal_tests.exe` → **All tests passed (62101 assertions in 497
  test cases)**; `build-msvc-rel\crystal_tests.exe` → same 497/497.
- `ctest --preset debug` and `ctest --preset release` → **100% passed, 497/497**
  each.
- `build-msvc\CrystalDungeons.exe --capture <dir>` → **64/64 scenes clean**
  (was 61; +`62_battle_log` / `63_equip_diff` / `64_settings_audio`;
  `41_bestiary_king` re-verified clean after the layout fix).

Test count grew 485 → **497** (+12 new `test_comforts` cases); the hidden `[.]`
sweep batteries are unchanged.
