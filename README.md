# Crystal Dungeons

A 16-bit-inspired, turn-based **JRPG roguelite** about clearing seeded dungeons
**efficiently**. Take a party of four from a town hub into procedurally generated
dungeons full of *visible* enemy teams and guarded chests, beat the boss, and
score on how few battle turns you spent — then upgrade and dive again, forever.

Original work — not a clone of any existing game; no copyrighted names, art,
music, or text. Built in **C++20** with **raylib**.

> **Status: feature-complete, polished playable build.** Town hub, seeded
> walkable dungeons, deterministic turn-based combat with status effects, an
> enmity/targeting model with control skills, danger ratings, scoring +
> scoreboard, content (classes/enemies/elites/bosses/items/skills/themes) with
> level-based class learnsets, equipment, XP/leveling, shops, a paid inn, and a
> controls screen are all in. The post-M10 completion program (M11–M22) and the
> M25–M30 polish program — UI/text safety, input remapping, replaceable assets,
> generated 16-bit-style art, compact rooms, encounter variety, a full original
> soundscape, onboarding + accessibility, an original bitmap font, per-enemy
> art, environment/ambience identity, smarter enemy AI, expanded content, and a
> paid-rest economy — are delivered, as is the **M31–M34 expansion**: a
> seven-town difficulty ladder with per-town art and music, a stakes-escalation
> score rule, a black market selling legendary gear for gold or elite-challenge
> tokens, and categorized equip shopping. The **M35–M42 endgame program**
> (statuses v2, passives, per-town content, boss drops, a castle with the King,
> story, and enrichment features) is underway — **M35** (statuses + to-hit layer),
> **M36** (10 passive skills), **M37** (per-town equipment), **M38** (12 per-town
> enemies + 6 per-town bosses), and **M39** (seeded, reload-proof boss legendary &
> token drops), **M40** (the castle above the seven towns, with Boss Rush /
> Endless Rush / the Hollow King challenges, its own records and rewards kept outside
> the dungeon scoreboard), and **M41** (an original light-hearted story serial — a
> wandering storyteller in every town and a Jester punchline at the castle) are
> approved, and **M42** (enrichment — a bestiary, result-screen victory stats +
> personal records, and ~16 achievements) is implemented and awaiting approval,
> closing the endgame program. The deferred
> **validation/balance playtesting (M23)** and **release packaging/sign-off
> (M24)** run last — see `docs/completion_roadmap.md` and `docs/milestones.md`.

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
pause menus). Settings, volumes, window mode, battle/message speed, effect
intensity, and the high-contrast palette persist in `settings.json` in the
user data folder; one-time tutorial-prompt progress persists in
`tutorial.json` beside it (toggle or reset in Settings).

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
   **Scoreboard**, **Save Point** (3 slots), and the **Guild**. Roads at the
   bottom corners connect **seven towns**: each later town raises enemy stats
   (up to +200 %) and score bonus (up to +100 %); clearing a dungeon in a town
   unlocks the road onward.
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
   **legendary tokens** won in optional elite challenges.
7. Clear any **town-7 dungeon** to open the northern road to the **castle** — a
   place above the ladder with the **King's three challenges**: the **Boss Rush**
   (all 12 bosses back-to-back, no free healing), the **Endless Rush** (escalating
   waves, survive as long as you can), and **the Hollow King** himself (the hardest
   fight — immune to blind/silence/confusion, beat him for a unique legendary and a
   title). The castle keeps its **own records**, separate from your dungeon scores.

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
  Confusion (attacks its own side) — all deterministic and seeded. Bosses use
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
