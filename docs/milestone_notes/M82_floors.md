# M82 — Floors: 1-or-4-floor dungeons & the split scoreboard

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Implemented 2026-08-06 on the
post-M81 checkout (`f2e32c8`).

## A. Status

**☑ complete (approved)** — implemented 2026-08-06; approved and
committed by the owner 2026-08-06 (`53ab02f`), including the §E
decisions (runSeed identity, per-floor map-piece/chart rolls, single
danger credit for stair gates). Evidence in §F.

## B. Goal (owner brief)

At the Guild the player chooses a 1-floor or 4-floor dungeon. A 4-floor run
descends through four full levels — floors 1–3 end in an elite gate
guarding the stairs, the real boss waits on floor 4 — and the scoreboard
separates the two run shapes.

## C. What was built

### Generation (the program's single bump, 14 → 15)

- **`dungeon::floorSeed(runSeed, i)`** — floor 0 IS the run seed (so a
  1-floor run generates **byte-identically to v14**, proven by test);
  deeper floors derive independent streams through `blackMarketHash`, the
  SplitMix64 finalizer every seeded run-level system already trusts. Pure
  function, no Rng draw anywhere.
- **`dungeon::generateFloors(seed, depth, db, theme, town, floorCount)`**
  — each floor is a **standard generated level** at the SAME depth (owner
  decision: flat). Floors before the last swap the boss-room team for the
  elite **"Stairway Wardens"** gate: a post-pass drawing from a **fresh
  pure-hash Rng** (salted per floor), zero draws from the floor's own
  generation stream — so every floor is otherwise byte-identical to what
  its sub-seed generates standalone (the sub-seed-independence test:
  editing floor 3 content can never move floor 1). The gate is drawn
  all-elite through the shared `makeTeam` composition machinery (elite
  pool substituted for normals), scaled exactly like the boss it
  replaces, with a fixed recognizable name.
- **`Dungeon`** gains `runSeed` (the re-enterable identity — the
  scoreboard, black market, and boss drops all key off it now; identical
  to `seed` on 1-floor runs so v14 behavior is untouched), `floorIndex`,
  `floorCount`, and the live `stairsOpen` flag.
- `kGenerationVersion` **14 → 15** with the history entry; the exact
  test pin (test_danger) moved with it.

### Run flow

- **Guild picker**: a third stepper row **Floors: 1 / "4 (boss below)"**
  (either arrow flips — there are exactly two shapes). Panel grew 18px;
  everything below shifted; the 240px vertical budget holds (banner ends
  220, footer at 224).
- **DungeonState** holds the run's floors (the current floor is moved out
  of the vector; descent is one-way). On a stair floor the boss slot's
  fight is the new `EncounterKind::StairGate`: victory clears the team,
  flips `stairsOpen`, and the **Stairs marker** (glyph "v", the M55-rite
  precedent — no new art) stands where the wardens stood. Confirm on it
  descends: floor swap, layouts re-realized, danger tiers re-snapshotted
  (the M68 rule now applies per floor), fade + door sound, a "Floor 2 of
  4" message. `run_`/`victoryStats_` accumulate across floors — **one
  continuous run**; retreat/defeat semantics and the single entry
  autosave are untouched. The stair-gate marker renders the truth (elite
  silhouette + gate colors, not the boss purple); its prompt is the
  normal team line; the stairway's prompt says where it leads.
- **HUD**: the theme/depth chip reads `Keep  D6  F2/4` on multi-floor
  runs (unchanged on 1-floor). The minimap simply shows the current
  floor.

### Scoreboard

- `ScoreEntry::floors` (optional in the file, default 1 — legacy entries
  sit on the 1-floor board). `entry.seed` records the RUN seed.
- **`score::onFloorsBoard(entry, boardFloors)`** — the pure split rule,
  headless-tested. Ranking WITHIN a board is `ranksAbove`, unchanged.
- **ScoreboardState** shows separate boards cycled with the M79
  `CyclePrev`/`CycleNext` pair: a centered chip names the board
  ("1-Floor Runs" gold / "4-Floor Runs" crystal), the footer hints the
  cycle, per-board ranks, and a 4F-specific empty-board line points at
  the Guild.

## D. Schema, save & version implications

- `dungeon::kGenerationVersion` 14 → 15 (owner-approved, the program's
  single generation bump). 1-floor output byte-identical to v14; the
  bump tags that a seed now also means a 4-floor shape.
- Scoreboard: optional `floors` field, no format bump (the
  M32/M33/M45 optional-field precedent).
- Save schema: **no new fields** — runs are not saved mid-dungeon; the
  floor lives in run state, exactly as the note planned.
- Battle rules untouched (v15).

## E. Deviations & decisions

1. **Run-level seeded systems now read `Dungeon::runSeed`** (score entry,
   black market, boss drops). On 1-floor runs it equals `seed`, so v14
   behavior is bit-identical; on a 4-floor run the alternative (keying
   off floor 4's sub-seed) would have recorded a seed the player never
   entered and cannot re-enter. Deterministic and reload-proof either
   way — this is the honest identity.
2. **M65 map pieces / M66 charts roll per floor** (each floor is a
   standard level, the plan's own words — its sub-seed carries its own
   pure-hash rolls). A 4-floor run therefore has up to 4× the chances of
   in-dungeon pieces/charts; `chartFound_` resets per floor. Flagged for
   your balance judgment; suppressing them on floors 2+ is a two-line
   veto.
3. **The stair-gate pays normal danger credit** (its tier weight, once —
   like a gate, unlike the double-credit EliteChallenge). Four floors
   already pay ~4× total danger; double-crediting the gates on top read
   as inflation.
4. The Stairs marker is a glyph (M55-rite precedent), not new prop art.
5. The Guild "Floors" stepper flips 1↔4 on either arrow (two shapes, not
   a range).

## F. Automated validation (evidence)

- Debug build: **passed** (VS2022 dev shell).
- `[floors]` suite (new, 5 cases, 909 assertions): floor-seed
  derivation; **the v14-identity proof** (1-floor `generateFloors` deep-
  equals plain `generate` over rooms/teams/chests/events across seeds);
  **the swap-isolation proof** (every floor of a 4F run deep-equals its
  sub-seed's standalone generation except exactly the stair floors'
  boss-slot team); stair-team shape (non-boss, all-elite, fixed name,
  boss-equivalent statScale); per-floor topology + realized-layout
  invariants over 8 seeds × both shapes (40 floors); ScoreEntry
  round-trip incl. a legacy file without the field.
- Full Debug suite: **693/693 passed**. Release build + suite:
  **689/689 passed**. Capture sweep: **90/90 scenes clean**, zero
  overflow — `90_dungeon_stairs` referees the F1/4 chip, the stairway
  marker and the "Descend to floor 2 of 4" prompt in one frame;
  `89_scoreboard_4f` referees the 4F board chip, rows, and the Q/E
  footer hint (both visually spot-checked).

## G. Owner manual validation

1. **One full 4-floor run** (the checklist item that matters): pacing —
   is four floors at one depth a slog or a build-up? Stair-gate
   readability (elite silhouette, "Stairway Wardens" prompt), the
   descend moment (prompt, fade, "Floor 2 of 4"), the F#/4 chip, and the
   reckoning at the bottom paying ONE score entry tagged 4F.
2. **A 1-floor run** to confirm nothing changed — same picker default,
   same dungeon for a same seed as before the update.
3. **Scoreboard**: cycle boards with Q/E (and LB/RB on pad); legacy
   entries all on the 1F board; the 4F empty-state line before your
   first 4F clear.
4. **Retreat and defeat** mid-descent: both still score 0 and land in
   town.
5. Balance judgment on §E.2 (per-floor map-piece/chart chances).

## H. Known limitations

- Descent is one-way (no stairs up — deliberate: one continuous run).
- All floors share the run's theme and depth (owner decision: flat).
- The DungeonResultState does not yet show the floor count (the
  scoreboard does); a natural M83/M84 touch-up if wanted.
- Endless Rush and castle challenges are untouched (M84 territory).

## I. Final status

`complete (approved)` — owner approval 2026-08-06, committed as `53ab02f`.
