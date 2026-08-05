# M69 — Town exteriors: facades, the scoreboard stele, the save crystal

Owner feedback (2026-07-29): the top-row houses looked odd with doors
sticking out at the bottom — make them look like actual buildings; the
Scoreboard and Save Point need not be buildings at all (a monument shaped
like a big sheet, and a crystal). Implemented 2026-07-29.

## A. Status

**☑ complete (approved by the owner 2026-08-05)** — implemented 2026-07-29. Evidence in §E.

## B. As implemented

- **Facades**: the five service buildings (Inn, Item Shop, Equip Shop,
  Guild, Training Hall) draw a real 48×32 south-facing exterior over their
  3×2 Building tiles — pitched shingle roof with per-service colors, a
  plastered/timbered wall, two warm-lit windows, an emblem pennant
  (crescent / flask / sword / star / dumbbell), and the **door integrated
  into the facade's middle tile**, directly above the interact tile. That
  trigger tile now renders as the **doorstep path** (base + per-town
  variants) instead of the old flat door-on-the-ground — nothing sticks
  out anymore. Generated on a shared template in
  `tools/asset_gen/generate_textures.ps1` (appended + reseeded; all prior
  files byte-identical — verified via git status after regeneration).
- **Scoreboard stele** (`prop.scoreboard`, 32×32): a freestanding stone
  sheet on a plinth, its face engraved with score rows (the top one gold)
  and crowned. Footprint shrank 3×2 → **2×2**.
- **Save crystal** (`prop.save_crystal`, 16×16): a cyan crystal on dark
  rock with a lit facet. Footprint shrank 3×2 → **1×1**.
- **Mechanics unchanged**: `TownData` still builds seven `Building`
  entries with solid bodies and a walkable `Door` interact tile (the
  layout invariants in `[town]` hold as-is); the freed bottom-row tiles
  are plain walkable ground, clear of every seeded plaza tile (black
  market, bard, dig site — all chosen clear of the OLD, larger
  footprints). Missing textures fall back to the generic Building-tile
  blocks (placeholder discipline).

## C. Files changed

`town/TownData.cpp` (monument footprints), `states/TownState.cpp` (Door
tile renders as path; `structureSpriteId` + exterior draw pass),
`tools/asset_gen/generate_textures.ps1` (M69 section: 5 facades + 2
props), `assets/manifest.json` (+7 rows), `assets/credits.md` (+1 row),
docs. No save/schema/rules/generation changes; no test changes needed
(the town tests are invariant-based and pass unchanged).

## D. Automated validation (2026-07-29)

- Generator rerun: ONLY the seven new PNGs appeared; every existing asset
  stayed byte-identical (git status).
- `[town]`, `[treasure]`, `[assets]` batteries: 11 cases / 370 assertions
  green (layout invariants — doors reachable, walkable, in-bounds — and
  the manifest/file validation over the new rows).
- `--capture` **83/83 scenes clean**; `06_town` and `25_town_ladder`
  show the new exteriors at town 1 and town 6.
- Closing verification: **601/601 Debug and 597/597 Release tests green**
  (the 4-case gap is the debug-only god-mode battery); `--capture`
  **83/83 scenes clean**; zero project-code warnings.

## E. Manual owner checklist

1. Walk the town: the five top houses read as buildings (roof, walls,
   windows, integrated door with its doorstep below); entering each still
   works by standing on the doorstep + Confirm.
2. The Scoreboard is a stone sheet monument; the Save Point a crystal;
   both interact from the tile in front (north side), labels unchanged.
3. Towns 2–7: the facades sit on the darker per-town ground; judge
   whether they should darken per town too (currently they stay lit —
   services read as welcoming; easy to add variants if wanted).
4. Walk behind/around the shrunken monuments — the freed ground tiles are
   walkable and nothing clips.
5. **Art judgment is the owner's**: roof colors, pennant emblems, the
   stele's silhouette, the crystal's read at 16×16.

## F. Known limitations

- The facades are shared across towns (no per-town shading yet).
- The emblem pennants are tiny (5×8 px) — the name labels above the
  buildings remain the primary identification.

## G. Final status

`complete (approved 2026-08-05)`
