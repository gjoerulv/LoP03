# M82 — Floors: 1-or-4-floor dungeons & the split scoreboard

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Re-audit this note against the
then-current checkout before implementation begins.

## A. Status

☐ planned

## B. Goal (owner brief)

At the Guild the player chooses a 1-floor or 4-floor dungeon. A 4-floor run
descends through four full levels — floors 1–3 end in an elite gate
guarding the stairs, the real boss waits on floor 4 — and the scoreboard
separates the two run shapes.

## C. Scope (planned)

### Generation (the program's only generation bump, 14 → 15)

- Guild pre-run picker gains **Floors: 1 / 4** beside theme/depth/seed.
- Each floor is a **standard generated level** built from a derived
  sub-seed (pure hash of run seed + floor index + generation version — the
  established discipline; no extra draws from the topology RNG).
- Floors 1–3: the boss-room slot holds an **elite stair-gate team** (drawn
  from the elite/composition machinery at the run's scale) guarding the
  stairs down; defeating it opens the descent. Floor 4 holds the real boss.
- **Flat depth** (owner decision 2026-08-05): all floors generate at the
  chosen depth; the elite gates and 4× length are the added challenge.
- 1-floor runs must stay **byte-identical to v14 output** for the same
  seed wherever feasible; if the picker's existence forces a divergence it
  is the bump's documented reason (the bump tags comparability regardless).
- Run flow: descending is one continuous run (turns, danger, chests, flags
  accumulate); retreat/defeat semantics unchanged (incomplete run scores
  0); autosave once at entry as today; HUD/minimap show the current floor.

### Scoreboard

- `ScoreEntry` gains optional **`floors`** (default 1 for every legacy
  entry). Ranking logic untouched.
- The scoreboard screen shows **separate 1-floor and 4-floor boards**,
  cycled with the M79 `CyclePrev`/`CycleNext` actions (hint in the footer).

## D. Schema, save & version implications

- `dungeon::kGenerationVersion` **14 → 15**.
- Scoreboard schema: optional `floors` field (defensive default 1).
- Save: no new persistent fields expected (runs are not saved mid-dungeon;
  the floor lives in run state). If anything must persist it lands as an
  optional field and is recorded here.

## E. Out of scope

Map-piece completion rolls (M83), Guild Boss unlocks (M84) — both key off
"completed a 4-floor dungeon" and land on this foundation.

## F. Dependencies

M79 (the cycle actions the scoreboard reuses).

## G. Acceptance criteria

- Same seed + version ⇒ identical 4-floor dungeon, every floor; mass
  generation keeps every invariant per floor (connectivity, ≥3 gates,
  boss/stair placement, guarded chest).
- A 4-floor clear pays one score entry tagged `floors: 4`; boards filter
  correctly; legacy entries all sit on the 1F board.
- Retreating or dying on any floor scores 0 as today.

## H. Automated validation

Mass generation tests over both floor counts (invariants + determinism);
sub-seed independence (editing floor 3 content cannot move floor 1);
score tagging/filter tests; scoreboard capture. Full suite green.

## I. Owner manual validation

One full 4-floor run: pacing across four floors (is 4× a slog at this
depth?), stair-gate readability, floor indicator clarity, scoreboard
cycling; then a 1-floor run to confirm nothing changed.
