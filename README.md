# Crystal Dungeons

A 16-bit-inspired, turn-based **JRPG roguelite** about clearing seeded dungeons
**efficiently**. Take a party of four from a town hub into procedurally generated
dungeons full of *visible* enemy teams and guarded chests, beat the boss, and
score on how few battle turns you spent — then upgrade and dive again, forever.

Original work — not a clone of any existing game; no copyrighted names, art,
music, or text. Built in **C++20** with **raylib**.

> **Status: feature-complete playable build** (Milestones 1–10). Town hub,
> seeded walkable dungeons, deterministic turn-based combat with status effects,
> danger ratings, scoring + scoreboard, content (classes/enemies/elites/bosses/
> items/skills/themes), equipment, XP/leveling, shops, placeholder audio, and a
> controls screen are all in. Art and audio are intentionally placeholder.
> A post-M10 completion program (M11–M24: UI/text safety, input remapping,
> replaceable assets, final art/audio, compact rooms, accessibility, release
> polish) is planned — see `docs/completion_roadmap.md`.

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

Build from the **Visual Studio developer environment** so `cl` and the bundled
CMake/Ninja are on `PATH`: open a **"Developer PowerShell for VS"** (or run
`vcvars64.bat` in a normal shell first):

```powershell
& "C:\Program Files\Microsoft Visual Studio\<edition>\VC\Auxiliary\Build\vcvars64.bat"
```

### Ninja (recommended)

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build-msvc
.\build-msvc\CrystalDungeons.exe
```

Use `-DCMAKE_BUILD_TYPE=Debug` for a debug build. `-DCMAKE_*_COMPILER=cl` forces
MSVC so no other compiler on `PATH` is picked by mistake. CMake copies the `data/`
folder next to the executable; the **deliverable is `CrystalDungeons.exe` plus the
`data/` folder beside it**.

### Visual Studio generator (alternative)

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
.\build-msvc\Release\CrystalDungeons.exe
```

## Controls

| Action                | Keyboard               | Gamepad            |
|-----------------------|------------------------|--------------------|
| Move / Navigate       | Arrows or WASD         | D-Pad / Left Stick |
| Confirm               | Enter or Space         | A                  |
| Cancel / Back         | Esc or Backspace       | B                  |
| Menu / Pause          | Tab                    | Start              |
| Adjust (Guild theme/depth, etc.) | Left / Right | D-Pad L/R          |
| Toggle debug overlay  | F1                     | —                  |

The same list is in-game under **Main Menu → Controls**. The window is resizable;
the 426×240 image always scales to fit with letterbox/pillarbox bars.

## How to play

1. **New Game** → pick 4 classes (Knight, Ranger, Mage, Cleric, Rogue, Guardian)
   and name them. You start with a little gold.
2. In the **town**, walk to buildings: **Inn** (free full heal), **Item Shop**
   (buy consumables), **Equip Shop** (buy + equip gear), **Training Hall** (pay
   gold to level up), **Scoreboard**, **Save Point** (3 slots), and the **Guild**.
3. At the **Guild**, pick a theme + depth and enter a seeded dungeon. Entering
   autosaves.
4. Walk the dungeon: enemy teams show a **danger tier**; fight them to clear
   **gates** (≥3 before the boss) and chest guards. Win battles to earn **XP and
   gold**; open chests for loot.
5. Beat the **boss** to clear the dungeon and post a **score** (driven mainly by
   *fewest battle turns*, plus danger defeated, treasure, and a no-death bonus).
6. **Retreat** any time (you keep XP/gold but score 0). **Defeat** returns you to
   town with half your gold. Upgrade, then dive deeper — runs scale with depth and
   seed, endlessly.

## Project layout

```
src/
  core/      Application loop, AppContext, config, FadeController (transitions)
  render/    VirtualScreen (426x240 scaling), Viewport, raylib RAII wrappers
  audio/     AudioManager (synthesized placeholder SFX + music)
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
ctest --test-dir build-msvc --output-on-failure
```

All tests should pass. For a manual smoke test, launch the exe and: start a new
game, enter a dungeon from the Guild, win a battle (numbers float, XP awarded),
open a chest, retreat or clear the boss, and confirm the score screen and that a
save round-trips via the Save Point + Continue.

## Known limitations

- **Placeholder art and audio.** Tiles, characters, and enemies are colored
  rectangles/glyphs; SFX and music are simple synthesized tones. Both are
  isolated and easy to replace (audio degrades to silence if unavailable).
- Status effects are a focused set (poison + attack/defense buffs/debuffs).
  Bosses use stats, skills, minions, telegraph text, and a Brute enrage; dynamic
  summons and true multi-wave "rush" are not implemented.
- Equipment has no per-class restrictions; the economy is lightly tuned.
- Single fixed town; no per-character portraits.

## Originality & assets

All content (names, classes, enemies, bosses, items, skills, themes, story
flavor, UI) is original to this project. No copyrighted assets are used. Audio
and tile/sprite visuals are generated in code as placeholders.

## Documentation

- [`docs/game_design.md`](docs/game_design.md) — what the game is and why.
- [`docs/technical_design.md`](docs/technical_design.md) — architecture & conventions.
- [`docs/milestones.md`](docs/milestones.md) — milestone ledger and status.
- [`docs/completion_roadmap.md`](docs/completion_roadmap.md) — post-M10
  completion program (M11–M24) direction and quality targets.
- `CLAUDE.md` — the project's operating contract.
