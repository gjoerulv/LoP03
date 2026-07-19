# Asset Pipeline — Current State and M14 Decision Draft (M11)

> Status: **decision draft** from the M11 audit at commit `8271871`. Nothing
> here is implemented; the version-1 manifest schema requires owner approval
> before M14 freezes it. M14 turns this into the authoritative pipeline doc.

## 1. Current state (audited)

- **`assets/` is empty** (`.gitkeep` only). Every visual is code-generated:
  colored rectangles, glyph markers, raylib default font.
- **`ResourceManager`** (`src/resource/`) caches by caller-supplied key and
  loads from a caller-supplied path (`texture(key, path)`,
  `font(key, path, size)`); missing/failed loads log and return a generated
  magenta-checkerboard placeholder / default font. RAII-owned; requires an
  initialized window. **No state currently loads any file asset** — the
  manager is exercised only by its fallback path.
- **`AudioManager`** (`src/audio/`) synthesizes every SFX and music loop at
  runtime into raylib `Sound`s behind a device guard; all calls are no-ops if
  the device failed. Music "streams" by re-playing a generated loop. Tracks:
  Town / Dungeon / Battle. SFX: move, confirm, cancel, hit, heal, KO,
  victory, defeat, chest.
- **Path safety:** `paths::sanitizeRelative()` exists and is the mandatory
  gate for any file access.
- **Packaging today:** CMake copies `data/` beside the exe; there is no
  `assets/` copy step (nothing to copy yet).

## 2. Proposed logical-ID model (for M14 approval)

States request stable dotted IDs; the manifest maps IDs to files + metadata:

```
ui.frame.default      ui.cursor.primary     font.ui.body
tiles.ruined_keep.floor_a                   actor.knight.overworld
enemy.crystal_slime.battle                  music.town.day
ambience.crystal_mine                       sfx.ui.confirm
```

Namespaces: `ui.` `font.` `tiles.<theme>.` `actor.` `enemy.` `effect.`
`music.` `ambience.` `sfx.`. IDs are public identifiers once shipped —
renaming one after release is a schema change.

## 3. Proposed manifest responsibilities (v1)

`assets/manifest.json`, versioned wrapper like the `data/` files:

- id → relative path (sanitized; no absolute paths, no `..`);
- asset type (texture / font / music / ambience / sfx);
- texture: filter mode, optional atlas frame metadata (extended in M17 with
  animation frames/timing/anchor — reserved, not designed here);
- font: base size, glyph-coverage policy;
- music/ambience: loop flag/points, default volume;
- sfx: default volume, optional variation set;
- reserved: theme/pack override table (design deferred);
- manifest `version` (starts at 1).

Validation (raylib-free, reusing `ObjectReader`/`LoadReport`): duplicate
IDs, unknown types, missing files, unsafe paths, malformed metadata → clear
errors, never crashes. Required-vs-optional IDs: a missing *required* ID is
a loud validation error; a missing *optional* ID degrades to fallback.

## 4. Required fallback behavior (unchanged principles)

- texture → generated checkerboard placeholder + log;
- font → raylib default font + log;
- music/ambience/sfx → synthesized placeholder if one exists for the role,
  else silence + log;
- never crash, never read outside the assets root.

Development diagnostics must stay loud even when the player experience
degrades gracefully.

## 5. Proposed directory organization

```text
assets/
  manifest.json
  credits.md            # provenance/license/attribution for every external asset
  fonts/
  textures/{ui,actors,enemies,environments,effects}/
  audio/{music,ambience,sfx}/
```

## 6. Build/package requirements

- CMake copies `assets/` beside the executable exactly like `data/` (M14).
- Release packaging (M24) ships `data/` + `assets/` + credits; a shipped
  external asset without a `credits.md` entry is a release blocker.
- Manual development reload command (debug builds only) re-resolves the
  manifest; handle semantics (stable handle vs re-fetch-by-ID) are an M14
  design decision that must be written down before implementation.

## 7. Attribution and licensing policy

Every external asset records at acquisition time: source, author, license,
license URL/text, and any required attribution string, in `assets/credits.md`.
Original assets are marked as original. No copyrighted or near-replica
material (CLAUDE.md).

## 8. Questions requiring owner approval before M14

1. Approve the v1 manifest schema shape (§3) before implementation freezes
   it (public-schema rule).
2. Logical-ID naming convention (§2) — IDs become public identifiers.
3. Sourcing strategy for real assets (commissioned / licensed / produced) —
   affects credits policy and M15 planning, can be deferred to M15.
4. Reload handle semantics (§6) — cheap to decide early, expensive to
   retrofit.
5. Whether `AudioManager`'s synthesized tones remain the permanent fallback
   tier under the manifest (recommended: yes) or silence-only.
