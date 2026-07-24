# Crystal Dungeons

A 16-bit-inspired, turn-based **JRPG roguelite** about clearing seeded dungeons
**efficiently**. Take a party of four from a town hub into procedurally generated
dungeons full of *visible* enemy teams and guarded chests, beat the boss, and
score on how few battle turns you spent — then upgrade and dive again, forever.

Original work — not a clone of any existing game; no copyrighted names, art,
music, or text. Built in **C++20** with **raylib**.

> **Status: feature-complete, polished playable build** (milestones M1–M52
> delivered and owner-approved). In the box: a seven-town difficulty ladder
> plus a castle endgame far above it (Boss Rush with escorts / Endless Rush /
> the Hollow King flanked by his reviving Royal Guards), seeded walkable
> dungeons with room events including the rare Royal Relics, deterministic
> turn-based combat (statuses, passives, forced-action turn-control, sparse
> elemental weaknesses/immunities, an enmity/threat model with control
> skills), a stakes-escalation score rule with an honest tagged scoreboard, a
> black market and legendary tokens/drops, per-town enemies/bosses/equipment
> with original generated art and music, a light-hearted story serial, three
> unlockable reward classes (Dragon / Jester / Goose), learnsets, shops, a
> paid inn, compact walk-through towns, onboarding, accessibility options,
> categorized settings (a 0–10 CRT Strength filter, background audio, an independent
> ambience slider), an in-battle action log, a bestiary (with each foe's
> strongest-context stats), victory records, achievements, and a fully
> procedural "8-bit-plus" UI.
> The **M53–M56 adjustment program** (a debug toolbelt + five save slots + a
> tougher Champion; an equipment rebalance; per-theme dungeon rites; and boss
> battle backdrops + a Crystal Shatter boss intro) is **implemented and awaiting
> the owner's manual approval**. After it, only the deferred **validation
> playtesting (M23)** and **release sign-off (M24)** remain. Current status always
> lives in `docs/milestones.md`.

## Requirements

- **Visual Studio 2022 or newer** with the **"Desktop development with C++"**
  workload (provides the MSVC compiler plus a bundled CMake and Ninja). This
  project is built with **MSVC / C++20**; MinGW and other GCC toolchains are not
  supported.
- **CMake ≥ 3.20** (the Visual Studio–bundled CMake is fine).
- **Internet access on the first configure only** — raylib `6.0`, nlohmann/json
  `v3.12.0`, and Catch2 `v3.15.1` are fetched and pinned by CMake (nothing to
  install manually), then cached under `build-msvc/_deps`.

## Build & run

Every command below must run from a **Visual Studio developer environment**, so
that `cl` and the bundled Ninja are on `PATH`.

**Recommended:** open **Developer PowerShell for VS 2022** from the Start menu
and run the build commands there.

Verify the environment before configuring — all three must resolve:

```powershell
where.exe cl
where.exe ninja
cmake --version          # 3.20 or newer
```

<details>
<summary>Alternatives if you are not using the Developer PowerShell shortcut</summary>

**Bootstrap an existing PowerShell session.** `vcvars64.bat` cannot configure a
PowerShell session — it sets variables in a child `cmd` process that are
discarded when it exits, so `& "...\vcvars64.bat"` leaves your shell unchanged.
Use Visual Studio's PowerShell entry point instead, which sets the variables in
the *current* session (substitute your edition — `Community`, `Professional`, or
`Enterprise`):

```powershell
$vsRoot = "C:\Program Files\Microsoft Visual Studio\2022\<edition>"
& "$vsRoot\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64
```

**Wrap a single command in `cmd`.** This configures only the wrapped command,
not your PowerShell session, so each build command needs its own wrapper:

```powershell
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\2022\<edition>\VC\Auxiliary\Build\vcvars64.bat"" >nul && cmake --preset msvc-debug"
```

</details>

### Presets (recommended, M24)

```powershell
cmake --preset msvc-debug      # development: debug overlay + capture CLI
cmake --build --preset debug
.\build-msvc\CrystalDungeons.exe

cmake --preset msvc-release    # shipping: static CRT, no capture CLI
cmake --build --preset release
```

The **development** build also carries a **debug menu** (M53), opened from the
**Debug** row on either pause menu (town or dungeon): set levels/gold/tokens/town,
grant items, toggle a party **god mode**, instantly clear a dungeon, unlock the
reward classes, and fill the bestiary. It is gated on the debug overlay and is
structurally absent from the Release preset — no shipping build can reach it.

The release preset links the **static MSVC runtime**, so the packaged exe
runs on a Windows machine without Visual Studio or the VC++ redistributable.
To build the full distribution zip (stage + validate + archive):

```powershell
powershell -ExecutionPolicy Bypass -File tools\package.ps1
# -> dist\CrystalDungeons-<version>-win64.zip
```

The version is set once in `CMakeLists.txt` `project(VERSION ...)` and flows
into the exe metadata, the title screen, and the package name.

### Ninja without presets

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build-msvc
.\build-msvc\CrystalDungeons.exe
```

`-DCMAKE_*_COMPILER=cl` forces MSVC so no other compiler on `PATH` is picked
by mistake. CMake copies `data/` and `assets/` next to the executable; the
**deliverable is the staged folder produced by `tools\package.ps1`**.

### Visual Studio generator (alternative)

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
.\build-msvc\Release\CrystalDungeons.exe
```

## Controls

Default bindings — everything except text-delete and the debug toggle is
**remappable in-game** under **Main Menu → Settings** (also reachable from the
pause menus). Settings are organized into **Audio / Display / Gameplay /
Controls** submenus (M51): master/music/SFX volumes, a separate **Ambience
Volume** slider (M52, default 5) and a **Background Audio** toggle; window
mode, a **CRT Strength** slider (M57, 0–10, 0 by default), battle flash/shake,
and high-contrast; battle/message speed and tutorial prompts; and per-device
remapping. All of it persists in `settings.json` in the user data folder;
one-time tutorial-prompt progress persists in `tutorial.json` beside it. By
default the game **mutes when its window loses focus** (turn on Background Audio
to keep it playing).

| Action                | Keyboard               | Gamepad            |
|-----------------------|------------------------|--------------------|
| Move / Navigate       | Arrows or WASD         | D-Pad / Left Stick |
| Confirm               | Enter or Space         | A                  |
| Cancel / Back         | Esc or Backspace       | B                  |
| Menu / Pause          | Tab                    | Start              |
| Details / Info        | C                      | Y                  |
| Adjust (Guild, Settings) | Left / Right        | D-Pad L/R / Stick  |
| Delete (name entry)   | Backspace              | X                  |
| Toggle debug overlay  | F1                     | —                  |

The in-game list (**Main Menu → Controls**) always shows your *current*
bindings. The window is resizable; the 426×240 image always scales to fit with
letterbox/pillarbox bars.

## How to play

1. **New Game** → pick 4 classes (Knight, Ranger, Mage, Cleric, Rogue, Guardian)
   and name them. You start with a little gold.
2. In the **town**, walk to buildings: **Inn** (rest to full HP/MP for gold, or
   free with a rest token), **Item Shop** (buy consumables), **Equip Shop**
   (buy by category + equip gear — each town unlocks stronger gear as you climb),
   **Training Hall** (level up, and buy passive skills — own many, equip one),
   **Scoreboard**, **Save Point** (5 slots), and the **Guild**. **Walk out the
   west/east roads** to move between the **seven towns** (no button — just walk
   into the road); each later town raises enemy stats (up to +200 %) and score
   bonus (up to +100 %); clearing a dungeon in a town unlocks the road onward.
3. At the **Guild**, pick a theme + depth and enter a seeded dungeon. Entering
   autosaves.
4. Walk the dungeon: enemy teams show a **danger tier**; fight them to clear
   **gates** (≥3 before the boss) and chest guards. Win battles to earn **XP and
   gold**; open chests for loot.
5. Beat the **boss** to clear the dungeon and post a **score** (driven mainly by
   *fewest battle turns*, plus danger defeated, treasure, a no-death bonus, and
   the town's score bonus). Runs that fail to raise your **stakes** — (town,
   depth) vs your last completed run — lose 30 % per repeat (to a −99 % cap);
   the Guild shows the exact penalty before you enter. Beating a boss in **town 3+
   at depth 4+** can also **drop legendary tokens and/or a legendary piece** —
   chances rise with town and depth (up to 75 %/30 % at town 7 depth 20, with
   double tokens in town 7), seeded so a reload can't reroll them; drops show on
   the result screen and never change the score.
6. **Retreat** any time (you keep XP/gold but score 0). **Defeat** returns you to
   town with half your gold. Upgrade, then dive deeper — runs scale with depth,
   town, and seed, endlessly. A stakes-raising clear in town 2+ can (20 %,
   seeded) spawn a **black market** selling one legendary piece for gold or
   **legendary tokens** won in optional elite challenges — and any boss kill at
   **town 7, depth 20+** rolls a second, independent **34 %** chance of the
   dealer, regardless of score or stakes (M52). During any battle, **Menu/Pause**
   opens a scrollable **battle log** of the last actions.
7. Clear any **town-7 dungeon** to open the northern road to the **castle** — a
   place above the ladder with the **King's three challenges**: the **Boss Rush**
   (all 12 bosses back-to-back **with their minions**, no free healing), the
   **Endless Rush** (escalating waves, survive as long as you can), and **the
   Hollow King** himself (the hardest fight — immune to blind/silence/confusion,
   flanked by **two Royal Guards he calls back from the dead every five turns**;
   beat him for a unique legendary and a title). The castle keeps its **own records**, separate from your dungeon scores.
   Failing (or fleeing) a challenge costs **no gold** — but nobody is healed:
   survivors are carried to the gates at **1 HP**, the fallen stay fallen, and a
   full wipe leaves exactly one member standing so an inn is always reachable.

## Project layout

```
src/
  core/      Application loop, AppContext, config, FadeController (transitions)
  render/    VirtualScreen (426x240 scaling), Viewport, raylib RAII wrappers
  audio/     AudioManager (manifest-driven music/ambience/SFX, synth fallback)
  input/     action mapping (keyboard + gamepad)
  resource/  cached textures/fonts with graceful fallback
  platform/  user-data paths, path sanitizing
  content/   JSON content model: defs, enums, loaders, validators
  game/      Character, Party, Inventory, stat derivation, XP/leveling
  save/      versioned JSON saves (slots + autosave)
  score/     scoring + persistent scoreboard
  danger/    stat-derived danger tiers
  town/      tilemap, movement, town layout
  dungeon/   seeded generator, model, RNG
  battle/    deterministic turn-based combat + headless simulator
  ui/        Menu, TextInput (pure) + UiDraw helpers
  states/    game states (menu, town, dungeon, battle, shops, ...)
data/        JSON content (classes, enemies, items, skills, bosses, themes)
tests/       Catch2 unit/integration tests (headless)
docs/        design + technical + milestone docs
```

## Testing / smoke test

The headless test suite doubles as a smoke test — it loads the shipped content,
generates dungeons, and simulates a full clear:

```powershell
ctest --preset debug      # or: ctest --preset release
```

Both presets already pass `--output-on-failure`. Without presets, point `ctest`
at the build directory instead:

```powershell
ctest --test-dir build-msvc --output-on-failure
```

All tests should pass. For a manual smoke test, launch the exe and: start a new
game, enter a dungeon from the Guild, win a battle (numbers float, XP awarded),
open a chest, retreat or clear the boss, and confirm the score screen and that a
save round-trips via the Save Point + Continue.

## Known limitations

- **Generated assets.** All art (16-bit-style pixel tiles/sprites), the UI
  **bitmap font** (an original pixel typeface + BMFont descriptors), and all
  audio (17 chiptune music tracks, 4 ambience beds, 15 SFX) are original and
  produced by deterministic in-repo generators (`tools/asset_gen/`). Every
  sound and visual role is replaceable without code via
  `assets/manifest.json` (see `docs/asset_pipeline.md`; debug builds reload
  with F5); missing files fall back to synthesized placeholders or silence.
- Status effects include poison, attack/defense buffs/debuffs, and (M35)
  Blind (physical attacks usually miss), Silence (no MP-cost skills), and
  Confusion (forces a basic attack at its own side, on both sides equally since
  M43) — all deterministic and seeded. **Elements (M48)** are a deliberately
  sparse layer: a handful of foes are weak (×150 %) or immune (0 damage, and no
  status rider) to one element, carried by elemental spells and by five
  elemental weapons; affinities are shown in the bestiary and the battle target
  panel for foes you have met. Bosses use
  stats, skills, minions, telegraph text, and a Brute enrage; dynamic summons and
  true multi-wave "rush" are not implemented.
- Equipment has no per-class restrictions; the economy is lightly tuned.
- The seven towns share one fixed layout (exterior palette, service interiors,
  and music vary per town); no per-character portraits.

## Originality & assets

All content (names, classes, enemies, bosses, items, skills, themes, story
flavor, UI) is original to this project. No copyrighted assets are used. All
audio and tile/sprite visuals are original, produced by the deterministic
generators in `tools/asset_gen/`; provenance is recorded in
`assets/credits.md`.

## Documentation

- [`docs/game_design.md`](docs/game_design.md) — what the game is and why.
- [`docs/technical_design.md`](docs/technical_design.md) — architecture & conventions.
- [`docs/milestones.md`](docs/milestones.md) — milestone ledger and status.
- [`docs/completion_roadmap.md`](docs/completion_roadmap.md) — post-M10
  completion (M11–M24) and polish (M25–M30) program direction and quality
  targets.
- `CLAUDE.md` — the project's operating contract.
