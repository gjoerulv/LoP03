# Current packaged-build release validation

The older rows below are retained as historical baseline evidence and are **not** a final M24 pass. Record the current packaged-build run using `docs/release_hardening_manual_checklist.md`.

- Build SHA: _not run_
- Package version: _not run_
- Tester/date: _not run_
- Display/input cases: _not run_
- Final result: _not run_

---

# Manual Test Matrix (M11 baseline)

> Companion to `docs/presentation_audit.md`. Status legend: **pass** /
> **fail(ID)** (links a defect) / **partial** / **not run**. The M11 session
> filled what could be verified by automated keyboard driving and screenshot
> review; everything else is owner work. Update this file as rows are run —
> it stays the living pre-release matrix through M24.
>
> **M27 update:** environment & ambience identity. Each of the six town
> services (Inn, Item Shop, Equip Shop, Training Hall, Scoreboard, Guild) now
> draws its own full-screen background instead of a flat fill — verify at W1–W6
> that every one reads as a distinct place **and every line of text stays
> readable over it** (the binding constraint); watch for any `[ui-overflow]`.
> Ambience: a blind listen should distinguish the town / Ruined Keep / Crystal
> Mine / Hollow Forest beds — each has its own signature: **town** a quiet
> breeze with frequent high bird whistles; **keep** a loud, hollow, beating low
> drone; **mine** a metallic hum under regular echoing water drips; **forest** a
> busy bright leaf rustle with low owl hoots. **The bed must switch per theme:**
> enter Ruined Keep / Crystal Mine / Hollow Forest dungeons from the Guild and
> confirm the ambience changes to that theme's bed (this was broken — the town
> bed used to play in every dungeon), and returning to town switches back.
> **Slider reroute (supersedes the M21-update note):** the
> **SFX** slider now moves ambience volume and the **Music** slider no longer
> does (music still follows Music) — check both in a dungeon. Missing-background
> drill: delete a `assets/textures/backgrounds/*.png` from the build → that
> service falls back to its old flat fill, no crash.
>
> **M25 update:** UI corrections + original bitmap font. Text now renders
> through a generated bitmap font (`font.ui.{small,main,title}`), not raylib's
> default — do a legibility pass across **every** screen at W1–W6 and judge the
> typography (owner art call); it should read as one clean pixel typeface, crisp
> at the small HUD sizes. The four reported defects (verify by eye; the
> `--capture` run is overflow-clean at 23/23 scenes incl. the new
> `23_battle_targeting`): (1) **Title** — the version stamp and the
> `Content: …` diagnostic sit on separate rows with no overlap in Debug, and
> the content line is **absent from a Release build** (`cmake --preset
> msvc-release`) while the version stamp remains; (2) **Battle** — no name is
> painted over any sprite; selecting a target shows a target-info panel (name,
> HP/MP, ATK/MAG/DEF/SPD, statuses) for the targeted unit only, not colliding
> with the status lines; check it with a full party vs a 5-enemy team; (3)
> **Battle MP** — every party member shows `HP c/m` and `MP c/m` numerals
> (check at full, partial, and zero MP); (4) **Guild** — Theme and Depth values
> render inline on their menu rows and update as you press Left/Right; the seed
> is a separate readout. Missing-font drill: delete `assets/fonts/*.fnt` from
> the build's assets → text falls back to the default font, no crash.
>
> **M23 update:** validation + playtesting. The capture set
> (`build-msvc\CrystalDungeons.exe --capture docs\screenshots\m23_captures`)
> replaces ad-hoc screenshots: 23 deterministic native-res scenes,
> self-checking for text overflow — review it for visual regressions after
> any UI change. External playtests follow `docs/playtest_protocol.md`
> (six profiles, uncoached, observation sheets); their findings are the
> M23 hardening input. Balance: early ramp retuned (generation v4);
> verify in play that depth 4 feels like a step up from depth 1-3 and
> depth 6+ demands leveling. Release builds must not react to
> `--capture`.
>
> **M22 update:** onboarding + accessibility. Checks, on a fresh
> `tutorial.json` (delete it or use Settings > "Reset tutorial prompts"):
> each of the nine one-time prompts appears exactly once at the right
> moment (town, guild, dungeon, battle, guarded chest, event, first
> victory, result, return to town), reads clearly, dismisses with one
> Confirm, and never reappears across restarts; Settings > "Tutorial
> Prompts: Off" suppresses all of them; a hand-corrupted tutorial.json
> starts fresh without crashing. **Details** ([C]/gamepad Y, remappable in
> both Remap screens): opens/closes cleanly in battle (focused unit stats
> + statuses), dungeon (danger tiers + faced team), result/scoreboard
> (score components), and equip-shop Buy (per-member gear deltas); footers
> advertise it. **High Contrast On**: every screen stays readable and
> nothing relies on color alone; toggle applies instantly and persists.
> Destructive confirmations: saving over an existing slot and Quit to
> Title both demand a second Confirm with a visible warning, and cursor
> movement/Cancel disarms them. Settings and Controls remain reachable
> from the title screen before New Game.
>
> **M21 update:** the full original soundscape ships (11 music tracks, 4
> ambience beds, 15 SFX — all generated, all manifest-driven). Listening
> pass, every scene and transition: title → town (music + light ambience) →
> guild (preparation track) → each dungeon theme (distinct track + bed:
> keep wind / mine drips / forest rustle) → normal battle → boss battle
> (heavier track) → victory fanfare / defeat dirge (one-shot jingles) →
> result (calm track) → back to town. Checks: no two music tracks ever
> stack (0.25 s crossfade on every switch); loop seams free of clicks;
> footsteps cadence while walking (town + dungeon) without spamming; doors,
> chests, merchant/omen interactions, and "cannot pay/afford" refusals each
> have a distinct sound (refusals buzz, not the cancel blip); physical vs
> magic hits vs status casts sound different in battle; volume sliders
> (master/music/SFX) behave, persist, and ambience follows the music
> slider; nothing clips at maximum volumes; rapid menu scrolling sounds
> clean; delete any WAV from the build's `assets/audio/` → silence or
> synth fallback + a log line, never a crash (drill scripted: title.wav);
> a full run muted loses no essential information. Final soundscape
> acceptance is the owner's subjective call.
>
> **M20 update:** dungeons gain composed encounters, events, and boss
> mechanics. Rows 20–24 checks: teams feel role-coherent (never two
> healers/buffers; always a damage threat); depth 8+ runs are markedly
> harder than depth 4 (bigger teams, more elites, stronger stats, labels
> still honest); find and trigger **every event** — shrine, spring,
> merchant, elite challenge (double danger credited on the result), omen
> wager (result shows the +150/−100 line), and a trapped chest (red-tinted,
> wounds stated before taking) — each trade-off must be fully readable in
> the footer *before* pressing Confirm; fight all four bosses and confirm
> the telegraphed mechanic visibly happens (rage announcement, empowered
> magic after minion kills, one rally below half HP, doubled opening blow).
> Captures: `docs/screenshots/m20_events/`.
>
> **M30 update:** the **Inn is now a paid rest** (row 12). Check: the cost is
> shown and scales up as the party levels; resting deducts gold and fully
> restores HP/MP; "Rest" is disabled with a clear reason when you cannot afford
> it or are already rested (never a dead end); leaving with Back is fine. Find
> the new **rest-camp event** in a dungeon (campfire marker "R"), confirm it
> grants a **free-rest token**, return to town, and spend the token at the inn
> for a free full rest (the token row appears only when one is held). Load a
> **pre-M30 save** and confirm it works with zero tokens.
>
> **M29 update:** classes now **learn skills as they level**. Check the battle
> **Skill** menu gains entries at the learn levels (e.g. Knight → Shield Bash 4,
> Whirlwind 10, Execute 16); the new enemies/bosses (Corpse Hound, Gloom Priest,
> Rune Sentry, Bone Colossus, Void Weaver, The Deep King, The Blight Matron,
> …) read clearly at native resolution and their roles behave (healers heal,
> disruptors debuff-and-blast, buffers buff their side).
>
> **M19 update:** the scoreboard gains an **Lv** column (party level at
> completion; "-" for pre-M19 runs) and a two-line legend stating the
> comparison conditions. Rows 18/19 checks: Lv shows for new runs and "-"
> for old entries; the legend is readable; columns still fit at every
> window case; an old scoreboard.json still loads. Economy spot-checks for
> row 16: one depth-1 clear must not fund training the whole party a level
> at mid-game prices; a fresh party still clears depth 1. Capture:
> `docs/screenshots/m19_score/`.
>
> **M18 update:** battle actions are staged (lunge → impact flash/shake +
> numbers + HP-bar commit → message). Battle rows 24–30 gain checks: at
> Normal speed each action reads clearly and briskly; Fast visibly halves
> the staging; Instant shows results at once; Confirm skips any moment of a
> sequence without losing information; Settings' new **Battle Flash** and
> **Battle Shake** rows (reduced/off) are visibly honored in the next fight;
> fallen enemies sink from the field while KO'd allies stay (revivable);
> a grayed command explains itself ("No skills learned." / "No usable
> items."); defeat states its consequences (town return, half gold, run
> forfeit); and a full fight is understandable with audio muted. Captures:
> `docs/screenshots/m18_battle/` (settings rows, target select, live impact
> flash + hit-stop, post-impact bar commit, enemy sunk after KO).
>
> **M17 update:** exploration has final-direction art in all three themes.
> New checks for rows 9/20: player walk animation plays while moving and
> freezes on a stand frame when stopped, facing matches movement (four
> directions); Crystal Mine (braced rock + crystal clusters) and Hollow
> Forest (trunk walls + root archways + shrine stones) render their own tile
> sets; sparse accent tiles appear but never obscure doors or markers; team
> markers are silhouettes whose **shape** differs by danger (plain / horned
> / crowned) with the text label still present; gold corner brackets pulse
> on the interactable you face (gate, guard, boss) or the chest you stand
> on; danger labels and brackets draw above everything. Verify by eye: a
> gate block, a guarded chest, and the boss arena with the crowned
> silhouette (scripted captures could not reach them — see the M17 note
> §N). Missing-asset drill: delete the `anim.player.walk.*` entries from
> `assets/manifest.json` → static player sprite, no crash; delete
> `actor.player.walk` too → same. Captures: `docs/screenshots/m17_explore/`.
>
> **M16 update:** dungeon rooms are now compact archetype layouts (Entry,
> Corridor, Crossroads, Gate Chamber, Treasure Alcove, Treasure Vault, Boss
> Antechamber, Boss Arena) drawn centered on a black backdrop instead of
> filling the screen. Row 20 gains checks: rooms visibly smaller than the
> screen and varied per archetype; door gaps centered on walls; pillars never
> block a door, chest, or encounter; entering a room places you just inside
> the door you used; minimap still matches reality; danger labels sit above
> markers. Walk at least: the entry room, a corridor, a treasure room
> (chest + guard beside it), a gate block, and the boss antechamber/arena
> (large room, boss marker centered). Same seed must reproduce the same
> rooms. Captures: `docs/screenshots/m16_rooms/`.
>
> **M15 update:** the slice screens (title, town, Ruined Keep, battle,
> pause/result panels) now render generated art + music via the manifest —
> judge them against `docs/art_bible.md`; captures in
> `docs/screenshots/m15_slice/`. Battle sprites and the framed battle panel
> were not reachable by scripted capture: verify rows 24–31 by eye (class
> sprites per party member, tier sprites on enemies, 32px boss, gray tint on
> KO, target box around sprites). Crystal Mine / Hollow Forest intentionally
> keep colored-rectangle tiles until M17.
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
| 14 | Equip shop: buy by category (M31) | Buy Gear → Weapons / Armor / Accessories → browse a filtered list; confirm the current-category name shows in the hint; Cancel steps back one level each (Buy→Category→Menu) | not run | not run | category menu + each filtered list fit (≤9 rows; capture-verified clean) | check cursor on each level | confirm/cancel sfx | 24_equip_categories.png, 09_equip_shop.png | not run |
| 14b | Equip shop: relics under Accessories (M31) | open Buy Accessories; confirm the 4 relics (Ember Charm, Titan Heart, Assassin Sigil, Guardian Crest) appear and are absent from Weapons/Armor | not run | not run | relic names fit rows | not run | not run | — | not run |
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
| 37 | Town travel (M32) | walk to the bottom-right road, Confirm to advance a town; bottom-left to go back; confirm fade + darker music each step | not run | not run | "Town n/7" indicator + exit signposts legible | travel prompt on the exit tile | town track changes per town | 25_town_ladder.png | not run |
| 38 | Locked next-town exit (M32) | in a town whose next is not yet unlocked, stand on the right road | not run | not run | "Clear a dungeon... open the road onward" hint fits | "Locked" signpost shown | error sfx on Confirm | 25_town_ladder.png | not run |
| 39 | Per-town identity (M32) | visit towns 1/4/7 exteriors + each service interior | not run | not run | every service keeps text within contrast/overflow over its per-town background | n/a | music grows more sinister town to town | — | not run |
| 40 | Town score bonus (M32) | clear a run in town ≥ 2; check the result "Town bonus (+N%)" row and the scoreboard "T#" tag | not run | not run | result panel fits the extra row; T# fits the Theme column | n/a | n/a | 19_result.png, 12_scoreboard.png | not run |
| 41 | Unlock + old-save compat (M32) | complete a dungeon (unlocks next town); load a pre-M32 save (starts in town 1) | not run | not run | n/a | n/a | n/a | — | not run |

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
