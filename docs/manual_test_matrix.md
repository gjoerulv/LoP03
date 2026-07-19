# Manual Test Matrix (M11 baseline)

> Companion to `docs/presentation_audit.md`. Status legend: **pass** /
> **fail(ID)** (links a defect) / **partial** / **not run**. The M11 session
> filled what could be verified by automated keyboard driving and screenshot
> review; everything else is owner work. Update this file as rows are run —
> it stays the living pre-release matrix through M24.
>
> **M13 update:** left-stick navigation now exists (run the stick pass for
> real); row 6 name-editing on gamepad is **expect pass** (A/B finish, X
> deletes — the Blocker fix); rows 3/17 prompts now show live binding labels
> and must update after remapping. New checks: Settings screen (volumes
> audible immediately, window toggle, battle/message speed take effect),
> Remap Keyboard/Gamepad (listen flow, [Esc] cancel, conflict swap message,
> reset, persistence across restart), corrupt `settings.json` recovery, and
> settings reachable from the title before New Game. Post-M13 captures:
> `docs/screenshots/m13_after/`.
>
> **M12 update:** rows previously marked *expect fail* for UI-TEXT-001/002,
> UI-LAYOUT-003, and the scoreboard/HUD text defects are now **expect pass —
> verify**: the fixes are implemented and unit-tested, and rows 1/3/9 were
> re-verified with post-fix captures (`docs/screenshots/m12_after/`). While
> running any row, also watch the console for `[ui-overflow]` warnings — any
> occurrence is a defect even if the screen looks acceptable. Battle rows
> gain new checks: skill/item description panel shows for the selected
> entry; command menu fully visible (UI-TEXT-024); shops show the detail
> line; scoreboard scrolls with the range indicator.

## How to run

Build and launch (Debug is fine for the baseline):

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build-msvc
.\build-msvc\CrystalDungeons.exe
```

**Input passes:** run the matrix once keyboard-only, once gamepad-D-pad-only.
A left-stick pass is expected to fail wholesale until M13 (defect CTRL-006:
no axis support) — verify and note it, don't file new defects per row.

**Display cases** (apply to every screenshot row; spot-check others):

| Case | How |
|---|---|
| W1 default 1278×720 | launch size |
| W2 maximized/1920×1080 | maximize |
| W3 small window ≈426×240 | drag to minimum-ish |
| W4 narrow (tall) window | drag width down |
| W5 near-square window | drag to ≈700×700 |
| W6 non-integer scale | any odd size; look for shimmer/blur |

**Screenshots:** save to `docs/screenshots/m11_baseline/` using the listed
filename. Existing captures 01–06 were taken this session at W1.

## Matrix

Columns: KB = keyboard pass, DP = gamepad D-pad pass, Text = fits/readably
wrapped, Focus = selection visible & correct, Audio = expected placeholder
sound fires.

| # | Screen / variant | Data case | KB | DP | Text | Focus | Audio | Evidence file | Status |
|---|---|---|---|---|---|---|---|---|---|
| 1 | Title / main menu | fresh launch | pass | not run | pass | pass | not run | 01_title_main_menu.png | partial |
| 2 | Title: Continue disabled | no save files (rename `%APPDATA%\CrystalDungeons\saves`) | not run | not run | not run | not run | — | 07_title_no_saves.png | not run |
| 3 | Help / controls | — | pass | not run | pass | n/a | not run | 02_help_controls.png | partial — table claims Left Stick: **fail(CTRL-006)** pending pad check |
| 4 | Party creation | default names | pass | not run | pass | pass | not run | 03_party_creation.png | partial |
| 5 | Party creation: max names | all four names 12 chars (e.g. `Brandelbrook`) | not run | not run | not run | not run | — | 08_party_max_names.png | not run |
| 6 | Name editing modal | type/backspace/finish | pass (auto) | **expect fail(UI-INPUT-001)** | pass | pass | not run | 04_party_name_editing.png | partial |
| 7 | Save slots (Save mode) | 0, 1, 3 slots used; overwrite | not run | not run | not run | not run | not run | 09_save_slots.png | not run |
| 8 | Load slots (Continue) | autosave + manual mix; empty disabled | not run | not run | not run | not run | not run | 10_load_slots.png | not run |
| 9 | Town exploration | 4×12-char names in HUD | partial | not run | **check UI-TEXT-008 spill** | n/a | not run | 05_town.png (default names), 11_town_max_names.png | partial |
| 10 | Town pause menu | — | pass | not run | pass | pass | not run | 06_town_pause_menu.png | partial |
| 11 | Quit to Title (unsaved) | progress since last save | not run | not run | n/a | n/a | — | — | not run — expect no confirm (CTRL-011) |
| 12 | Inn | hurt party, then full | not run | not run | check %-pad columns (UI-TEXT-017) | n/a | heal sfx | 12_inn.png | not run |
| 13 | Item shop | buy; not-enough-gold; owned counts | not run | not run | not run | not run | confirm/cancel sfx | 13_item_shop.png | not run |
| 14 | Equip shop: buy list | scroll to last of 25 items | not run | not run | **expect fail(UI-TEXT-001)** | **expect fail** | not run | 14_equip_buy_overflow.png | not run |
| 15 | Equip shop: equip flow | char→slot→item, unequip, swap | not run | not run | not run | not run | not run | 15_equip_flow.png | not run |
| 16 | Training hall | affordable, poor, max-level rows | not run | not run | check pad columns | pass? | heal sfx | 16_training_hall.png | not run |
| 17 | Guild | each theme; depth 1 and 20; reroll | not run | not run | 20-digit seed fits? | not run | not run | 17_guild.png | not run |
| 18 | Scoreboard: empty | fresh scoreboard | not run | not run | not run | n/a | not run | 18_scoreboard_empty.png | not run |
| 19 | Scoreboard: 14+ runs | after many clears (or hand-edit scoreboard JSON) | not run | not run | **check UI-TEXT-012** (columns, cap) | n/a | not run | 19_scoreboard_full.png | not run |
| 20 | Dungeon exploration | each theme, depth 1 + deep | not run | not run | danger labels legible? | n/a | dungeon music | 20_dungeon_<theme>.png | not run |
| 21 | Encounter prompt | gate, guard, boss; many tags | not run | not run | **check UI-INFO-005** (no team name) | n/a | not run | 21_encounter_prompt.png | not run |
| 22 | Chest prompts | unguarded, guarded, opened, empty | not run | not run | long item names in message | n/a | chest sfx | 22_chest.png | not run |
| 23 | Dungeon pause / retreat | mid-run | not run | not run | pass | pass | not run | 23_dungeon_pause.png | not run |
| 24 | Battle: intro + boss telegraph | boss with longest telegraph (Sorcerer) | not run | not run | 55-char line fits? | n/a | battle music | 24_battle_boss_intro.png | not run |
| 25 | Battle: command menu | item/skill rows disabled when none | not run | not run | pass? | pass? | move/confirm sfx | 25_battle_command.png | not run |
| 26 | Battle: skill list, 5+ skills | high-level char or scroll-taught | not run | not run | **expect fail(UI-TEXT-002)** | **expect fail** | not run | 26_battle_skills_overflow.png | not run |
| 27 | Battle: item list, many types | carry 6+ consumable types | not run | not run | **expect fail(UI-TEXT-002)** | not run | not run | 27_battle_items.png | not run |
| 28 | Battle: 5-enemy encounter | deep-dungeon gate team | not run | not run | **expect fail(UI-LAYOUT-003)** | target marker visible? | not run | 28_battle_5_enemies.png | not run |
| 29 | Battle: status-heavy | poison + buffs/debuffs on several units | not run | not run | status rows overlap? | n/a | not run | 29_battle_statuses.png | not run |
| 30 | Battle: outcomes | victory (+XP/gold msg), escape, defeat | not run | not run | defeat: gold-loss unexplained (**UI-INFO-014**) | n/a | victory/defeat sfx | 30_battle_outcome.png | not run |
| 31 | Dungeon result | with and without escapes line | not run | not run | 8-line fit (UI-LAYOUT-018) | n/a | victory sfx | 31_dungeon_result.png | not run |
| 32 | Malformed data startup | temporarily break `data/classes.json` | not run | n/a | error path readable; no crash | n/a | n/a | 32_degraded_startup.png | not run |
| 33 | Malformed save load | hand-corrupt a slot file | not run | not run | "Load failed" message | n/a | n/a | — | not run |
| 34 | Controller disconnect | unplug pad in a menu and in town | n/a | not run | n/a | recovers to KB? | n/a | — | not run |
| 35 | Audio-device failure | launch with audio device disabled | not run | n/a | n/a | n/a | silent no-crash | — | not run |
| 36 | Window scaling W2–W6 | rows 1,3,4,9,20,25 at each case | not run | not run | letterbox clean; no blur at W6? | n/a | n/a | 33_scaling_<case>.png | not run |

## Session-verified summary (2026-07-19, automated keyboard driving)

- Rows 1, 3, 4, 6, 9, 10 partially verified at W1 with screenshots 01–06;
  no overflow observed on those screens with default data.
- Build/tests: 125/125 pass; exe clean start/exit (row-independent).
- Everything marked **expect fail(ID)** is a geometry- or code-certain defect
  from the register — the run confirms severity/frequency, not existence.

## Owner notes

- Rows 14, 26–28 are the fastest way to see the worst text defects live.
- Row 6 on gamepad is the Blocker (UI-INPUT-001): have a keyboard within
  reach when you test it.
- File new defects in `docs/presentation_audit.md` with the next free ID in
  the matching category.
