# M16 — Compact Dungeon-Room System

## A. Status and authority

- **Status:** planned
- **Last reviewed repository commit:**
  `a316f244e870718aa27d9995dc871e11572ad429` (2026-07-19).
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M16 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Planning fidelity:** architectural. Archetype lists and dimension ranges
  are starting points to be validated against M15's compact-room preview and
  refreshed against the then-current repository before implementation begins.

## B. Problem statement

At the reviewed commit, `DungeonState` realizes every room as a fixed 26×15
tilemap at 16-pixel tiles (reusing `town::Tilemap`), so each room fills
essentially the entire 426×240 exploration surface. Rooms are undifferentiated
boxes: no archetypes, no composition, large purposeless traversal. CLAUDE.md's
completion target requires compact, purposeful layouts. Meanwhile, dungeon
**topology** (`dungeon::generate` — 7×7 grid, unique main path, ≥3 gates,
side-room chests, boss reachability invariants) is proven and tested and must
not be destabilized.

## C. Goals

- Topology and room realization become separate systems: the existing graph
  keeps deciding connectivity, gates, branches, boss placement, and rewards; a
  new room-layout layer decides dimensions, door anchors, internal collision,
  markers, prop zones, spawn points, and draw origin.
- Standard rooms no longer fill the exploration screen. Starting ranges
  (validate in playtest): common chambers ≈ 9–15 × 7–11 tiles; corridors
  ≈ 5–9 × 7–11; special/boss rooms larger only when needed (≈ ≤19×13).
- Room archetypes give rooms gameplay purpose. Initial set: Entry,
  Connector/Corridor, Crossroads, Gate Chamber, Encounter Chamber, Treasure
  Alcove, Shrine/Event Chamber, Elite Chamber, Boss Antechamber, Boss Arena.
- Room realization uses **derived room-local seeds** (stable hash of dungeon
  seed + generation/layout version + room index + archetype) — never extra
  draws from the topology RNG, so presentation changes cannot alter graph
  generation.
- Every generated layout is validated (flood fill/BFS): all connected doors
  anchored and reachable from spawn; required markers do not block the only
  path; chests/interactables reachable; spawn valid; boss layouts navigable;
  bounds inside the exploration viewport; paired graph doors consistent.

## D. Non-goals

- No topology changes (path/gate/boss rules stay as designed and tested).
- No final environment art (M17) — placeholder tiles at the new scale are
  fine.
- No room events/modifiers (M20).
- No score/save schema change without explicit owner approval (see H).
- No uncontrolled proceduralism: random dimensions + random props are not room
  design; archetypes and composition rules are required.

## E. Dependencies

- M11 audit (room-scale defects register).
- M15 compact-room preview — validates target scale and art assumptions
  before this system-level work.
- **Owner decisions required:**
  1. whether a `generationVersion` field is added to score entries/saves if
     published seed semantics change (schema decision — approval required;
     existing loading must stay backward compatible);
  2. final archetype list and dimension ranges after the M15 evidence.

## F. Proposed slices (provisional — refine before starting)

1. **Layout model + archetypes (pure)** — data structures for archetype
   parameters and realized layouts; derived-seed function; unit tests.
2. **Layout generation per archetype (pure)** — bounded dimensions, door
   anchors, collision/prop/marker/spawn placement; flood-fill/BFS validator;
   mass-generation tests (thousands of rooms across seeds/depths/themes).
3. **DungeonState integration** — render compact rooms (draw origin/centering
   within the viewport, HUD/minimap relationship intact); movement/door
   transitions on the new bounds; gates/chests/teams placed via markers.
4. **Reproducibility policy** — implement the owner's generation-version
   decision; determinism tests (same seed + version ⇒ identical topology and
   layouts).
5. **Representative human testing** — seed list covering every archetype for
   the owner to walk.

## G. Expected systems affected

- `src/dungeon/` — new layout/archetype module beside `DungeonGenerator`/
  `DungeonModel` (topology functions ideally untouched).
- `src/states/DungeonState.cpp` — room realization, rendering, movement
  integration (largest integration risk; 533 lines today).
- `src/town/Tilemap` — reused as-is if possible; extended only if bounds
  assumptions require it.
- `src/score/`/`src/save/` — only if the generation-version decision adds a
  field (owner-gated).
- `tests/` — layout validation, determinism, mass-generation suites.

## H. Data, schema, and save compatibility

- **Deterministic seeds:** same seed must keep producing the same dungeon
  *within* a generation version. If room realization changes what a published
  seed means, a reviewed generation-version policy is required; score entries
  may gain an optional `generationVersion` field. **This is a public-schema
  change requiring owner approval; old score/save files must still load.**
- Content schemas (`data/*.json`): expected no impact unless archetype
  parameters are externalized into theme data — if so, that is a reviewed,
  versioned content-schema addition (owner-gated).
- Settings: no impact.

## I. Automated validation

- Determinism: same seed + version ⇒ byte-identical layouts; different rooms
  get independent local seeds.
- Mass generation: thousands of rooms across seeds/depths/themes pass every
  validator invariant (connectivity, reachability, spawn validity, bounds).
- Existing topology invariants stay green (≥3 gates, boss only via gates,
  guarded chest presence).
- Score/save backward-compatibility tests if a schema field is added.
- Full suite green.

## J. Manual validation for the owner

- Walk the representative seed list: every archetype, each theme, shallow and
  deep depths, keyboard and gamepad.
- Judge: rooms feel compact and purposeful; entrances/exits readable; no
  disorienting camera/origin behavior; minimap still matches reality; gates,
  chests, and teams clearly placed.
- Confirm one known seed from before the milestone either reproduces (if
  version unchanged) or is correctly labeled under the new generation version.

## K. Acceptance criteria

- Standard rooms no longer cover the full exploration screen.
- At least five archetypes appear across representative runs.
- Thousands of generated rooms pass connectivity/reference invariants.
- Mandatory gate and boss reachability rules remain true and tested.
- Same seed + generation version produces identical topology and layouts.
- Navigation and interactables are immediately legible in owner testing.

## L. Risks and failure modes

- **Topology destabilization** — the highest technical risk; keep topology
  code paths untouched and their tests as the tripwire.
- **Seed-meaning drift** — accidental extra RNG consumption changes published
  seeds silently; the derived-local-seed rule and determinism tests are the
  control.
- **Score-integrity break** — comparing runs across generation versions as if
  equivalent; resolve via the owner's version-tagging decision (feeds M19).
- **Procedural sameness** — archetypes degenerating into random boxes;
  composition requirements (landmark, entrance rhythm, limited empty
  traversal) are acceptance-relevant, human-judged.
- **DungeonState entanglement** — integration may balloon the already-large
  state; extract room-realization logic into the pure layer rather than
  growing the state.
- **Viewport/HUD regressions** — compact rooms change the empty-space
  relationship with the HUD/minimap; include in owner review.

## M. Required documentation updates on completion

- `docs/milestones.md`; this note (as-implemented + deviations).
- `docs/technical_design.md` — topology/layout separation, derived-seed
  policy, generation-version policy.
- `docs/game_design.md` — room-scale/archetype player-facing description.
- Save/score schema docs if a field was added.
- `docs/manual_test_matrix.md` — archetype/seed rows.
- Completion report per `docs/milestone_completion_template.md`.
