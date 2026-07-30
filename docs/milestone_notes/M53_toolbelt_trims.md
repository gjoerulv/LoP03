# M53 — Toolbelt & trims

> Program: **Adjustment program (M53–M56)**, authorized by the owner
> 2026-07-24. This is the first milestone in the program and runs first because
> the debug toolbelt accelerates manual testing of everything after it.
>
> Authority: `CLAUDE.md` > this note > `docs/milestones.md` > `docs/game_design.md`
> > `docs/technical_design.md`. This note is the exact scope for M53; §J is the
> as-implemented record.

## Goal

Four independent quality/comfort items, none of which changes anything a Release
binary does:

1. **Debug menu** (S1) — a development-only cheat console reachable from both
   pause menus, plus a battle **god mode**.
2. **Five manual save slots** (S2) — up from three, plus the autosave slot.
3. **Champion** achievement (S3) — becomes *"Defeat the Hollow King in N turns
   or fewer"* (N tuned via the simulator and reported).
4. **Weapon element** (S4) — shown in the equip shop (Buy / Equip / Details).

## Baseline

Re-audited against HEAD `07a13bb` (identical source to the plan's stated
`7444406` — the intervening commit was docs only). Verified at audit:
`kBattleRulesVersion` **10**, `kGenerationVersion` **10**, `kSaveVersion` **1**,
`kSettingsVersion` **1**, `kAchievementVersion` **1**. Debug baseline
**497/497 tests** green. No version bump in M53.

## Owner decisions (2026-07-24)

- **Champion** = *"Defeat the Hollow King in N turns or fewer"* reading
  `castleRecords.kingBestTurns`; N tuned by the simulator and reported. Name
  stays "Champion"; the `achievements.json` id set is untouched, so earned
  unlocks persist. Retro-unlock for a save already at/under N is acceptable and
  documented.
- **All four debug-menu extras** are included on top of the core cheat set:
  legendary-token stepper, instant dungeon clear, unlock reward classes, fill
  bestiary.

## S1 — Debug menu (debug builds only)

- **Gate: `CRYSTAL_DEBUG_OVERLAY`** ([CMakeLists.txt:146-150](../../CMakeLists.txt)),
  which the generator expression already omits from Release. Not
  `!CRYSTAL_SHIPPING_BUILD` (un-toggleable) and no new define.
- New `src/states/DebugMenuState.hpp/.cpp`; the whole `.cpp` body is inside
  `#ifdef CRYSTAL_DEBUG_OVERLAY` (the file is added to the target
  unconditionally — the `CaptureRunner`/BattleState-capture precedent). When the
  macro is off, the TU compiles to nothing.
- Opened from a **"Debug" row appended after Quit in BOTH pause menus**
  ([DungeonMenuState.cpp](../../src/states/DungeonMenuState.cpp),
  [TownMenuState.cpp](../../src/states/TownMenuState.cpp)) under the same ifdef —
  existing `constexpr` row indices untouched (`kDebug = kQuit + 1`), box height
  bumped under the ifdef.
- **Cheats home:** new `src/core/DebugCheats.hpp` — an *unconditional*
  `struct DebugCheats { bool godMode; bool requestDungeonClear; }` value member
  on `AppContext` (layout is macro-independent; the single `Application::context_`
  owns it). All readers/writers are ifdef'd on `CRYSTAL_DEBUG_OVERLAY`.
- **Rows** (SettingsState-style hand-rolled rows over `ui::Menu` + a Row enum;
  `Action` fires on Confirm, `Stepper` steps on MoveLeft/Right; scrolling list
  via `drawMenuScrolled` + `ui::ScrollWindow`, Inset frame, values right-aligned
  as the row `suffix`):
  1. `Lv <member>` ×party (1..99): set `level`, `xp=0`, `refreshCharacter`, full
     heal.
  2. Gold ±1000 (clamp 0..9,999,999).
  3. Legendary tokens ±1.
  4. Highest town stepper (1..7) + "Unlock castle" action.
  5. "Grant 5x each consumable" / "Grant 1x each legendary" actions.
  6. "Spawn black market" (town only) — fills `party.blackMarket`; footer hints
     to re-enter town so `TownState` rebuilds its markers.
  7. "God mode" toggle.
  8. "Instant dungeon clear" (dungeon only) — sets
     `cheats.requestDungeonClear`, popped back to the dungeon, consumed by
     `DungeonState::update` which routes through the **real**
     `completeDungeon()` so scoring/unlocks/market rolls stay honest.
  9. "Unlock reward classes": `profile.recordKingDefeated()`.
  10. "Fill bestiary": insert every enemy/boss id into `party.encountered`.
  - "Learn all skills": **dropped** — learnsets derive purely from level
    (`content::knownSkillsFor`, resolved in `buildBattle`), so the Lv-99 stepper
    already grants every skill.
- **God mode (the one Battle touch):** `battle::Battle` gains
  `bool debugPartyUnkillable = false;` inside `#ifndef CRYSTAL_SHIPPING_BUILD`.
  `applyDamage` becomes a private `Battle` member (all six callers are already
  `Battle` methods, so no call-site changes) reading the flag directly; the two
  ifdef'd, Iron-Will-shaped clamp sites are that chokepoint and the **poison
  tick** (which bypasses it). Set once in the BattleState ctor from
  `context_.cheats.godMode` (ifdef'd). The Simulator/tests never set it (default
  false ⇒ byte-identical streams); **no `kBattleRulesVersion` bump** — the flag
  does not exist in shipped binaries. Hygiene: scoreboard submission is skipped
  in `completeDungeon` when god mode is on (one ifdef at the submit site).

## S2 — Five manual save slots

- `SaveSlot` gains `Manual4, Manual5`; `kSaveSlotCount = 6`; stems
  `save_slot4/5`, names "Slot 4/5". **No `kSaveVersion` bump** (per-file schema
  untouched; old slot files load; new slots read "(empty)").
- [SlotMenuState.cpp](../../src/states/SlotMenuState.cpp) static tighter layout
  (no scrolling — a save picker wants all slots visible): title plaque y=14;
  `kRowsY=52`, `kRowH=24`; frame sized from the **actual** row count (also fixes
  the prior Save-mode overshoot); message banner moved to 204.
- Sweep the other slot enumerations: `MainMenuState` `anySaveExists`
  (loop over `kSaveSlotCount`) and the `43_slot_menu_load` capture scene.

## S3 — Champion achievement

- `Achievements.cpp` `champion` condition becomes
  `kingBestTurns > 0 && kingBestTurns <= kChampionKingTurns` (a constant beside
  the def; description "Defeat the Hollow King in N turns or fewer."). Kingslayer
  unchanged. N tuned by simulating strong parties vs the King and reported in §J.

## S4 — Weapon element in the equip shop

- Data (`ItemDef::element`), display names (`content::elementDisplayName`), and
  the chip primitive (`ui::drawChipRight`, `kChipH=11`) all exist — only wiring
  is missing, in three spots of
  [EquipShopState.cpp](../../src/states/EquipShopState.cpp): the Buy-phase info
  panel, the EquipItem phase (beside the candidate's diff), and the "Gear
  Details" overlay body (`Element: <name>` line).
- A small pure `elementAccent(Element, palette)` helper maps Fire→danger,
  Ice→crystal, Lightning→gold, Earth→success, Holy→gold, Dark→magic — reusing
  existing palette roles (no new palette entries). The battle target-panel chips
  at [BattleState.cpp:1497-1506](../../src/states/BattleState.cpp) are the idiom.

## Tests & capture

- `tests/test_save_system.cpp`: stems/names for all 6 slots, Manual5 round-trip,
  empty-slot summary.
- `tests/test_achievements.cpp`: Champion predicate cases (turns 0 / N / N+1;
  old-unlock persistence).
- `tests/test_battle.cpp`: god mode — lethal hit leaves a party unit at 1 HP,
  enemies still die, poison tick clamped, flag-off battle byte-identical
  (outcome + `rollCursor`) to baseline, Iron Will interaction.
- Element-accent mapping helper test.
- Capture: slot picker Load scene refit with 6 rows; new `NN_debug_menu` scene
  (ifdef'd in CaptureRunner; longest labels, gold 9,999,999); equip-shop scenes
  re-verified with an element chip.

## Documents to update

`docs/game_design.md` (save slots, Champion condition, weapon-element display),
`docs/technical_design.md` (DebugCheats + god-mode clamps + debug menu),
`docs/manual_test_matrix.md` (new rows), `docs/milestones.md` (status),
`README.md` (debug-menu one-liner; save slots 3→5).

## Acceptance criteria

Debug menu reachable from both pause menus in Debug, absent from Release
binaries; god mode survives lethal hits AND poison with flag-off streams
byte-identical; 5 manual slots + autosave all visible and loadable at 426×240
with old saves intact; Champion = King ≤ N turns (N reported); weapon elements
visible in Buy/Equip/Details. Full suite green from 497 in Debug AND Release,
capture clean with the new scene, docs updated, status set to `implemented,
awaiting manual approval`.

## J. As-implemented record

Implemented 2026-07-24 on base checkout `07a13bb` (source-identical to the plan's
`7444406`). All four slices landed as scoped, with one small mechanical deviation
noted below. **No version bumps** (rules 10 / generation 10 / save 1 / settings 1
/ achievements 1, verified before and after).

**Deviation (routine, local):** `applyDamage` was a file-local free function
inside Battle.cpp's anonymous namespace, so promoting it to a `Battle` member
(the plan's approach for reading the god-mode flag without threading it through
six callers) required **moving its definition out of the anonymous namespace** to
`cd::battle` scope — a member cannot be defined inside an anonymous namespace
(MSVC C2888). Behaviour is unchanged; the six call sites (all `Battle` methods)
are untouched. No owner approval needed (no rule/schema/behaviour change).

**Champion N — tuning report.** The `[king-report]` battery (a scripted maxed
party vs the King at the shipped 500 % scale, effective HP 3750) records the
`rounds` each win takes — which is exactly the value stored as
`castleRecords.kingBestTurns`:

| Loadout | Outcome | Turns (rounds) |
|---|---|---|
| nothing / 30 snacks / 1+1+20 / 2+2+30 | loss | — |
| 3 sheets + 3 geese + 30 snacks | win | 21 |
| + Crown | win | 20 |
| + Crown + Spoon (full optimal) | win | **12** |

The only sub-15-turn win is the fully-optimized loadout (12 turns); every
adequate-but-not-optimal win lands at 20–21. **N = 15 (`kChampionKingTurns`) is
therefore well-calibrated**: it cleanly separates an *optimal* King kill
(Champion) from an *adequate* one (Kingslayer only), is reachable with the game's
best tools (12 < 15), and — because the sim party is a skill floor, not a
ceiling — leaves a strong human margin. Kept at the owner's chosen 15; adjust
only with new data.

**As-built specifics.**
- `applyDamage` is now a private `Battle` member with two ifdef'd
  (`CRYSTAL_SHIPPING_BUILD`) clamp sites (the chokepoint + the poison tick);
  `debugPartyUnkillable` is the only new Battle field, compiled out of Release.
- `DebugCheats { godMode; requestDungeonClear; }` is an unconditional AppContext
  value member; `DebugMenuState.cpp` body is entirely under
  `CRYSTAL_DEBUG_OVERLAY`; the "Debug" row is appended after Quit in both pause
  menus (`kDebug = kQuit + 1`, box +18) under the same ifdef. "Instant dungeon
  clear" routes through the real `completeDungeon()`; `submitScore` skips the
  scoreboard when god mode is on. "Learn all skills" was dropped (level-derived).
- `SaveSlot::{Manual4,Manual5}`, `kSaveSlotCount = 6`; `SlotMenuState` static
  six-row layout; `MainMenuState::anySaveExists` and the slot capture scene
  iterate all slots.
- `champion` predicate reads `kingBestTurns` vs `kChampionKingTurns`;
  description updated; retro-unlock documented; Kingslayer unchanged.
- `ui::elementAccent` (pure, `states/ElementChip.hpp`) + `drawChipRight` wiring in
  the Buy panel, EquipItem diff row, and Gear Details body.

**Validation (this session).**
- Configure: `cmake --preset msvc-debug` / `cmake --preset msvc-release` — OK.
- Build: `cmake --build --preset debug` / `--preset release` — OK, no project
  warnings/errors introduced.
- Debug tests: `ctest --preset debug` → **505/505 passed** (497 baseline + 8 new:
  2 save-slot, 1 Champion, 1 element-accent, 4 god-mode).
- Release tests: `ctest --preset release` → **501/501 passed** (the 4 god-mode
  cases are `#ifndef CRYSTAL_SHIPPING_BUILD`, so absent from the shipping build —
  the feature genuinely does not exist there).
- Capture: `CrystalDungeons.exe --capture` → **65/65 scenes clean** (64 baseline
  + `65_debug_menu`; `43_slot_menu_load` refit to six rows; equip-shop scenes
  re-verified with the element chip).

Debug and Release test counts differ **by design** — god mode is compiled out of
Release, so its four tests are too.
