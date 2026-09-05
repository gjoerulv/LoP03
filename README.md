# Are P Geese

> Formerly *Crystal Dungeons* - renamed in M108 (the old name was taken, and the stranger may be a goose; we ask, politely: Are P Geese?).

A 16-bit-inspired, turn-based **JRPG roguelite** about clearing seeded dungeons
**efficiently**. Take a party of four from a town hub into procedurally generated
dungeons full of *visible* enemy teams and guarded chests, beat the boss, and
score on how few battle turns you spent — then upgrade and dive again, forever.

Original work — not a clone of any existing game; no copyrighted names, art,
music, or text. Built in **C++20** with **raylib**.

> **Status: feature-complete, polished playable build** (milestones
> **M1–M108** delivered and owner-approved — the M98–M108 "Are P Geese"
> program, from the post-M97 fix batch through the full rebrand at
> **v0.7.0**, approved 2026-09-02; the **M109–M116 program** — the lifetime
> ledger, the patrol dispatcher with the Golden Goose, the Jester's lore
> trap and the treasure chests, cutscene stages, the font readability
> redesign, the Last Dragon redesign and the End-game Summary — is
> **implemented, awaiting manual approval**). In the box: a seven-town difficulty ladder
> plus a castle endgame far above it (Boss Rush with escorts / Endless Rush /
> the Hollow King flanked by his reviving Royal Guards), seeded walkable
> dungeons with room events including the rare Royal Relics and per-theme
> rites, deterministic turn-based combat (statuses, passives, forced-action
> turn-control, sparse elemental weaknesses/immunities, an enmity/threat
> model with control skills), a stakes-escalation score rule with an honest
> tagged scoreboard, a black market and legendary tokens/drops, per-town
> enemies/bosses/equipment with original generated art and music, a
> light-hearted story serial, three unlockable reward classes (Dragon /
> Jester / Goose), learnsets, shops, a paid inn, compact walk-through towns,
> five save slots + autosave, a debug toolbelt in dev builds, onboarding,
> accessibility options, categorized settings (a 0–10 CRT Strength filter,
> background audio, an independent ambience slider), an in-battle action
> log, a bestiary (with each foe's strongest-context stats), victory
> records, achievements, boss battle backdrops with a Crystal Shatter
> intro, and a fully procedural "8-bit-plus" UI.
> The newest additions (M59–M74, approved 2026-08-05): the **CrystalForge content editor**
> (M59–M60, see *Development tools*), **Goose Town & the Deadly Duck**
> (M61 — fell the King with a Goose in the party and the ultimate gauntlet
> opens), the **M62 polish** (Purify truly heals nothing, bespoke
> goose/duck art, the Duck's own battle theme), and the **M63 class level
> milestones** (pick 1 of 2 permanent class bonuses at levels 10/20/30 —
> all nine classes), **M64 scroll learning + the Party panel** (skill
> scrolls finally teach; a detailed party ledger on both pause menus), and
> the two treasure-map systems: **M65's town puzzle map** (a HoMM2 homage —
> four Secret Map Pieces reveal a boss-guarded dig paying exclusive Lost
> Scrolls) and **M66's single-use dungeon charts** (a minimap X, twelve
> collectable curios, the Curator achievement), plus the **M67 polish
> batch** (class portraits across the menus, milestone/passive
> descriptions in the party panel, load-screen fixes, and the
> boss-victory return-to-town fix), **M68** (a victory spoils panel
> with level-up diffs on the battle's own final beat, and threat labels
> recalibrated to be relative to YOUR party), and **M69** (real town
> facades with integrated doors; the Scoreboard as a stone monument and
> the Save Point as a crystal), **M70** (CRT Strength and CRT
> Curvature as separate 0–10 sliders — the screen only bends as much as
> you ask), and **M71** (a victory celebration after flawless-stakes
> clears and the great challenge wins — the team jumping, the MVP on a
> pedestal), and **M72** (the party panel reflowed so a maxed member's
> milestones and skills all stay visible), **M73** (every enemy and boss
> sprite redrawn as hand-authored pixel grids — bosses on a larger 36×36
> canvas with real silhouettes), and **M74** (the Crystal Mine ambience
> rebuilt around rockfall and crystal-shard echoes — the old "drips" were
> synthesised as bird whistles). The **M75–M86 expansion program**
> (approved milestone by milestone through 2026-08-07) then added: three
> new statuses (**Reflect / Sleep / Curse**) with authored counterplay
> skills and items, a deterministic **boss trigger system**, an enemy &
> boss offensive pass (King and Duck reworks included), **held-item caps**
> and shop UX, party-cycling keys and a **three-slot keyboard remap**,
> dungeon **event flavor** panels, elemental weapons + resist charms +
> **gear icons**, **1-or-4-floor dungeons** with split scoreboards, a
> map-piece economy, per-town **Guild Masters** paying permanent town
> perks, the curio-gated **Last Dragon** at the castle, and the
> CrystalForge catch-up with the version renumbered to 0.6.0. **M87**
> then made every text container translation-ready (bounded scrollable
> prose, a Latin-1 bitmap font). The newest work, the **M88–M97 program**
> (approved 2026-08-16): town/Guild/shop UX fixes with a
> hand-editable seed and pre-fight **team inspection**, the Dragon's
> fixed breath economy and a no-free-heal dungeon **carry-out**, Equip
> Party + usable Items on both pause menus, per-element hit effects and
> sounds, **20-floor descents** with a skill-scroll prize, fog-of-war
> minimaps with a paid Surveyor reveal, a visible 100-step **patrol
> countdown**, a dragonform pact, a Training Hall **sparring mirror** (AI
> or manual control of the echoes), three once-per-run **summons** dug
> from treasure maps, sixteen worn **heirlooms** with triggered effects,
> and an eight-scene **cutscene story** told by a hooded goose, now over
> painted stages; since M109–M116 also a **seeded patrol mixture** (the
> Golden Goose hunt, the Jester's lore question, three chests and the
> Mimic, a Stranger scene), a persistent **lifetime ledger** with a
> six-page **End-game Summary**, and the typeface redrawn on a 6×9 cell.
> With the M98–M108 approvals (2026-09-02) only the M109–M116 owner
> approval and the deferred **validation playtesting (M23)** and
> **release sign-off (M24)** remain. Current status always lives in
> `docs/milestones.md`.

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
.\build-msvc\ArePGeese.exe

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
# -> dist\ArePGeese-<version>-win64.zip
```

The version is set once in `CMakeLists.txt` `project(VERSION ...)` and flows
into the exe metadata, the title screen, and the package name.

### Ninja without presets

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build-msvc
.\build-msvc\ArePGeese.exe
```

`-DCMAKE_*_COMPILER=cl` forces MSVC so no other compiler on `PATH` is picked
by mistake. CMake copies `data/` and `assets/` next to the executable; the
**deliverable is the staged folder produced by `tools\package.ps1`**.

### Visual Studio generator (alternative)

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
.\build-msvc\Release\ArePGeese.exe
```

## Controls

Default bindings — everything except a few fixed keys (text-delete, the
debug toggle, the debug-build F5 asset reload) is
**remappable in-game** under **Main Menu → Settings** (also reachable from the
pause menus); since M79 every keyboard action offers **three slots**
(Primary / Alt 1 / Alt 2), with a steal-with-confirm warning when a key is
already in use. Settings are organized into **Audio / Display / Gameplay /
Controls** submenus (M51): master/music/SFX volumes, a separate **Ambience
Volume** slider (M52; default 3 since M79) and a **Background Audio**
toggle; window
mode, **CRT Strength** and **CRT Curvature** sliders (M57/M70, each 0–10),
battle flash/shake,
and high-contrast; battle/message speed and tutorial prompts; and per-device
remapping. All of it persists in `settings.json` in the user data folder;
one-time tutorial-prompt progress persists in `tutorial.json` beside it. By
default the game **mutes when its window loses focus** (turn on Background Audio
to keep it playing).

| Action                | Keyboard               | Gamepad            |
|-----------------------|------------------------|--------------------|
| Move / Navigate       | Arrows or WASD         | D-Pad / Left Stick |
| Confirm               | Enter, Space, or Z     | A                  |
| Cancel / Back         | Esc, Backspace, or X   | B                  |
| Menu / Pause          | Tab                    | Start              |
| Details / Info        | C                      | Y                  |
| Prev / Next member    | Q / E (or Ctrl / Alt)  | L1 / R1            |
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
   free with a rest token), **Item Shop** (buy consumables — held
   quantities are capped per item since M78), **Equip Shop**
   (buy by category + equip gear — each town unlocks stronger gear as you climb),
   **Training Hall** (level up, and buy passive skills — own many, equip one),
   **Scoreboard**, **Save Point** (5 slots), and the **Guild**. **Walk out the
   west/east roads** to move between the **seven towns** (no button — just walk
   into the road); each later town raises enemy stats (up to +200 %) and score
   bonus (up to +100 %); clearing a dungeon in a town unlocks the road onward.
3. At the **Guild**, pick a theme, a depth, and **1, 4, or 20 floors**
   (M82/M92 — a multi-floor run keeps the boss on the last floor behind
   Stairway Warden gates, posts to its own scoreboard, and in town 2+ can
   drop **Secret Map Pieces**; a stakes-raising 20-floor clear also opens
   a pick-one **skill-scroll trove**), then enter a seeded dungeon. Entering
   autosaves. The Guild also hosts **"Fight the Guild Boss"** (M84):
   clear a 4-floor dungeon in that town to unlock its unique Master —
   the first victory pays a pick-1-of-2 **permanent town perk**.
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
   place above the ladder with **four challenges**: the **Boss Rush**
   (all 15 dungeon bosses back-to-back **with their minions**, no free
   healing), the
   **Endless Rush** (escalating waves — every 10th fields a boss and its
   court), **the
   Hollow King** himself (immune to blind/silence/confusion,
   flanked by **two Royal Guards he calls back from the dead every five turns**;
   beat him for a unique legendary and a title), and — for collectors of
   all **twelve curios** — **the Last Dragon** (M85): three elite waves,
   then the game's largest single fight. The castle keeps its **own records**, separate from your dungeon scores.
   Failing (or fleeing) a challenge costs **no gold** — but nobody is healed:
   survivors are carried to the gates at **1 HP**, the fallen stay fallen, and a
   full wipe leaves exactly one member standing so an inn is always reachable.
8. One secret remains beyond the castle: defeat the King with **at least one
   Goose in the party** and the north road forks to **Goose Town** — a pond-side
   hub with the game's true final fight, a two-stage no-heal gauntlet (five Evil
   Geese, then the **Deadly Duck**) with its own best-turns Pond Record.

## Project layout

```
src/
  core/      Application loop, AppContext, config, FadeController (transitions)
  render/    VirtualScreen (426x240 scaling + CRT shader), Viewport, RAII wrappers
  audio/     AudioManager (manifest-driven music/ambience/SFX, synth fallback)
  assets/    AssetManifest loader/validator (logical asset IDs)
  input/     action mapping (keyboard + gamepad)
  resource/  cached textures/fonts with graceful fallback
  platform/  user-data paths, path sanitizing, atomic file writes
  settings/  versioned settings persistence
  tutorial/  one-time contextual onboarding prompts
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
  capture/   deterministic screenshot scenes (dev builds only)
  editor/    CrystalForge content editor (separate dev tool; never shipped)
data/        JSON content (14 files: skills, classes, enemies, items, bosses,
             themes, composition, passives, milestones, story, tutorials,
             event_flavor + curio_lore + cutscenes)
assets/      manifest.json + generated textures/audio/font + credits.md
tools/       package.ps1 + deterministic asset generators (asset_gen/)
tests/       Catch2 unit/integration tests (headless)
docs/        design + technical + milestone docs
```

## Development tools

**CrystalForge** (M59) is a designer-facing content editor built alongside the
game (`CRYSTAL_ENABLE_EDITOR`, on by default; `tools/package.ps1` never stages
it). It links the game's own loader, validator, and battle simulator, so what
it accepts and what it simulates can never drift from the game:

```powershell
cmake --build --preset debug --target CrystalForge
.\build-msvc\CrystalForge.exe
```

It edits the **source-tree `data/`** (pass `--data <dir>` to point elsewhere)
in a 1280x720 window: pick a category, pick an entry, edit fields; `Ctrl+S`
saves through an atomic canonical writer and re-validates everything (errors
jump to the offending entity); `F5` re-validates on demand and runs a
three-battle quick-sim sanity battery. `N`/`D`/`Del` add, duplicate, and
delete entries (deletes warn about dangling references). The game reads its
content at startup — restart it (or rebuild, which recopies `data/`) to see
edits in play. `CrystalForge --canonicalize` reformats every data file through
the canonical writer headlessly (used once at M59; safe to re-run — it proves
values unchanged by re-validating through the real loader).

See `docs/editor_guide.md` for the full designer workflow.

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

- **Generated assets.** All art (16-bit-style pixel tiles/sprites — 100
  battle sprites incl. 27 bosses — plus five painted cutscene stages), the UI
  **bitmap font** (an original 161-glyph pixel typeface on a 6×9 cell, three
  BMFont descriptors), and all audio (22 chiptune music tracks incl. the
  mocking jingle, 5 ambience beds, 22 SFX) are original and
  produced by deterministic in-repo generators (`tools/asset_gen/`). Every
  sound and visual role is replaceable without code via
  `assets/manifest.json` (see `docs/asset_pipeline.md`; debug builds reload
  with F5); missing files fall back to synthesized placeholders or silence.
- Status effects: poison, attack/defense buffs/debuffs, (M35) Blind
  (physical attacks usually miss), Silence (no MP-cost skills), and
  Confusion (forces a basic attack at its own side, on both sides equally since
  M43), (M44) Terrified and Stunned, and (M75) **Reflect, Sleep, and
  Curse** — all deterministic and seeded; Curse is the one status ordinary
  cures never lift (a dedicated skill or item does). **Elements
  (M48/M81)** are a deliberately
  sparse layer: a handful of foes are weak (×150 %) or immune (0 damage, and no
  status rider) to one element, carried by elemental spells and **ten**
  elemental weapons and answered by seven resist accessories; affinities
  are shown in the bestiary and the battle target
  panel for foes you have met. Bosses use
  stats, skills, minions, telegraph text, archetype mechanics, and (M75)
  deterministic triggers — one can even raise a clone of itself; true
  mid-fight reinforcements are still not implemented (the endgame
  gauntlets run their waves as separate battles).
- The six starting classes share all equipment (no per-class
  restrictions); each reward class bans slots (the Goose wears no arms or
  armor — an M96 heirloom is the one keepsake anyone may hold).
  The economy is lightly tuned.
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
