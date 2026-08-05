# M65 — The town puzzle map (a HoMM2 homage)

Fourth milestone of the M62–M66 program (plan approved 2026-07-27; owner
Q&A: repeatable cycles, class-agnostic exclusive scrolls). Implemented
2026-07-27.

## A. Status

**☑ complete (approved by the owner 2026-08-05)** — implemented 2026-07-27. Evidence in §E.

## B. Goal (owner brief)

A non-event dungeon room may, with low probability, hold a **Secret Map
Piece**. Four pieces reveal a treasure in a town — the town of the LAST
piece — shown on a puzzle map (Heroes of Might and Magic 2 inspiration)
viewable from the town pause menu. The treasure is usually an
immediately-learned exclusive skill scroll, guarded by a generated boss at
the level of the town + depth where the final piece was found.

## C. As implemented

- **Generation v12**: ~**10 %** of dungeons hide one piece in a plain
  Normal room, decided by a **pure hash of the dungeon seed** (its own
  salts, never the generator Rng — every other roll of a seed is
  byte-identical to v11). `Dungeon.mapPieceRoom`; the piece lies on the
  room's walkable center tile as a stand-on gold glyph marker (the
  M55-rite glyph precedent, no new asset).
- **Pickup**: a footer prompt states the count; the FOURTH piece completes
  the puzzle on the spot — the reveal records **this run's town**, a
  **seeded dungeon-roster guard** (`bossRushOrder[hash(seed)]` — the King
  and the Duck keep their own arenas), and **this dungeon's own boss
  scale** (the owner's "same level as the town + depth" rule, read off the
  boss team's `statScalePct`).
- **The Maps screen** (town pause menu → "Maps"): a four-quadrant original
  parchment sketch (coastline, ridge, forest, the dotted path to a bold X)
  filling in HoMM2-style per piece held; the reveal stamps the town and
  names the guardian; a footer counts treasures dug and Lost Scrolls found.
- **The dig** (`kDigTile` (9,10) — walkability pinned by a unit test): an
  X-marked plaza tile in the revealed town; Confirm warns, then fights the
  guard (his authored court, the stored scale, Crystal Shatter, castle
  semantics: no dungeon score, no gold penalty, 1-HP defeat clamp, retry —
  the map keeps marking the spot).
- **The treasure**: the next unawarded of **six exclusive Lost Scrolls**
  (fixed order, drawn without repetition), **learned immediately** by a
  picked member (the M64 scroll rules; a member who knows it refuses and
  another is picked); once the pool is spent, digs pay **+1 legendary token
  and +2500 gold**. The reveal clears; cycles repeat forever.
- **Content**: 6 new class-agnostic skills (Meteor Dive, Chain Lightning,
  Mending Wind, War Drums, Creeping Doom, Vanishing Act — existing schema
  only) + 6 `value: 0` scroll items (the M44 valueless rule keeps them out
  of every shop/chest/drop pool).
- **Save**: optional Party fields — `mapPieces` (clamped 0..3),
  `treasureActive/Town/BossId/ScalePct` (a ghost guard deactivates
  cleanly), `treasureScrollsAwarded` (unknown ids dropped).

## D. Files changed

`dungeon/DungeonModel.hpp`, `dungeon/RoomLayout.hpp` (gen 12),
`dungeon/DungeonGenerator.cpp`, `game/TreasureMap.hpp` (new),
`game/Party.hpp`, `save/SaveSystem.cpp`, `states/DungeonState.{hpp,cpp}`
(MapPiece marker/pickup), `states/TownState.{hpp,cpp}` (dig spot),
`states/MapsState.{hpp,cpp}` (new), `states/TreasureFightState.{hpp,cpp}`
(new), `states/TownMenuState.cpp` ("Maps" row), `capture/CaptureRunner.cpp`
(+`80_puzzle_map`, `81_treasure_dig`), `data/skills.json` (+6),
`data/items.json` (+6), `CMakeLists.txt`, `tests/CMakeLists.txt`,
`tests/test_treasure_map.cpp` (new), count pins (skills 57→63, items
74→80, shipped scrolls 3→9), docs.

## E. Automated validation (2026-07-27)

- `[treasure]` battery: **5 cases / 301 assertions green** — 200-seed
  sweep (piece rate inside a 4–20 % band, Normal-room-only, reload-proof
  determinism), gen ≥ 12 pin, pool order/no-repeat/exhaustion + the
  value≤0 exclusivity of all six Lost Scrolls, seeded guard ∈ the dungeon
  roster, **dig-tile walkability** (and clear of every building door), and
  the save round-trip incl. the ghost-guard deactivation.
- Two stale premises the new content exposed, fixed honestly: the M55 test
  pinned `kGenerationVersion == 11` exactly (relaxed to ≥ 11; the exact pin
  now rides the newest bump in the `[treasure]` battery), and the
  black-market invariant "every Legendary-rarity item is sellable gear"
  caught the Lost Scrolls at legendary rarity — the scrolls moved to
  **epic** (their exclusivity comes from `value: 0`, not the label, and the
  legendary-gear semantic is worth keeping meaningful).
- Closing verification: **588/588 Debug and 584/584 Release tests green**
  (the 4-case gap is the debug-only god-mode battery); `--capture` **81/81**
  scenes clean (+`80_puzzle_map`, `81_treasure_dig`; re-run after the
  rarity fix); zero project-code warnings.

## F. Manual owner checklist

1. Run dungeons until a gold "?" scrap appears in a plain room (~1 in 10);
   take it — the counter reads on the prompt and the Maps screen's quadrant
   fills in.
2. Gather four (the debug menu's gold/levels make farming runs quick): the
   fourth announces the reveal in-run; the Maps screen shows the full
   sketch, the town, and the guardian's name.
3. In that town: the X-marked **Dig Site** on the plaza; Confirm warns then
   fights the guard (his court, that dungeon's scale, Crystal Shatter).
   Lose once — 1-HP clamp, the map still marks the spot; win — pick a
   member to learn the Lost Scroll ON THE SPOT (a knowing member refuses).
4. Dig all six cycles if you have the patience: the seventh pays the token
   + gold fallback. Save/reload at every stage — pieces, reveal, and
   scroll tally all persist.
5. Sanity: an old save opens with nothing found; the piece never appears
   in event/treasure/boss rooms; scores are untouched by the whole system.

## G. Known limitations

- Re-entering the SAME seed can re-collect that seed's piece (the
  rites/trap-chests precedent — the stakes penalty is the designed
  deterrent against repeat-run farming). Flagged for the owner; a
  recent-seeds ledger would be a new decision.
- The four quadrants always reveal in a fixed order (TL→TR→BL→BR) rather
  than a per-piece position — a presentation simplification.
- The dig spot is the same plaza tile in every town.

## H. Final status

`complete (approved 2026-08-05)`
