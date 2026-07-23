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
> **M52 update:** comforts & secrets (the capture set is **64 scenes**). Checks:
> **(1) Ambience volume** — Settings → Audio now lists **Ambience Volume**
> between SFX and Background Audio (default **5**); lower it in a dungeon and the
> theme bed gets quieter while music and SFX are unaffected; it persists across a
> restart; a `settings.json` with no `audio.ambience` field loads at 5. **(2)
> Battle log** — in any battle, the **Menu/Pause** key opens a scrollable overlay
> of the last actions (exactly the lines the battle showed, newest at the
> bottom, Up/Down scroll); the same key or Cancel closes it; it never changes the
> fight and is gone in the next battle. **(3) Equip shop** — Buy rows show an
> owned-count column beside the price; the equip flow (char → slot → item) shows
> the **current** item in that slot and the **stat diff** of the highlighted
> candidate (green gain / coral loss / dim no-change), including Unequip. **(4)
> Bestiary max stats** — every known foe shows a `max` line under its base stats;
> spot-check the **King** (base HP 750, max 3750) and confirm the panel is not
> clipped; unknown foes still read `?`. **(5) The Crown's secret** — in the King
> fight, let his court fall and use the **Dragon Crown** on the King; his Royal
> Guards must **never come back** for the rest of the fight, and the game shows
> **no message** saying so (it should be discoverable-but-unexplained). Used on
> anyone but the King it does nothing and is kept. **(6) High-stakes market** —
> beat a boss at **town 7, depth 20+** on a **penalized/score-0** run
> (accept a wager you can't out-turn, or a stakes penalty); over a few such runs
> the black-market dealer should appear noticeably more often than the old 20 %
> would explain, and reloading the entry autosave cannot reroll a given run's
> result.
>
> **M46 update:** every screen was restyled by the procedural UI kit
> (selection slabs + chevrons, keycap footers, framed meters, header bands,
> shape-iconed banners; see `docs/ui_style_guide.md`). Rows below describe
> **content and behavior**, which are unchanged — read visual phrasing
> ("yellow cursor", "footer string") as the M46 equivalents. Capture scene
> filenames remain valid (the set is 51 scenes as of M46, 64 as of M52).
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
> **Slider history (see the M52 update — this is now superseded):** M27 moved
> ambience off the Music slider and onto the **SFX** slider; **M52 gives ambience
> its own slider entirely** (see the M52 update below). Missing-background
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
> magic hits vs status casts sound different in battle; the volume sliders
> (master/music/SFX/ambience since M52 — ambience originally followed music,
> then SFX) behave and persist; nothing clips at maximum volumes; rapid menu
> scrolling sounds
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
| 11 | Quit prompt from town (M42 fix; **3 answers since M47 — see row 115**) | progress since last save; try Cancel and "Keep Playing" | not run | not run | the prompt appears with the cursor on "Keep Playing"; Cancel and "Keep Playing" return to the pause menu with the run intact; only a deliberate pick leaves | n/a | confirm/cancel sfx | 44_quit_confirm.png | not run |
| 12 | Inn | hurt party, then full | not run | not run | check %-pad columns (UI-TEXT-017) | n/a | heal sfx | 12_inn.png | not run |
| 13 | Item shop | buy; not-enough-gold; owned counts | not run | not run | not run | not run | confirm/cancel sfx | 13_item_shop.png | not run |
| 14 | Equip shop: buy by category (M31) | Buy Gear → Weapons / Armor / Accessories → browse a filtered list; confirm the current-category name shows in the hint; Cancel steps back one level each (Buy→Category→Menu) | not run | not run | category menu + each filtered list fit (≤8 visible rows since M46, scrolling; capture-verified clean) | check cursor on each level | confirm/cancel sfx | 24_equip_categories.png, 09_equip_shop.png | not run |
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
| 37 | Town travel — walk-through (M32; reworked M50) | **walk into** the east road to advance a town, the west road to go back — no button; confirm fade + darker music each step; then travel back and forth repeatedly | not run | not run | compact centred town inside the M46 matte; "Town n/7" indicator + road signposts legible; arriving drops you just inside the matching edge; **no arrive→bounce loop** in either direction | player lands beside the road it came from | town track changes per town | 25_town_ladder.png, 59_town_road.png | not run |
| 37b | Castle road, no bounce (M50) | in town 7 walk up the north road into the castle, then leave the castle back to town | not run | not run | entering the castle is walk-through; **returning does not instantly bounce back in** — you land on the north road and must walk down off it | n/a | door sfx | 33_castle_hub.png | not run |
| 38 | Locked next-town exit (M32; M50) | in a town whose next is not yet unlocked, walk onto the right road | not run | not run | "Clear a dungeon... open the road onward" hint fits; **walking onto it does NOT travel** (locked road is inert) | "Locked" signpost shown | n/a | 25_town_ladder.png | not run |
| 39 | Per-town identity (M32) | visit towns 1/4/7 exteriors + each service interior | not run | not run | every service keeps text within contrast/overflow over its per-town background | n/a | music grows more sinister town to town | — | not run |
| 40 | Town score bonus (M32) | clear a run in town ≥ 2; check the result "Town bonus (+N%)" row and the scoreboard "T#" tag | not run | not run | result panel fits the extra row; T# fits the Theme column | n/a | n/a | 19_result.png, 12_scoreboard.png | not run |
| 41 | Unlock + old-save compat (M32) | complete a dungeon (unlocks next town); load a pre-M32 save (starts in town 1) | not run | not run | n/a | n/a | n/a | — | not run |
| 42 | Guild stakes forewarning (M33) | after a completed run, re-open the Guild; change Depth left/right and watch the penalty line update; raise depth above last run to clear it | not run | not run | "-N% (raise town or depth to clear)" fits + updates live | n/a | n/a | 26_guild_penalty.png | not run |
| 43 | Stakes penalty on result (M33) | clear a run that does not raise the stakes | not run | not run | result panel fits the "Stakes penalty (-N%)" row (also with town bonus) | n/a | n/a | 19_result.png | not run |
| 44 | Stakes save-scum resistance (M33) | with a penalty pending, enter a dungeon (autosaves), reload that autosave, re-enter same config | not run | not run | penalty is the same after reload (not shed) | n/a | n/a | — | not run |
| 45 | Stakes reset + New Game (M33) | raise town/depth to clear the penalty; start a New Game and confirm it begins with no penalty | not run | not run | n/a | n/a | n/a | — | not run |
| 46 | Legendary tokens (M34) | win an optional elite challenge in a dungeon; confirm "+1 legendary token" and the count on the market screen | not run | not run | token message fits | n/a | victory sfx | — | not run |
| 47 | Black-market spawn (M34) | in town 2+, do stakes-raising runs until the dealer appears (~20%); confirm the NPC + "Black Market" label + prompt | not run | not run | NPC label + prompt legible | prompt on the NPC tile | confirm sfx | 25_town_ladder.png | not run |
| 48 | Black-market purchase (M34) | open the market; buy with gold, and (another spawn) with 3 tokens; confirm insufficient rows disable; declining leaves the offer | not run | not run | item stats/desc fit; gold/tokens clear of the title | rows disable when unaffordable | confirm/error sfx | 27_black_market.png | not run |
| 49 | Market save-scum + persistence (M34) | after a spawn, reload the entry autosave and replay the same run — same outcome; the offer persists until bought | not run | not run | n/a | n/a | n/a | — | not run |
| 50 | Legendary balance (M34) | equip legendaries; confirm they are a clear upgrade yet a low-level party still cannot walk over town 7 | not run | not run | n/a | n/a | n/a | — | not run |
| 51 | Blind status (M35) | blind an enemy (Rogue Smoke Screen L7 / Ranger Flash Arrow L11); watch its physical attacks | not run | not run | "Miss!" floater + BLD label read clearly; ~3 of 4 attacks miss | n/a | n/a | 17_battle_five_enemies.png | not run |
| 52 | Silence status (M35) | silence an enemy caster (Mage Silence L7); also get a party member silenced | not run | not run | silenced MP skills greyed with "[Silenced]"; SIL label shown; foe stops casting | n/a | n/a | 23_battle_targeting.png | not run |
| 53 | Confusion status (M35) | confuse an enemy (Mage Bewilder L12); also a confused party member; then hit a confused unit | not run | not run | "is confused and attacks" its own side; CNF label; CNF **clears when the unit takes damage** | n/a | n/a | 17_battle_five_enemies.png | not run |
| 56 | Status potency (M35) | poison a foe and watch a tick; note how long any status lingers | not run | not run | poison tick deals ~2x the old amount; statuses last ~2x as long | n/a | n/a | — | not run |
| 54 | Cure / Purify (M35) | afflict the party, then use a Remedy item and the Cleric's Purify (L10) | not run | not run | all afflictions (poison/blind/silence/confusion/debuffs) lifted; buffs kept | n/a | heal sfx | — | not run |
| 55 | Stakes re-tune (M35) | do a non-raising completed run, then repeat it several times | not run | not run | Guild + result show -30% then -60/-90/-99% (was -15/-90) | n/a | n/a | 26_guild_penalty.png, 19_result.png | not run |

| 57 | Passive buy/equip (M36) | Training Hall -> a character -> Manage Passives; buy one, equip/unequip, swap between two owned | not run | not run | rows show Equipped/Owned/price; gold deducts once; swap is free | equip flow legible on both | confirm/error sfx | 28_training_passives.png | not run |
| 58 | Passive effects in battle (M36) | equip and try each: Counter, Evasion, Spell Ward, Thorns, Lifedrink, Clarity, Iron Will, First Strike, Bodyguard, Keen Senses | not run | not run | each reads clearly (Miss!, counter/thorns/drain/shield lines, MP tick, survive-at-1, acts first) | n/a | n/a | - | not run |
| 59 | Foe passive reveal (M36) | target an elite/boss that carries a passive; open Details | not run | not run | target panel + Details show "Passive: name" and effect | reveal on the targeted foe | n/a | 29_battle_passive_reveal.png | not run |
| 60 | Passive save round-trip (M36) | equip passives, save, Continue; also load a pre-M36 save | not run | not run | owned + equipped persist; old save loads with none | n/a | n/a | - | not run |
| 61 | Per-town gear unlock (M37) | Equip Shop at town 2 vs town 5 vs town 7; note the growing weapon/armor/accessory lists | not run | not run | higher towns stock more gear; buy list scrolls cleanly at max stock; long names fit | n/a | n/a | 30_equip_shop_max.png | not run |
| 62 | Gear power curve (M37) | buy and equip per-town gear as you climb; compare to legendaries | not run | not run | each town's gear is an upgrade; prices gate without grinding; legendaries still the peak | n/a | n/a | - | not run |
| 63 | Merchant bargain (M37) | find a dungeon Merchant event; check the price vs the item's shop value | not run | not run | merchant sells at ~75% of value (a bargain), shown before buying | n/a | n/a | - | not run |
| 64 | Generation comparability (M37) | run a dungeon; check the scoreboard shows the run under gen v7 | not run | not run | scoreboard tags the generation version; old entries read their own version | n/a | n/a | 12_scoreboard.png | not run |
| 65 | Per-town foes appear (M38) | run dungeons at town 1 vs town 5 vs town 7; watch the enemy/boss variety | not run | not run | new foes (Dune Reaver ... Dread Sovereign) show only at their town+; each has its own sprite | distinct silhouettes readable | n/a | 31_battle_high_town.png | not run |
| 66 | New boss kits (M38) | fight the per-town bosses (towns 2-7); read their telegraphs/passives | not run | not run | each boss's status/passive kit reads in play (silence/blind/confuse, thorns/ward/etc.) | n/a | boss music | 18_battle_boss.png | not run |
| 67 | Per-town difficulty (M38) | clear towns 2->7 with town-appropriate level/gear | not run | not run | escalation feels fair; town 7 is beatable by a maxed, well-geared party | n/a | n/a | - | not run |
| 68 | Generation comparability (M38) | run a dungeon; check the scoreboard shows gen v8 | not run | not run | scoreboard tags v8; old entries read their own version | n/a | n/a | 12_scoreboard.png | not run |
| 69 | Boss drops appear (M39) | clear bosses in town 3+ at depth 4+ (and deeper/higher); watch the result screen | not run | not run | "Boss drops" block shows tokens (+2 in town 7) and/or a legendary; block + footer stay on-screen even on the fullest panel | n/a | victory sfx | 32_result_drops.png | not run |
| 70 | Boss drops reload-proof (M39) | reload the entry autosave and replay the same run — same drops; a shallow/low-town boss never drops | not run | not run | n/a | n/a | n/a | - | not run |
| 71 | Boss drops feel/balance (M39) | climb towns 3->7 at depth; confirm drops reward deep clears without replacing the black market | not run | not run | rates feel right; tokens/legendaries accrue over clears, market still worthwhile | n/a | n/a | - | not run |
| 72 | Castle unlock + travel (M40) | clear a town-7 dungeon; in town 7 take the northern road up to the castle; leave back to town | not run | not run | castle road locked until the town-7 clear, then opens; castle reads as a distinct place; leaving returns to town 7 | road tile at top of town 7 | castle music | 33_castle_hub.png | not run |
| 73 | The Hollow King (M40) | from the castle choose "Challenge the King"; fight him with a maxed party | not run | not run | the King is the hardest fight, beatable but a real struggle; immune to blind/silence/confusion; telegraph + King theme read | n/a | King battle music | 34_king_battle.png | not run |
| 74 | Boss Rush / Endless (M40) | run the Boss Rush (12 bosses, no free heal) and the Endless Rush (escalating waves) | not run | not run | no healing carries between fights; Boss Rush is clearable, Endless overwhelms after a sane number of waves | n/a | battle/boss music | - | not run |
| 75 | Castle records + rewards (M40) | clear each challenge once; check the records panel and the first-clear rewards (King -> regalia + title) | not run | not run | records show on the hub and the load screen title; first clear pays gold/tokens; King grants the unique regalia + title; repeats set records only | n/a | n/a | 33_castle_hub.png, 35_castle_result.png | not run |
| 76 | Castle save/scoreboard separation (M40) | save in the castle, reload; confirm castle records persist and never appear on the dungeon scoreboard | not run | not run | records round-trip; the dungeon scoreboard is untouched by castle challenges | n/a | n/a | - | not run |
| 77 | Storyteller in each town (M41) | find the wandering storyteller in each town; hear the verse; read one in town order as you climb | not run | not run | storyteller easy to find at a fixed plaza spot; each verse fits the dialog panel; the serial reads in town order | storyteller sprite + "Hear the storyteller" prompt | confirm sfx | 36_story_dialog.png | not run |
| 78 | Jester punchline gate (M41) | at the castle choose "Speak with the Jester" before hearing all 7 (teaser) and after (punchline) | not run | not run | teaser until all 7 heard, then the punchline; the joke lands and pays off the climb | n/a | confirm sfx | 37_jester.png | not run |
| 79 | Story read-progress persists (M41) | hear some verses, save, reload; confirm progress kept and the Jester still gated/unlocked correctly | not run | not run | storyMet round-trips; old saves load with nothing heard | n/a | n/a | - | not run |
| 80 | Story tone (M41) | read the whole serial start to finish | not run | not run | light-hearted and funny; original; the King wink does not undercut the boss fight | n/a | n/a | - | not run |
| 81 | Bestiary (M42) | fight varied foes, then open Pause -> Bestiary; scroll the whole list, inspect a fought foe, an unknown, and a boss with several passives | not run | not run | the full roster is listed with a known/total counter; fought foes show sprite/stats/profile/passives (one per line)/boss flavor in full; unmet foes read `? ? ?` with masked stats; list scrolls; detail panel fits | n/a | n/a | 38_bestiary.png, 41_bestiary_king.png | not run |
| 82 | Victory stats (M42) | clear a dungeon; on the result screen press the Details key | not run | not run | this-run stats (damage, biggest hit, statuses, MVP) + personal records read clearly and update run to run | n/a | n/a | 40_run_stats.png | not run |
| 83 | Achievements + toasts (M42) | earn a few achievements (clear a dungeon, reach a town, beat the King); watch for the unlock toast; open Pause -> Achievements | not run | not run | one toast per unlock (no nagging); the screen shows locked/unlocked + count; locked ones stay hidden as goals | n/a | confirm sfx | 39_achievements.png | not run |
| 84 | Enrichment persistence (M42) | unlock achievements + record foes, then restart the game and reload | not run | not run | bestiary + records round-trip per save; achievements persist globally (achievements.json); old saves load empty | n/a | n/a | - | not run |
| 85 | Battle skill/item lists (M42 fix) | in battle open Skill with a character that has long skill names, then Item; scroll both; try it again while silenced | not run | not run | every name and its right-aligned MP cost / item count are fully readable; a silenced skill shows `SIL` with the reason spelled out beside the list; the description column still reads in two lines | n/a | n/a | 42_battle_skills.png | not run |
| 86 | Battle status tags (M42 fix) | take a battle where party members and enemies carry several statuses at once | not run | not run | party tags sit right of the sprite under the HP/MP numerals and never paint over another row; enemy tags sit right of their sprite; a unit immune to a status never shows it | n/a | n/a | 17_battle_five_enemies.png, 31_battle_high_town.png | not run |
| 87 | Achievements layout (M42 fix) | open Pause -> Achievements; walk the cursor through every row with up/down and left/right | not run | not run | two columns; up/down walk the whole roster, left/right jump a column; the description reads centered below the list and never touches the Back hint | n/a | n/a | 39_achievements.png | not run |
| 88 | Save/load slot rows (M42 fix) | beat the King so the party carries a title, save to every slot, then open both Save and Load | not run | not run | slot/level/party/gold on one line, the King title on its own line under it; nothing runs off screen in either mode; the status message stays clear of the rows and the footer | n/a | n/a | 11_slot_menu_save.png, 43_slot_menu_load.png | not run |
| 89 | Repriced consumables (M43) | visit the item shop in towns 1 and 3; price a Remedy, Ether, Hi-Ether, Phoenix Tear and Royal Snacks against your gold after a clear | not run | not run | prices read 100 / 150 / 500 / 300 / 250 g; a clear's gold buys a handful, so stocking up is a real decision rather than a formality; nothing overflows the shop rows | n/a | n/a | 08_item_shop.png | not run |
| 90 | Royal Snacks are town-1 only (M43) | look for Royal Snacks in the town-1 item shop, then in towns 2-7 | not run | not run | listed (250 g, "Bring Snacks!") in town 1 and absent everywhere else; other consumables are unaffected | n/a | n/a | 08_item_shop.png | not run |
| 91 | Royal Snacks in and out of the King fight (M43) | use snacks in an ordinary battle, then in the King fight (buy several first) | not run | not run | ordinary: 10 HP and ATK-/DEF- lifted; against the King: 100 HP + 10 MP and the same cure; the log states what happened both times | n/a | heal sfx | - | not run |
| 92 | Battle item targeting (M43) | with everyone alive open Item and look at a Phoenix Tear; KO an ally and look at a potion, an ether, a Remedy and the Tear | not run | not run | a Tear is greyed with "No fallen ally to revive." while all stand; with an ally down the Tear targets only the fallen and heal/MP/cure items skip them; no item is ever spent for "No effect" | n/a | error sfx on a blocked pick | - | not run |
| 93 | Renew as the emergency button (M43) | with a Cleric at level 4+, cast Renew on a wounded ally, then on a KO'd one | not run | not run | the target list includes the fallen; a living ally gets a small heal, a fallen one stands at 20% HP; the message says which happened; judge whether 12 power / 9 MP / 20% is the right emergency button | n/a | heal sfx | - | not run |
| 94 | Purify and Clarity (M43) | cast Purify on a status-loaded party; run a battle with Clarity equipped | not run | not run | Purify lifts everything and heals nothing (the description says so); Clarity restores 2 MP a round; judge whether the nerfs sting appropriately | n/a | n/a | - | not run |
| 95 | Confused units on both sides (M43) | confuse an enemy caster with Bewilder and watch its turns; then let a foe confuse one of your casters | not run | not run | a confused enemy only ever swings at its own side - never heals, buffs or curses; your confused member does the same and takes no input; damage snaps either out of it | n/a | hit sfx | - | not run |
| 96 | Losing a castle challenge (M43, restated M47) | lose the Boss Rush partway, then lose to the King; check the party screen afterwards | not run | not run | the defeat message claims no gold loss; the result panel counts the real roster ("X of 12 bosses") and says you are carried to the gates barely breathing; **survivors stand at 1 HP, the fallen are still fallen, MP is unchanged, gold is unchanged** | n/a | defeat jingle | 35_castle_result.png | not run |
| 97 | Finding a reliquary (M44) | run town 5-7 dungeons at depth 5+ until a reliquary room appears (gold casket, `*` marker); read the footer, then open it | not run | not run | the prop reads as a treasure worth crossing the room for; the footer states the reward before you confirm; the first one shows the Royal Relic tutorial beat; the message names the relic you got | reliquary prop | interact sfx | 45_relic_event.png | not run |
| 98 | Relic grant is reload-proof (M44) | before opening a reliquary, note the seed; open it, then reload the entry autosave, walk back and open it again | not run | not run | the same relic both times; a reload cannot fish for the Deadly Spoon | n/a | n/a | - | not run |
| 99 | Relics in the battle item list (M44) | carry all four relics into a fight and open Item | not run | not run | each relic name and its count read fully; the description explains the effect in two lines; choosing one aims at the ENEMY side | n/a | n/a | 46_battle_relics.png | not run |
| 100 | Evil Goose and Tax Sheets (M44) | use each on a boss and watch the following turn | not run | not run | the goose forces exactly one guarded turn ("terrified"); the sheets cost exactly one turn ("buried in paperwork"); both read clearly in the log and as the TRF / STN tags | n/a | status sfx | - | not run |
| 101 | Dragon Crown on the wrong target (M44) | use the crown on an ordinary enemy, then on the Hollow King | not run | not run | on anyone but the King: "Nothing happens. (kept)" and the item is still in your bag afterwards; on the King: ATK- and DEF- both land and it is consumed | n/a | n/a | - | not run |
| 102 | Deadly Spoon (M44) | use the spoon on a boss and compare its damage before and after | not run | not run | its ATK/MAG/DEF/SPD are visibly halved for the rest of the fight (Details panel), its HP untouched, and the effect never wears off | n/a | n/a | - | not run |
| 103 | The doubled King (M44) | fight the Hollow King with a maxed party carrying no relics, then again with one Tax Sheets + one Evil Goose + a bag of Royal Snacks | not run | not run | unaided he wins; with the two relics and snacks the fight is winnable but tense (the sim wins in 18 rounds with 2 of 4 alive at the chosen 340% scale). **Judge the feel** - too brutal, too easy, or right | n/a | King theme | 34_king_battle.png | not run |
| 104 | Locked classes (M45) | on a machine that has never beaten the King, start New Game and cycle a slot through every class | not run | not run | Dragon, Jester and Goose are listed and marked "(Locked)"; the hint under the roster says how to unlock them; Begin refuses (error beep) while one is selected | class art | error sfx | 47_class_select_locked.png | not run |
| 105 | Unlocking (M45) | beat the Hollow King, then start a New Game | not run | not run | the King result says the three classes are now available; the class list no longer marks them locked and Begin works | n/a | n/a | 48_class_select_unlocked.png | not run |
| 106 | Retroactive unlock (M45) | with profile.json deleted, load an old save whose party already beat the King, then open New Game | not run | not run | the classes are unlocked without beating him again | n/a | n/a | - | not run |
| 107 | Dragon in battle (M45) | take a Dragon into a fight with 3+ enemies | not run | not run | one attack strikes every living foe; each connecting hit leaves poison + blind (PSN/BLD tags); misses leave nothing; the Skill command is empty; armor cannot be equipped (the shop says why) | dragon sprite | hit sfx | - | not run |
| 108 | Jester in battle (M45) | take a Jester into several fights and watch its turns | not run | not run | it never asks for input; it uses skills it can afford and swings otherwise; quips appear now and then (~15%) and read as dry, not annoying; no weapon can be equipped. **Tone check: do the twelve lines land?** | jester sprite | n/a | 49_battle_jest.png | not run |
| 109 | Goose in battle (M45) | take a Goose into a fight and use its heals; reach level 30 for the ultimate | not run | not run | the heal works AND buffs every enemy (visible tags on the foes); nothing can be equipped; the level-30 ultimate lays every debuff on every foe for 30 MP | goose sprite | heal sfx | - | not run |
| 110 | Class score modifier (M45) | clear a dungeon with a mixed party (e.g. Dragon + Jester + Goose + Knight), then with four Dragons | not run | not run | the result screen shows a "Class modifier" line with the signed percentage; the scoreboard tags the run; the total moves accordingly and never goes below 0. **Judge whether the modifiers feel fair** | n/a | n/a | 32_result_drops.png | not run |
| 111 | Purify vs the debuffs (M47) | in battle, get a member poisoned/blinded/silenced/confused AND hit with ATK-/DEF-; cast Purify, then use a Remedy on someone in the same state | not run | not run | Purify lifts the four afflictions and **leaves ATK-/DEF- standing** (tags remain); the description says so; a Remedy still lifts everything. **Judge whether the narrowed Purify makes the debuff kits matter without being annoying** | n/a | heal sfx | - | not run |
| 112 | Generous Mending's cleanse (M47) | with a Goose in the party, cast Generous Mending on a party carrying afflictions and ATK-/DEF- | not run | not run | it mends, cleanses the four afflictions, leaves ATK-/DEF-, and still cheers up the enemies. **Owner decision point: the narrowing applies to this skill too** | n/a | heal sfx | - | not run |
| 113 | Castle defeat stakes (M47) | lose a castle challenge with a mixed party (some alive, some KO'd), then lose one with a full wipe, then **flee** a challenge | not run | not run | mixed: survivors at 1 HP, the KO'd still at 0, MP untouched. Wipe: exactly the FIRST member at 1 HP, the rest KO'd. Fled: the HP/MP the fight ended with, survivors clamped to 1 HP, nothing healed. Gold never moves; the inn is reachable in every case. **Judge whether the new stakes feel right or too harsh** | n/a | defeat jingle | 35_castle_result.png | not run |
| 114 | Menu closes the pause menu (M47) | in town and in a dungeon, press Tab (then Start on a pad) to open the pause menu, and press it again | not run | not run | the same key closes it; Cancel still closes it; no double-toggle from one press | cursor returns where it was | cancel sfx | 50_dungeon_pause.png | not run |
| 115 | Quit from town (M47) | town pause → Quit; try Cancel, Keep Playing, Quit to Title, and finally Quit Game | not run | not run | three answers with the cursor on Keep Playing; Cancel and Keep Playing return to the pause menu with the run intact; Quit to Title returns to the main menu; **Quit Game closes the game cleanly** (no crash, no hung window, settings kept) | cursor never starts on a destructive row | confirm/cancel sfx | 44_quit_confirm.png | not run |
| 116 | Quit from a dungeon (M47) | mid-run, dungeon pause → Quit; read the body, then try each answer | not run | not run | the body says the run is lost but the entry autosave is kept — verify that by quitting and using Continue; the panel fits all three answers over the taller pause box | n/a | confirm/cancel sfx | 52_dungeon_quit.png | not run |
| 117 | Battle-rules tag (M47/M48) | clear a dungeon, then open the Scoreboard | not run | not run | the run is tagged with the current battle rules (v8 since M48); older entries keep their own version and are flagged as played under different rules (never renormalized) | n/a | n/a | 12_scoreboard.png | not run |
| 118 | Weakness in play (M48) | with a Mage, cast Fireball at a Frost Imp (town 1) and at the Frost Monarch (town 3 boss); compare with Arcane Burst | not run | not run | the fire hit is visibly larger, floats a gold **Weak!** above the foe, and the log says "It is devastating!"; the untagged spell is unchanged. **Judge whether x150% feels worth aiming for** | n/a | hit sfx | 53_battle_weak_hit.png | not run |
| 119 | Immunity in play (M48) | cast Blizzard at the Frost Monarch; then let a Dragon (fire weapon) swing at it | not run | not run | damage is **0**, a coral **Immune** floats, the log says it is immune — and it never reads as a miss; no status rider lands either. **Judge whether a wasted turn is instructive or just annoying** | n/a | hit sfx | 54_battle_immune_hit.png | not run |
| 120 | Affinity reveal (M48) | target a tagged foe in battle; then open Pause → Bestiary on a foe you have fought and one you have not | not run | not run | the target panel shows **Weak Fire** / **Immune Ice** chips clear of the other rows; the bestiary shows the same lines for a known foe and **nothing** for an unmet one | chips legible at the panel's edge | n/a | 56_battle_target_affinity.png, 55_bestiary_affinity.png | not run |
| 121 | Elemental weapons (M48) | equip the Holy Mace (town 1) and swing at a Zombie; later equip Dragonfang/Sunforged and fight the tagged foes | not run | not run | basic attacks carry the weapon's element (gold Weak! on the Zombie); an unarmed or untagged weapon behaves exactly as before; **no weapon is ever nullified** by an immunity | n/a | hit sfx | - | not run |
| 123 | The King's Court (M49) | challenge the King with a maxed party; watch the opening | not run | not run | he enters flanked by **two Royal Guards**; three enemy sprites + their HP bars fit without overlap; the telegraph names the court; the guards' buffs land on the King (ATK+/DEF+ tags on HIM, not just on themselves) | target cursor reaches all three | King battle music | 57_kings_court.png | not run |
| 124 | The revive clock (M49) | kill both guards, then count the King's turns | not run | not run | on the **5th** of his turns with both down they return at **full HP with no statuses**, announced in one line ahead of his action; kill them again and it happens **again**; leave one alive and it never fires. **Judge whether 5 turns is the right window** | n/a | n/a | 58_court_revived.png | not run |
| 125 | **King reward gate (M49 + castle raise + L99 cap)** | at level 99, fight the King at ×5.00; try nothing, then the M44 "plan" answer (1 Sheets + 1 Goose + 20 Snacks), then a full relic haul (3 Sheets + 3 Geese + 30 Snacks), then + Crown + Spoon | not run | not run | **BLOCKING: does the reward gate feel right?** Sim: nothing loses, the plan answer loses, a full relic haul WINS (3 survivors @33 % HP), Crown+Spoon makes it comfortable (@75 %). Beating him unlocks the Regalia, the title AND the three M45 classes. Judge whether "farm the full relic set" is the right bar — brutal but reachable | n/a | King theme | 34_king_battle.png | not run |
| 125b | **Level cap 99 (progression)** | level a hero past 50 at the Training Hall / via battle XP; check stats, the "Peerless" achievement, save round-trip | not run | not run | levels advance to 99; stats keep growing; "Peerless" now reads "level 99" and fires at 99; a level > 50 character saves and re-loads intact; old (≤ 50) saves still load. **Judge whether 99 feels overpowered in the intended way, and whether the XP/gold grind to reach it is acceptable** | n/a | level-up sfx | 11_slot_menu_save.png | not run |
| 126 | Boss Rush above the ladder (M49 + castle raise) | run the Boss Rush at ×5.80 | not run | not run | every boss brings its dungeon minions and each is scaled past the deepest dungeon boss. **The sim survives only 1 of 12 — judge how far a real player gets and whether the first-clear reward is reachable** | n/a | boss music | - | not run |
| 126b | Endless above the ladder (M49 + castle raise) | run the Endless Rush at ×5.00 + 0.10/wave | not run | not run | the sim reaches wave 6. Confirm the challenge still reads as "push as far as you can" rather than "die immediately", and that the opening waves already feel endgame | n/a | battle music | - | not run |
| 127 | The guards stay in the throne room (M49) | run dungeons across themes/towns and push the Endless Rush deep | not run | not run | neither Royal Guard **ever** appears outside the King's fight; they do appear in the Bestiary as unmet until you meet them | n/a | n/a | 38_bestiary.png | not run |
| 122 | Elements do not break old fights (M48) | fight several untagged teams across themes | not run | not run | nothing about untagged encounters looks or feels different; no stray Weak!/Immune floats; damage numbers match expectations | n/a | n/a | 17_battle_five_enemies.png | not run |
| 128 | Title phrase (M51) | open the title screen several times (New Game → back to title) | not run | not run | a different dry-comedy line appears (incl. "Geese and Dragons; Spoons and Snacks!"); **none describes the genre**; it pulses gently without moving layout; readable on any frame. **Tone check on the twelve lines** | n/a | title music | 01_title.png | not run |
| 129 | Settings submenus (M51) | open Settings; enter each of Audio / Display / Gameplay / Controls; Cancel out of a submenu, then Cancel from the top | not run | not run | the top level lists the four categories + Reset + Back; each submenu holds its own rows; Cancel steps back one level, then (at top) saves and closes | cursor lands sensibly per level | move/confirm/cancel sfx | 03_settings.png, 60_settings_display.png | not run |
| 130 | CRT effect (M51) | Display → CRT Effect → On; look at gameplay; toggle Off | not run | not run | subtle scanlines + faint mask appear, **no curvature** (pixels stay crisp and aligned); Off restores the plain image; no crash on any machine. **Judge whether it's tasteful or distracting** — default is Off | n/a | n/a | — (window-only; not in the capture set) | not run |
| 131 | Background audio / focus (M51) | with Background Audio **Off** (default), click away from the window; then set it **On** and click away again | not run | not run | Off: music/ambience fall silent when the window loses focus and resume on return; On: audio keeps playing unfocused. **This is a deliberate behaviour change from always-on** | n/a | audio mutes/resumes | — | not run |
| 132 | AoE screen tint (M51) | cast a mass spell (Inferno/Blizzard/Radiance), the Goose's ultimate, and a Dragon's sweep; then set Battle Flash to Reduced, then Off | not run | not run | a faint full-screen tint pulses once on impact — coral for damage, green for a party heal/buff, violet for a mass debuff; **Reduced halves it, Off removes it**; never a strobe, never over the message panel | n/a | n/a | 61_aoe_tint.png | not run |

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
