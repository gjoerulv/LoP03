# Asset Pipeline — Implemented Reference (M14)

> Status: **implemented** (M14; schema v1 owner-approved 2026-07-19).
> Modules: `src/assets/AssetManifest.*` (raylib-free parse/validate),
> `src/resource/ResourceManager` (logical-ID textures/fonts),
> `src/audio/AudioManager` (file-tier audio over the synthesized fallback).
> This is the authoritative how-to for adding or replacing assets.

## 1. The contract

States and systems request **stable dotted logical IDs**; `assets/manifest.json`
maps IDs to files and metadata. Replacing any asset for an existing role is a
manifest + file change — **never a C++ edit**. IDs are public identifiers once
shipped; renaming one is a schema-level decision.

## 2. Manifest schema (v2 — v1 plus animation entries)

```json
{
  "version": 2,
  "assets": [
    { "id": "sfx.ui.confirm",   "type": "sfx",     "path": "audio/sfx/confirm.wav", "volume": 0.9 },
    { "id": "music.town",       "type": "music",   "path": "audio/music/town.ogg",  "loop": true, "volume": 0.8 },
    { "id": "ambience.crystal_mine", "type": "ambience", "path": "audio/ambience/mine.ogg" },
    { "id": "font.ui.body",     "type": "font",    "path": "fonts/body.ttf", "size": 10 },
    { "id": "ui.frame.default", "type": "texture", "path": "textures/ui/frame.png", "filter": "nearest" },
    { "id": "anim.player.walk.down", "type": "animation", "texture": "actor.player.walk",
      "row": 0, "frameCount": 3, "frameWidth": 12, "frameHeight": 12, "frameTime": 0.15, "loop": true }
  ]
}
```

- `id` — dotted namespaces: `ui.` `font.` `tiles.<theme>.` `actor.` `enemy.`
  `marker.` `prop.` `effect.` `anim.` `music.` `ambience.` `sfx.`.
- `type` — `texture | font | music | ambience | sfx | animation`.
- `path` — relative to `assets/`, sanitized (no absolute paths, drives, `..`).
  Animation entries have **no path**; they reference a texture entry by id.
- Per-type optional metadata: texture `filter` (`nearest`|`bilinear`,
  default nearest); font `size` (default 10; used only to rasterize TTF/OTF —
  a BMFont `.fnt` atlas carries its own base size in the file); music/ambience
  `loop` (default true); music/ambience/sfx `volume` (0..1 multiplier,
  default 1).
- **Fonts (M25):** `ResourceManager::font` dispatches by extension —
  `.ttf`/`.otf` via `LoadFontEx` at `size`, `.fnt` (AngelCode BMFont) via
  `LoadFont` — and forces point filtering. A missing/failed font falls back to
  raylib's default font (never a crash). The UI installs its fonts with
  `ui::setFonts` after each manifest load and draws through the `DrawTextEx`
  wrappers in `src/ui/UiDraw`.
- Animation entries (v2, owner-approved 2026-07-19): `texture` (the strip),
  `row` (default 0), `frameCount`, `frameWidth`, `frameHeight` (required,
  ≥ 1), `frameTime` seconds (default 0.15, > 0), `loop` (default true;
  false = hold last frame). Frames run left-to-right on the given row.
  Version 1 manifests (no animations) still load.

**Validation** (headless-tested, `tests/test_asset_manifest.cpp` +
`tests/test_animation.cpp`): duplicate ids, unknown types, unsafe paths,
out-of-range volumes, missing files, malformed animation metadata, and
animations referencing a missing strip texture are reported and the entry
skipped — valid entries survive, the game never crashes. A missing
`manifest.json` is a valid empty catalog. The shipped manifest is validated
by the test suite with zero errors, including a PNG-header check that every
shipped animation fits inside its strip texture.

**Using an animation from code:** `resources.animation(id)` returns the
entry (or null → fall back to a static texture / shape);
`render::frameAt`/`frameRect` pick the frame; `render::drawAnimationCentered`
draws it anchored on the collision center. Callers keep their own clock —
pass 0 to show the first (stand) frame.

## 3. Audio roles (M21: full soundscape shipped)

The stable role tables live in `src/audio/AudioRoles.hpp` (raylib-free;
tests validate the shipped manifest against them). All 30 shipped files are
original, produced by `tools/asset_gen/generate_audio.ps1` (deterministic —
reruns are byte-identical).

| Role id | Used for |
|---|---|
| `sfx.ui.{move,confirm,cancel,error}` | menus; `error` = refusals (can't pay/afford) |
| `sfx.battle.{hit,hit_magic,heal,status,ko,victory,defeat}` | combat feedback by action type |
| `sfx.world.{chest,step,door,interact}` | exploration (steps are rate-limit cadenced) |
| `music.title` / `music.town` / `music.guild` | scene music (streamed loops) |
| `music.dungeon.{keep,mine,forest}` | per-theme dungeon music (owner decision) |
| `music.battle` / `music.boss` | normal vs boss battles |
| `music.victory` / `music.defeat` | one-shot jingles (`loop: false`) at battle end |
| `music.result` | dungeon result screen |
| `ambience.{town,keep,mine,forest}` | looping beds layered under music |

**Fallback order (owner-approved):** manifest file → synthesized placeholder
tone → silence; every miss logs a warning, nothing crashes. New M21 music
roles map to the nearest M8 synth loop (`kSynthMusicIndex`); a missing
victory/defeat jingle falls back to the matching stinger SFX; ambience has no
synth tier (silence). File-backed music uses raylib music streams with the
manifest `loop` flag; track changes crossfade over 0.25 s; rapid SFX are
rate-limited per role (`kSfxMinInterval`). Volumes combine group settings
(M13 Settings screen) × per-asset `volume`; ambience follows the **SFX**
slider (M27 owner decision — the two ambience gain sites in `AudioManager`
multiply by `sfxVolume_`, not `musicVolume_`; no new Settings field).

Texture/font roles follow the same pattern (placeholder checker / default
font as fallback); visual role names are assigned in M15/M17 as art lands.

**Fonts (M25):** the shipped UI font is original, produced by
`tools/asset_gen/generate_font.ps1` (deterministic — reruns are byte-identical),
which emits one 5×7 proportional glyph design (printable ASCII 32–126) as a PNG
atlas + three BMFont `.fnt` descriptors sharing it:

| Role id | File | Base size (`lineHeight`) | Used for |
|---|---|---|---|
| `font.ui.small` | `fonts/font_small.fnt` | 8 | HUD/caption text (size 8) |
| `font.ui.main` | `fonts/font_main.fnt` | 10 | body/menu text (size 10–15) |
| `font.ui.title` | `fonts/font_title.fnt` (2× atlas) | 20 | headings/title (size ≥ 16) |

`ui::setFonts` picks the base whose native size is nearest the requested size,
so pixel glyphs stay crisp at the dominant sizes and scale from the nearest
base (point-filtered) elsewhere.

**Enemy battle sprites (M26):** every content enemy/boss id has its own battle
sprite — `enemy.<id>.battle` (23, 24×24) and `boss.<id>.battle` (4, 32×32) —
generated by `generate_textures.ps1`. `BattleState` prefers the per-id sprite
and falls back to the generic tier sprite (`enemy.normal/elite.battle`,
`boss.generic.battle`, retained) only for ids without bespoke art.
`tests/test_presentation_lint.cpp` enforces this: a content id missing its
`enemy.<id>.battle` / `boss.<id>.battle` row **fails** `[lint]`, so new content
cannot ship without art.

**Service backgrounds (M27):** six full-screen (426×240) service backgrounds —
`bg.inn`, `bg.item_shop`, `bg.equip_shop`, `bg.training_hall`, `bg.scoreboard`,
`bg.guild` — generated by `generate_textures.ps1`. States draw them via
`ui::drawSceneBackground(resources, id, fallbackColor, w, h)`, which fills the
old solid colour first and overlays the texture only if the catalog has it, so
a missing background degrades to the previous flat fill. Backgrounds are
authored dark/low-contrast (art bible §7) to keep overlaid text legible.

## 4. How to add or replace an asset

1. Put the file under the matching `assets/` subtree
   (`textures/…`, `fonts/…`, `audio/{music,ambience,sfx}/…`).
2. Add or edit its entry in `assets/manifest.json`.
3. **Record it in `assets/credits.md`** (file, role, source/author, license,
   attribution). A shipped external asset without an entry is a release
   blocker (M24 verifies).
4. Run the game — debug builds can press **F5** to reload the manifest live
   (caches drop, callers re-fetch by id, current music restarts on the
   re-resolved tier). Or just relaunch.
5. `ctest` validates the shipped manifest automatically.

The pipeline is proven end-to-end by `sfx.ui.confirm`: the shipped
`audio/sfx/confirm.wav` (original, generated) replaces the synthesized
confirm tone with zero C++ involved — delete its manifest entry and the old
tone returns.

## 5. Directory layout

```text
assets/
  manifest.json
  credits.md
  fonts/
  textures/{ui,actors,enemies,environments,effects}/
  audio/{music,ambience,sfx}/
```

CMake copies `assets/` next to the executable exactly like `data/`; the
release package (M24) ships both trees.

## 6. Engineering rules

- Parsing/validation stays raylib-free (`src/assets/`); GPU/audio loading
  lives in the managers and requires the window/device.
- Reload model (owner-approved): **callers cache nothing** — request by id at
  use time; `ResourceManager::reload()` clears its cache; holding a
  `Texture2D&`/`Font&` across a reload is a bug.
- Presentation data stays out of `data/` (gameplay content) — changing art
  can never change combat or generation.
- New asset *types* or metadata semantics = manifest schema revision =
  owner approval first.
