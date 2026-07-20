# M17 — Exploration Visuals and Animation

## A. Status and authority

- **Status:** complete (approved) — approved by the owner on 2026-07-19
  after manual testing (see §N).
- **Last reviewed repository commit:** `faa7ed8` (M16, 2026-07-19). Re-audit:
  manifest v1 (29 entries) live; M15 direction approved and generated
  in-project; M16 compact rooms landed (markers = prop sprites, themes keyed
  off `Dungeon.themeId`, only ruined_keep has tiles); player is a
  single-frame 12×12 sprite; no animation system exists.
- **Owner decisions (2026-07-19):** (1) **manifest schema v2 approved** — new
  `"animation"` entry type referencing an existing texture as a horizontal
  frame strip (`texture`, `row`, `frameCount`, `frameWidth`, `frameHeight`,
  `frameTime`, `loop`); loader accepts v1 and v2; missing/invalid animations
  fall back to a static frame or plain texture. (2) **All three themes are
  produced in this milestone with a single owner review at the gate** (the
  per-theme review gates were designed for commissioned art; generated art
  iterates by one-script changes).
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M17 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Planning fidelity:** architectural. Asset lists and animation-system
  details depend on the approved M15 art bible and the M16 room system;
  re-audit and refresh this note against the then-current repository before
  implementation begins.

## B. Problem statement

Town and dungeon presentation is abstract: code-generated colored tiles,
rectangle/glyph actors and enemies, minimal procedural feedback. After M14–M16
establish replaceable assets, an approved direction, and compact rooms, the
exploration layer still needs the actual production systems and content:
sprite animation, draw ordering, theme-specific environments, and readable
interactables — none of which exist at the reviewed commit.

## C. Goals

- A sprite animation system driven by manifest metadata: frame rectangles,
  timing, loop mode, anchor; missing animation definitions fall back to a
  static valid frame.
- Visual bounds separated from collision bounds (art size can change without
  touching gameplay collision).
- Deterministic draw layers with no depth errors.
- Final-quality exploration content per the art bible: player actor
  (facing/walk/idle), visible enemy silhouettes with danger/elite/boss
  differentiation, theme-specific tiles and props for Ruined Keep, Crystal
  Mine, and Hollow Forest, interaction highlights that do not rely on color
  alone, and restrained glows/particles/overlays.
- Theme identity through composition and shape, not palette swaps: masonry/
  defensive geometry (Keep); supports/rails/luminous clusters (Mine); roots/
  clearings/shrines (Forest).

## D. Non-goals

- No battle-scene presentation (M18).
- No audio content (M21).
- No room-layout or generation changes (M16 is closed).
- No collision or movement-rule changes.
- No decorative density that obscures exits, hazards, enemies, or chests.

## E. Dependencies

- M14 manifest (all art flows through it).
- M15 approved art bible + production-cost evidence.
- M16 compact rooms (environments are produced for the real room scale).
- Owner validation of readability at each production stage (theme by theme).

## F. Proposed slices (provisional — refine before starting)

1. **Animation system** — manifest animation metadata + pure playback logic
   (frame selection is testable headlessly) + renderer integration; static-
   frame fallback.
2. **Draw layers + bounds separation** — explicit layer ordering; visual
   anchor/bounds decoupled from `Tilemap` collision.
3. **Actors** — player sprites (facing/walk/idle) in town and dungeon; enemy
   team markers become silhouettes with danger/elite/boss differentiation
   (non-color-only).
4. **Theme production, one theme at a time** — Ruined Keep first (validates
   the pipeline), then Crystal Mine, then Hollow Forest; tiles, props,
   interactable indicators, restrained effects; owner review gates between
   themes.
5. **Town visual pass** — the town at the same quality bar.

## G. Expected systems affected

- New animation module (likely `src/render/` or a sibling; decide at
  implementation) + manifest metadata extensions (reviewed schema revision if
  needed).
- `src/states/DungeonState.cpp`, `src/states/TownState.cpp` — rendering paths.
- `src/dungeon/`/`src/town/` — only visual-marker hookup; collision untouched.
- `assets/` — the major content growth (textures + animation metadata +
  credits).
- `tests/` — animation playback logic, metadata validation.

## H. Data, schema, and save compatibility

- Asset manifest: likely extended with animation metadata — a reviewed,
  versioned manifest revision (owner-gated).
- Gameplay content (`data/`), saves, settings, deterministic seeds, scoring:
  **no impact.** Collision must remain independent of texture dimensions.

## I. Automated validation

- Animation-metadata validation (frames in bounds, durations positive, loop
  modes known, referenced textures exist).
- Pure playback tests (frame selection over time, loop/hold behavior,
  fallback).
- Asset lint: every referenced ID resolves; every shipped asset has a license
  record.
- Collision-independence regression tests (movement tests unchanged by art).
- Full suite green.

## J. Manual validation for the owner

- Walk each theme at native resolution and larger windows: silhouettes,
  exits, hazards, enemies, chests, and interactables all readable.
- Grayscale screenshot review per theme (composition-driven identity).
- Danger/elite/boss differentiation visible without color.
- Animation timing feels right (walk cycles, idle, indicator pulses).
- Missing-asset drill: remove an animation entry; confirm static-frame
  fallback, no crash.

## K. Acceptance criteria

- Player/enemy/interactable silhouettes readable at native resolution.
- Collision unaffected by texture dimensions.
- Draw ordering has no observable depth errors.
- Themes distinguishable in grayscale composition tests.
- Decorative density does not obscure gameplay information.
- Missing animation definitions fall back to a static valid frame.

## L. Risks and failure modes

- **Production-volume underestimate** — three themes of tiles/props/actors is
  the largest asset load in the program; the M15 cost estimate and
  theme-by-theme gating are the controls.
- **Readability regression** — atmosphere vs. legibility tension; grayscale
  and density checks are acceptance-relevant.
- **Per-frame allocation** — animation/effects introducing heap churn in the
  render loop; reuse buffers per the hot-path rule.
- **Draw-order bugs** — actor/prop overlap errors are easy to ship; owner
  walkthroughs plus explicit layer rules.
- **Scope bleed into M18/M20** — battle sprites and event props belong to
  later milestones.
- **Second-order:** visual markers changing perceived danger information —
  the danger tier must stay stat-derived and consistently presented.

## M. Required documentation updates on completion

- `docs/milestones.md`; this note (as-implemented + deviations).
- `docs/technical_design.md` — animation/draw-layer architecture.
- `docs/asset_pipeline.md` — animation metadata authoring.
- `docs/art_bible.md` — any refinements discovered in production.
- `docs/manual_test_matrix.md` — theme/readability rows.
- Completion report per `docs/milestone_completion_template.md`.

## N. As-implemented record (2026-07-19)

**Delivered.**

- **Manifest v2** (`src/assets/AssetManifest`): `"animation"` entries per
  the owner-approved schema; loader accepts v1 and v2; malformed metadata
  and dangling texture references are dropped with readable errors
  (`dropDanglingAnimations`, re-run after file existence checks so a
  dropped strip texture orphans its animations safely).
- **Pure playback** (`src/render/Animation.hpp`): `frameAt` (loop wraps,
  non-loop holds, degenerate metadata → frame 0) and `frameRect` (strip
  indexing, clamped). **Anchored drawing** (`src/render/SpriteDraw`):
  animation/texture drawn centered on the collision-rect center with pixel
  snapping — visual bounds independent of collision (anchor rule: center,
  a deliberate change from the bible's earlier bottom-center note; §8
  updated).
- **Art** (deterministic generator additions; 15 new PNGs, all pre-existing
  PNGs byte-identical): player 36×48 walk sheet (4 facings × 3 frames,
  frame 0 = stand), overworld enemy silhouettes (plain / horned / crowned —
  shape encodes tier), Crystal Mine and Hollow Forest full tile sets
  (floor/wall/door/accent), Ruined Keep rubble accent, town flower accent,
  2-frame facing-brackets strip. Manifest: 45 file entries + 5 animations;
  credits updated.
- **Integration:** Town and Dungeon states play the directional walk cycle
  (walk clock resets to the stand frame when stopped; fallback static
  sprite, then rectangle); dungeon team markers select tier silhouettes
  (danger::Tier Boss → crowned, Dangerous/Deadly → horned, else plain) with
  M15 props then glyph rectangles as fallbacks; deterministic sparse floor
  accents (hash of tile, room, seed — presentation only); pulsing gold
  facing-brackets on the faced interactable or occupied chest; explicit
  draw order tiles → markers → player → overlays (danger labels, brackets)
  → HUD.

**Automated evidence:** 201/201 tests (8 new): manifest v2 parse/validate/
drop rules, v1 compatibility, playback loop/hold/clamp semantics, frame-rect
indexing, and a shipped-manifest check that reads PNG headers to prove every
animation fits its strip texture. Build clean, zero warnings.

**In-game evidence** (`docs/screenshots/m17_explore/`): town standing +
mid-step walk frame (01/02); Ruined Keep entry + walk frame (03/04);
Crystal Mine entry with braced walls, timber doors, crystal-cluster accents
(05) and a second mine room (06); Hollow Forest entry with trunk walls,
root archways, shrine accents (07). All three themes visibly distinct by
composition; clean exit code 0.

**Deviations / notes:**
1. Blind scripted walks did not reach a gate block, guarded chest, or the
   boss arena this run (seeds rolled unhelpful layouts), so the tier
   silhouettes, facing brackets, and crowned boss marker were not captured
   in-game. Their draw path is the same verified marker/animation code, and
   the matrix M17 note asks the owner to verify them by eye.
2. Anchor rule changed from the bible's forward-looking "bottom-center" to
   center-anchoring (recorded in §8) — better fit for 12×12 actors on 16px
   tiles in pure top-down.
3. Town flower accents land only where Grass tiles exist (the tree border
   ring), so they are subtle; deliberate, not a bug.
4. No glow/particle effects beyond the luminous crystal pixels and the
   pulsing brackets — restraint per §D; more would risk the density rule.
