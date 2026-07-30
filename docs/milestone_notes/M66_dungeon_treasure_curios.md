# M66 — The dungeon treasure map + curios

Final milestone of the M62–M66 program (plan approved 2026-07-27; owner
Q&A: 12 curios, 4 per theme). Implemented 2026-07-27.

## A. Status

**◑ implemented, awaiting manual approval** — set 2026-07-27. Evidence in §E.

## B. Goal (owner brief)

Like the town treasure map but single-use: a random room may, with low
probability, hold a map showing a treasure **inside the current dungeon**,
revealed immediately, visible from the dungeon pause context. The treasures
pay collectables with an achievement for the full set; once complete, they
pay a legendary token instead.

## C. As implemented

- **Generation v13**: ~**12 %** of dungeons carry a **chart room** and a
  distinct **buried room** (both plain Normal rooms, never the M65
  map-piece room — the three stand-on markers share the center-tile
  contract), decided by the same pure-seed-hash discipline (no Rng draw;
  everything else in a seed is byte-identical to v12).
- **Live flow**: reading the cyan chart (stand-on + Confirm) immediately
  reveals the treasure — a gold **X burns on the minimap** over the buried
  room, a gold **"Treasure!" chip** rides the HUD, and the buried room
  shows a red X marker. Digging pays **1 of 12 curios**; live-run only
  (dungeon runs never persist mid-run), so an unclaimed X is simply lost
  with the run.
- **Curios** (`game/Curios.hpp`, a constexpr table — the kAchievements
  pattern, no editor churn): twelve original trinkets, four per theme, each
  with a dry one-line description. The award is a seeded draw among the
  UNOWNED — the current theme's four first, any unowned otherwise —
  deterministic per (owned, theme, seed) so a reload cannot re-fish. The
  **Maps & Treasures** screen gains the collection grid (owned names,
  masked "? ? ?" otherwise).
- **Curator** — the 18th achievement, at the full dozen (toasted in-run);
  afterwards buried treasures pay **+1 legendary token**.
- **Save**: `Party.ownedCurios` (optional; unknown ids dropped, deduped).

## D. Files changed

`dungeon/DungeonModel.hpp` (+chartRoom/buriedRoom),
`dungeon/RoomLayout.hpp` (gen 13), `dungeon/DungeonGenerator.cpp`,
`game/Curios.hpp` (new), `game/Party.hpp`, `save/SaveSystem.cpp`,
`game/Achievements.hpp/.cpp` (Curator), `states/DungeonState.{hpp,cpp}`
(Chart/Buried markers, readChart/digBuried, the minimap X, the HUD chip),
`states/MapsState.cpp` (collection grid), `capture/CaptureRunner.cpp`
(+`82_curio_collection`), `tests/CMakeLists.txt`, `tests/test_curios.cpp`
(new), `tests/test_treasure_map.cpp` (exact gen pin moved here), docs.

## E. Automated validation (2026-07-27)

- `[curio]` battery: **5 cases / 774 assertions green** — 200-seed
  chart/buried sweep (determinism, pair-integrity, distinctness from each
  other and the map-piece room, Normal-only, a 5–22 % rate band), gen == 13
  exact pin, the 12/4-per-theme table shape, the seeded
  theme-first/no-repeat/run-dry draw, the Curator predicate + 18-entry
  roster, and the save round-trip with unknown-id drops.
- The capture lint earned its keep twice in one pass: the first curio names
  overflowed the four-column grid (shortened to ≤ ~13 chars — the
  descriptions keep the flavor), and scene `82_curio_collection` leaked its
  party state into `80_puzzle_map` (scenes share the capture AppContext;
  scene 80 now clears the reveal/curios it does not own). Both fixed;
  capture re-ran **82/82 clean**.
- Closing verification: **593/593 Debug and 589/589 Release tests green**
  (the Release run repeated on the rebuilt binary after a transient
  file-lock build failure; the 4-case gap is the debug-only god-mode
  battery); `--capture` **82/82** scenes clean (+`82_curio_collection`);
  zero project-code warnings.

## F. Manual owner checklist

1. Run dungeons until a cyan **M** scrap appears (~1 in 8); read it — the
   message, the minimap's gold X, and the "Treasure!" chip all point at the
   buried room; the red X waits there; dig it up.
2. The curio lands in **Pause → Maps** (town): the grid names it; its
   theme matches the dungeon's (until that theme's four are owned).
3. Retreat with an unclaimed X — it is gone with the run (by design; the
   maps are single-use).
4. Collect all twelve (debug tools help): **Curator** toasts; the next
   buried treasure pays +1 legendary token.
5. Save/reload — curios persist; an old save shows an empty grid.
6. Screen fit: the Maps screen now stacks parchment + status + curio grid —
   capture `82_curio_collection` guards overflow; judge the density.

## G. Known limitations

- An unclaimed revealed treasure dies with the run (single-use by the
  owner's brief; dungeon runs are never persisted mid-run).
- Curios are pure collection flavor — no gameplay effect beyond the
  achievement and the token faucet; deliberate.
- Same-seed re-entry can re-find a chart, but a complete collection only
  ever pays the token, so nothing compounds.

## H. Final status

`implemented, awaiting manual approval`
