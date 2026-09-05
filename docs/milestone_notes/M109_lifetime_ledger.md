# M109 — Lifetime ledger (persistent telemetry foundation)

**Status:** implemented, awaiting manual approval
**Program:** M109–M116 (owner-authorized 2026-09-02 via the approved plan).
No battle-rules, generation or save-version bump: the ledger is one
additive optional save object (`"lifetime"`), the only pure-model change
is record-only telemetry (the M60 parity proof holds), and every seam is
an explicit call at the site that already decided the fact.

## Scope (plan section M109)

A persistent, display-only lifetime ledger for the current save — per
member, global combat, economy, per town, patrols, exploration, a defeat
ledger keyed by content id, and an active-play clock — recorded at the
authoritative seams, with old-save migration and the zero-stakes
exclusions (sparring, Simulator, editor, capture, tests). No player-facing
screen yet: the End-game Summary (M116) reads it. The `patrols` group's
special-kind counters exist now and are incremented by M110–M112.

## What was built

- **Model** — `src/game/Lifetime.hpp`: `LifetimeStats` (int64 counters in
  `members[4]` / `combat` / `economy` / `towns[7]` / `patrols` / `explore`,
  a `defeats` map, `migrated`), the slot-keyed identity rule, the pure
  `recordLifetimeHit` / `migrateLifetime` helpers, and the **field tables**
  that are the single serialization contract (writer, reader and tests
  iterate them). `Party.lifetime` is the member; `resetForNewGame` covers
  it (whole-object reset).
- **Persistence** — `SaveSystem.cpp` writes the nested object and reads it
  through the new `ObjectReader::optInt64` (type-checked, no minimum) with
  inline clamping, so a below-zero counter degrades to 0 instead of
  failing the load; a wrong type still reports like every other field.
  The defeat ledger is read off the raw JSON, capped at 8192 entries, and
  deliberately not validated against the content database (the M42
  `encountered` precedent). Absent block → zeros + `migrated` + the one
  derivable backfill (`recordBiggestHit` → `combat.highestHit`, author
  unknown); `recordRunDamage` is a run peak and is not backfilled.
- **Battle seam** — `src/game/BattleTelemetry.hpp`: `LifetimeHook{stats,
  town}` rides the launch payloads (`BossIntroState` → `BattleState`,
  trailing defaulted parameters); `BattleState` owns the `BattleTelemetry`
  observer and attaches it only when a hook carries stats (DungeonState
  with the run's town; CastleChallengeState and TreasureFightState with
  town 0; SparState and every capture scene pass none). The observer
  vocabulary grew additively in `src/battle/`: Damage/KO events carry the
  attacker (`applyDamage` gained a trailing `attacker` parameter — the
  deliberate hit's actor, the thorns bearer, the counter-attacker; poison
  ticks stay unattributed), plus `Guard` and `StatusApplied` events; the
  free `addStatus` now returns whether the status landed, and the outer
  functions that know the actor emit only then. `recordBattleEnd` runs
  once in `BattleState::finish` (outcomes, turns, the boss-encounter count,
  the defeat ledger with the summon-slot exclusion). The editor's
  `BattleRecorder` switch gained the new cases (no behaviour change).
- **Economy seam** — `src/game/Ledger.hpp` (`earnGold` / `spendGold` /
  `loseGold`, tokens, purchases, finds, curios, map treasures, scroll
  learns, level-ups) routed at every production gold/token site: battle
  spoils and level-ups (`Spoils.hpp`), chests, every dungeon event (shrine,
  merchant, level altar, token changer, reels incl. the tax-papers "prize",
  blackjack stake and payouts, miner's cache, elder root with a level
  snapshot around `grantPartyXp`, duck peddler, surveyor), the elite
  challenge token, boss drops, the buried-treasure token/curio, the defeat
  halving (`goldLost` + the town's wipe), the Inn (`innGold`), item and
  equipment shops, the Training Hall (tuition + passives), the black
  market (gold and token prices), castle first-clear rewards, the treasure
  dig (completion, fallback riches, the learned scroll) and the Party
  panel's scroll learn.
- **Exploration seams** — run attempts at the Guild's two entry points
  (before the entry autosave), completions + town clears + best score in
  `completeDungeon`, retreats (dungeon pause menu), wipes (defeat branch),
  floors cleared at each felled stair-gate and the completion (Eternal
  floors cumulatively), tiles in the M93 counted-tile block, chests, every
  event-resolved site (ten, across DungeonState / BlackjackEventState /
  ArmoryGhostState), patrols at the trigger (total + normal + town; M110
  refines the kinds), and the encounter-level King / Duck / Dragon / Guild
  Master counters in `CastleChallengeState::finish` (once per cleared
  gauntlet — distinct from the per-unit defeat ledger).
- **Play clock** — `GameState::pausesPlayClock()` (true for the main menu,
  slot menus, Settings, remap, Help, both pause menus, the debug menu);
  `Application::processFrame` accumulates `min(dt, 0.25)` while a party is
  loaded, the window has focus and the top state does not pause the clock,
  flushing whole seconds into `explore.playSeconds`.
- **Debug** — a read-only "Lifetime (read-only)" row on both debug menus
  (fights recorded, play time, a "(migrated)" tag) for smoke checks.

## Routine assumptions (recorded per the plan)

- The defeat halving is a LOSS (`goldLost`), never a spend.
- Defeat-ledger ids are not content-validated; every other list still is.
- The play clock gates on window focus only (the Background Audio setting
  is about sound, not play) and pauses on the menu states listed above.
- Per-town battle statistics attribute dungeon fights only; castle, pond,
  guild-hall and treasure-dig fights count in the global combat totals.
- "Damage dealt" counts non-poison hits by a member ON A FOE (striking a
  confused ally is nobody's feat); "damage taken" counts every HP a member
  loses, poison included; "healing" is what members restore (in battle —
  the Inn, shrines and springs are not combat healing).
- Tiles walked are dungeon tiles (the M93 counter's own unit).
- The BaldHead reel scroll is a find (`treasureFound`); it becomes a
  `scrollsLearned` only when the Party panel teaches it.

## Tests

- `tests/test_lifetime.cpp` [lifetime][save]: fresh defaults; the field
  tables name every counter exactly once (struct-size pins); the pure hit
  helper; the town accessor; a full round-trip with values above INT_MAX;
  old-save migration (zeros, the backfill, the flag persists); the
  degrade-vs-report reader rules (negative → 0 without failing; out-of-
  range member → unknown; malformed ledger entries skipped; a fifth
  member entry ignored; a wrong type still fails the load and leaves the
  target party untouched); the nested-object shape; slot-keyed history
  survives a rename; New Game; `optInt64`.
- `tests/test_battle_telemetry.cpp` [lifetime][telemetry]: attaching the
  observer changes nothing (three seeded gauntlets resolve byte-identically
  with and without it); hit attribution dealt/taken; KO attribution
  (finishing blows, the fallen, enemy KOs); guards, skills, statuses,
  heals and revives at their seams; a summon counts once and never on the
  M95 refusal; `recordBattleEnd` outcomes/turns/ledger; the Dragon's clone
  never counts as a Dragon defeat while a KO'd clone still credits the raw
  tallies.
- `tests/test_ledger.cpp` [lifetime][ledger]: earn/spend/lose, the largest
  purse as a peak, town attribution, the Inn line, tokens, counts.
- The pre-existing observer parity tests and the whole `[battle]` suite
  run unmodified and green (the model change is emit-only).

## Deviations from the plan

- Events resolved are counted by an increment beside each existing
  `resolved = true` site rather than a `markEventResolved` helper — the
  same ten sites, one line each, nothing to keep in sync.
- The plan's `trackedSinceVersion` string was dropped: `migrated` alone
  carries the "tracking began with this version" note (the version is the
  build's own stamp).
- `summaryShown` (M116's flow flag) is NOT added here — it lands with the
  summary.

## Compatibility

- Saves: schema v1 unchanged; the `lifetime` object is optional and
  additive. Pre-M109 saves load with zeros, `migrated`, and the biggest-hit
  backfill; a hand-edited ledger degrades value-wise and reports type-wise.
- Battle rules (18), generation (23), settings, achievements, profile:
  unchanged. The observer emit sites never move `rollCursor` (parity test).
- Content, manifest, packaging: untouched.

## Automated validation

Filled in by the completion report below.

## Manual owner checklist

Matrix row **213**: play a run touching every seam (a fight, a chest, an
event, a patrol, a retreat, a wipe), shop/rest/train in town, save and
reload — the debug menu's read-only Lifetime row grows and survives the
reload; a pre-M109 save shows zero fights with its old biggest hit
carried over and the "(migrated)" tag; the clock stops in the pause menus,
Settings and the main menu; a spar leaves every number as it was; nothing
in ordinary play looks or feels different. Send back the save file and the
row's text on any mismatch.

## Documentation updated

`docs/milestones.md` (rows 109–116, the program paragraph and section),
`docs/game_design.md` (the Lifetime statistics passage),
`docs/technical_design.md` (§51), `docs/manual_test_matrix.md` (row 213),
this note.

## Completion report

### 1. Implementation summary

- **Milestone:** M109 — Lifetime ledger.
- **Slices completed:** model + field tables; save writer/reader +
  `optInt64` + migration; observer extension + `BattleTelemetry` +
  `LifetimeHook` wiring; `Ledger.hpp` at every economy site; exploration
  and castle seams; the play clock; the debug smoke row; tests; docs. All
  completed.
- **Player-facing changes:** none visible (the ledger is silent until
  M116); the dev-only debug menu gains a read-only row.
- **Engineering changes:** `game/Lifetime.hpp`, `game/BattleTelemetry.hpp`,
  `game/Ledger.hpp` (new); `Party.lifetime`; `ObjectReader::optInt64`;
  `BattleEvent` Guard/StatusApplied + attacker attribution; `addStatus`
  returns bool; `GameState::pausesPlayClock`; `BattleState`/`BossIntroState`
  trailing `LifetimeHook` parameter.

### 2. Files changed

- **Source:** `src/game/{Lifetime,BattleTelemetry,Ledger}.hpp` (new),
  `src/game/{Party,Spoils}.hpp`, `src/save/SaveSystem.cpp`,
  `src/content/JsonValidation.{hpp,cpp}`, `src/battle/{Battle.hpp,
  Battle.cpp,BattleObserver.hpp}`, `src/editor/BattleRecorder.hpp`,
  `src/core/Application.{hpp,cpp}`, `src/states/GameState.hpp`,
  `src/states/{BattleState,BossIntroState}.{hpp,cpp}`,
  `src/states/{DungeonState,DungeonMenuState,GuildState,
  CastleChallengeState,TreasureFightState,InnState,ItemShopState,
  EquipShopState,TrainingHallState,BlackMarketState,BlackjackEventState,
  ArmoryGhostState,PartyState,DebugMenuState}.cpp`, the eight menu-state
  headers (`pausesPlayClock` overrides), `src/states/DebugMenuState.hpp`.
- **Tests:** `tests/test_lifetime.cpp`, `tests/test_battle_telemetry.cpp`,
  `tests/test_ledger.cpp` (new), `tests/CMakeLists.txt`.
- **Content/data:** none.
- **Documentation:** as listed above.
- **Build/release configuration:** none.

### 3. Plan deviations

See "Deviations from the plan" above — three routine local decisions, no
owner approval required.

### 4. Compatibility

See "Compatibility" above: no impact on deterministic seeds, score
records, packaged assets, settings or content schemas; saves gain one
optional object.

### 5. Automated validation

- **Configure:** `cmake --preset msvc-debug` (VS 2022 developer shell).
- **Build:** `cmake --build --preset debug` — clean, zero project-code
  warnings.
- **Targeted tests:** `crystal_tests.exe "[lifetime]"` — 25 cases, 664
  assertions green; `"observer*"` — 3 cases green; `"[battle]"` — green
  (the unmodified suites).
- **Full suite:** `ctest --preset debug` — **all tests passed** on the
  final M109 build (the run recorded in the session log; count in the
  program's closing report).
- **Capture lint:** `ArePGeese.exe --capture <dir>` — exit 0, **127/127
  scenes clean** (the debug menu scene re-rendered with the new row).
- **Release:** not built for this milestone alone — the program's closing
  battery builds and tests the Release preset (`cmake --build --preset
  release` then `ctest --preset release`, expected all green); reported
  there.

### 6. Manual owner validation

Matrix row 213 (above).

### 7. Known limitations

- Healing done outside battle (Inn, shrine, spring) is not a combat
  statistic by design.
- The clock counts a cutscene, a shop or a battle log as play; only the
  listed menu states pause it.

### 8. Documentation updated

As listed above.

### 9. Final status

`implemented, awaiting manual approval`
