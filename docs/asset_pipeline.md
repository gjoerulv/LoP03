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
  `boss.` `bg.` `building.` `marker.` `prop.` `anim.` `music.` `ambience.`
  `sfx.` (`effect.` is reserved — no entries or directory ship today).
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
tests validate the shipped manifest against them). All **48** shipped WAVs
(21 music, 5 ambience, 22 SFX — 30 at M21, grown by the per-town, castle,
King and Duck tracks, the six M91 elemental hits, the M107 summon
arrival, and the M106 Goosy strut + wetland bed; the mine bed rebuilt
in M74) are original, produced by
`tools/asset_gen/generate_audio.ps1` (deterministic — reruns are
byte-identical). Since the owner-directed 2026-08-08 extension the music
note tables live in `tools/asset_gen/music_data.ps1` (sectioned 24–46s
arrangements; art_bible §9) — one source of truth shared with
`tools/asset_gen/generate_midi.ps1`, which exports every tune as a
Standard MIDI File into the git-ignored `docs/music/` for DAW editing
(never shipped; ambience and SFX are untouched by the music pipeline).

| Role id | Used for |
|---|---|
| `sfx.ui.{move,confirm,cancel,error}` | menus; `error` = refusals (can't pay/afford) |
| `sfx.battle.{hit,hit_magic,heal,status,ko,victory,defeat}` | combat feedback by action type |
| `sfx.battle.hit_{fire,ice,lightning,earth,holy,dark}` | M91 per-element impact accents (`audio::elementHitSfx`; a missing file remaps to `hit_magic` — see fallbacks) |
| `sfx.world.{chest,step,door,interact}` | exploration (steps are rate-limit cadenced) |
| `music.title` / `music.town` / `music.guild` | scene music (streamed loops) |
| `music.dungeon.{keep,mine,forest}` | per-theme dungeon music (owner decision) |
| `music.town.<2..7>` | per-town variants of the town track (M32/M50; `AudioManager::setTown`) |
| `music.battle` / `music.boss` | normal vs boss battles |
| `music.castle` / `music.king` / `music.duck` | castle hub, King fight (M40), Duck wave (M62) |
| `music.victory` / `music.defeat` | one-shot jingles (`loop: false`) at battle end |
| `music.result` | dungeon result screen |
| `ambience.{town,keep,mine,forest}` | looping beds layered under music (mine bed reworked M74) |

**Fallback order (owner-approved):** manifest file → synthesized placeholder
tone → silence; every miss logs a warning, nothing crashes. New M21 music
roles map to the nearest M8 synth loop (`kSynthMusicIndex`); a missing
victory/defeat jingle falls back to the matching stinger SFX; a missing M91
elemental hit remaps to `hit_magic` before any other tier (M14 rule, applied
in `AudioManager::play`); ambience has no synth tier (silence). File-backed music uses raylib music streams with the
manifest `loop` flag; track changes crossfade over 0.25 s; rapid SFX are
rate-limited per role (`kSfxMinInterval`). Volumes combine group settings
(M13 Settings screen) × per-asset `volume`; since **M52** ambience has its
**own volume slider** (`ambienceVolume`, default 3/10 since M79 — 5/10 at
M52; it followed the SFX
slider from M27 to M52, and the music slider before that).

Texture/font roles follow the same pattern (placeholder checker / default
font as fallback); visual role names are assigned in M15/M17 as art lands.

**Fonts (M25; Latin set M87):** the shipped UI font is original, produced by
`tools/asset_gen/generate_font.ps1` (deterministic — reruns are byte-identical),
which emits one 5×7 proportional glyph design — **161 glyphs**: printable
ASCII 32–126 plus, since M87, every Latin-1 Supplement letter and `¡ ¿ « »`
(accents in the cell's top two rows, compressed capitals; ASCII glyphs are
byte-identical to M25). The supported-codepoint authority is
`src/ui/GlyphCoverage.hpp`, enforced by `tests/test_glyph_coverage.cpp`
against the emitted `.fnt` char ids AND all shipped `data/*.json` text. The
output is a PNG atlas + three BMFont `.fnt` descriptors sharing it:

| Role id | File | Base size (`lineHeight`) | Used for |
|---|---|---|---|
| `font.ui.small` | `fonts/font_small.fnt` | 8 | HUD/caption text (size 8) |
| `font.ui.main` | `fonts/font_main.fnt` | 10 | body/menu text (size 10–15) |
| `font.ui.title` | `fonts/font_title.fnt` (2× atlas) | 20 | headings/title (size ≥ 16) |

`ui::setFonts` picks the base whose native size is nearest the requested size,
so pixel glyphs stay crisp at the dominant sizes and scale from the nearest
base (point-filtered) elsewhere.

**Enemy battle sprites (M26; art redrawn M73):** every content enemy/boss id
has its own battle sprite — `enemy.<id>.battle` (24×24) and `boss.<id>.battle`
(**36×36** since M73, 32×32 before) — generated by `generate_textures.ps1`.
**98 sprites ship: 32 normal + 40 elite + 26 boss** (three of them the
generic tier fallbacks `enemy.normal/elite.battle` and `boss.generic.battle`,
retained) — the M73 set plus M84's seven Guild Masters (36×36) and twelve
guild minions (24×24), M85's Last Dragon (36×36), and the eleven Goosy
Gauntlet foes from the M106 art round (2026-08-17). Later sprites are
authored by **inserting before the M73 marker** in `generate_textures.ps1`,
so no other file's bytes shift (the no-RNG rule below is what makes that
safe). `BattleState` prefers the per-id sprite and falls back to the
generic tier sprite only for ids without bespoke art.
`tests/test_presentation_lint.cpp` enforces this: a content id missing its
`enemy.<id>.battle` / `boss.<id>.battle` row **fails** `[lint]`, so new content
cannot ship without art. Every addition is recorded in `assets/credits.md`.

Boss *design* rules (masses, crowns, asymmetry, tone) live in
`docs/art_bible.md` **§5b** — the 2026-08-05 boss-art repair redrew all 14
non-Duck boss grids under them. Three M73 mechanical rules govern the whole
family:

- **Authoring is an explicit ASCII pixel grid**, one block per sprite with a
  single-character palette key bound to the art-bible §2 ramps. `Draw-Grid`
  rejects ragged rows and unknown keys; `Save-EnemyGrid` rejects any `boss_*`
  that is not 36×36.
- **The section consumes no RNG.** Every speckle pixel is hand-placed, so
  sprites can be added, removed or reordered without shifting any other file's
  bytes. (Sections that *do* use `Speckle` — environments, backgrounds, town
  exteriors — still reseed `$script:rng` at their start for the same reason.)
- **The boss canvas is bounded by battle layout, not taste.**
  `BattleState::drawUnit` anchors bottom-centre at
  `sy = enemyBaseY() + 16 - tex.height`; `enemyBaseY()` returns 20 once a fight
  fields 5+ enemies, which several authored fights do (three boss teams, plus
  the M84 Guild Trial and boss-court Endless waves). 36 rows is therefore
  the tallest sprite that never clips off the top of the screen, and 36 columns
  the widest that stays inside the 40px unit footprint. Going larger requires
  an `enemyBaseY()` change and owner approval.

**Reviewing sprites (`tools/asset_gen/preview.ps1`, M73).** Nothing in this
family is finished until it has been looked at. The harness reads the
generated PNGs (it never writes to `assets/`) and emits, into
`docs/sprite_review/`: a magnified labelled **contact sheet** grouped
normal/elite/boss, a **silhouette sheet** rendering each alpha channel as solid
black on white, and a **1× strip** on the native 426-wide canvas plus a 4×
copy of it. The silhouette sheet is the binding artefact — two enemies that
read as the same shape there have failed regardless of colour. Review one
family at a time with `-Only goblin_grunt,kobold_scout -Zoom 12`.

**Gear icons (M81):** eleven **10×10** category icons —
`ui.icon.<category>` for sword / axe / dagger / bow / staff / mace / spear /
shield / armor / accessory / relic (the vocabulary is
`content::kIconCategoryIds`; `data/items.json` binds gear to it via
`iconCategory`, with slot-derived defaults for armor/accessory/relic and an
authored value required on every weapon). Authored as M73-style ASCII grids in
a **RNG-free section at the very end** of `generate_textures.ps1`
(`Save-IconGrid` hard-fails any non-10×10 grid), written to
`assets/textures/ui/icons/`. 10×10 is layout-bound like the boss canvas: menu
rows are 14px at font 10 and the party panel's gear lines sit on a 10px
pitch, so it is the largest square every render site fits at 1×. Rendered by
`ui::drawGearIcon` / `drawMenuScrolled` (equip shop, Armory Ghost, party
panel, black market at 2×); a missing texture draws nothing. The `[lint]`
sweep holds vocabulary ↔ shipped icon set in lockstep both ways, so gear
cannot ship icon-less. Review with `tools/asset_gen/preview_icons.ps1`
(magnified contact sheet on a dark and a light row →
`docs/sprite_review/icons_contact.png`).

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
  textures/{ui,actors,enemies,environments,backgrounds,props}/
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
