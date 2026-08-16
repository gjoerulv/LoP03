# M93 — Dungeon dynamics: fog, the Surveyor, patrols & dragonform (generation v17)

**Status:** implemented, awaiting manual approval
**Program:** M88–M97 (authorized 2026-08-14). Generation **16 → 17** (two
new pure-hash event replacements move seeds' event rolls; everything else
byte-identical to v16 — the M55/M76 precedent). No battle-rules bump (the
patrol and dragonform are content/construction; the engine is untouched).

## Scope (plan-review items 2, 3, 4) & the named design amendment

game_design §6's core line **"no random encounters"** is amended BY THE
OWNER (Q&A decisions 5/7): the danger counter's patrol is the one
step-driven fight, always forewarned by a visible HUD countdown. §6 now
records the amendment explicitly (the CLAUDE.md conflict rule: name it,
never drift silently).

## What was built

- **Fog of war** (multi-floor runs only): unvisited rooms absent from the
  minimap, door stubs toward the unknown, the M66 chart's X burns
  through; `floorRevealed_` per floor. 1F maps stay complete (M82 rule).
- **The Surveyor** (new RoomEventKind; ~25 %/floor, multi-floor only,
  placed in `generateFloors` by pure hash): pays **20 gold** (the owner's
  flat number) to chart the current floor; free courtesy when nothing is
  left to chart. Flavor authored ("The Surveyor").
- **The danger counter**: a visible **Patrol N** chip from 100; every
  tile ticks it; at 0 `dungeon::patrolTeam` (the stair-gate recipe —
  real pools, real composer, the run's town/depth scale, seeded from
  (runSeed, patrolIndex)) attacks **immediately**. **XP yes; gold, drops,
  and danger credit no** (`EnemyTeam.patrol` → `teamSpoils` zeroes gold —
  one shared rule); turns count against score; fleeing counts as an
  escape; either way the counter rewinds to 100 and survives floor
  descents. Defeat = the M89 carry-out.
- **Dragonform** (new RoomEventKind; ~8 %/dungeon, any run, pure hash):
  the panel states the whole pact — the NEXT battle fought as Dragons
  (M45 kit at each member's own level, bare-clawed), **flat −100 score**
  per fight ("Dragonform pact" on the result screen;
  `RunSummary.dragonformFights` → `ScoreBreakdown.dragonformPact`). The
  pure `game/Dragonform.hpp` swaps members by PERCENTAGE both ways (KO
  stays KO, survivors never round to 0) around the untouched battle
  engine. The M45 class modifier is not additionally applied (owner
  decision 6).
- **Debug**: "Patrol on next step", "Arm dragonform" (cheat one-shots,
  the InstantClear pattern). **HUD**: Patrol chip + "Dragonform: next
  battle" chip.

## Deliberate design notes

- Fog invites exploring; the counter taxes it — an intentional pairing,
  flagged for manual judgment (matrix 178–179).
- Patrol XP-grinding is possible BY OWNER DECISION (7): it costs time and
  score; gold and score credits stay shielded.
- The counter is runtime-only: the entry autosave restarts a reload at
  100 (consistent with every other run-runtime state).

## Compatibility

- Saves/settings/battle rules: untouched. Old scoreboard entries keep
  their generation versions; new entries tag v17.
- 1F/4F/20F structural generation identical to v16; only event ROLLS
  moved where the two new hashes land (the version bump's whole point).

## Automated validation

- Debug + Release builds clean (VS2022 shell); capture 110/110 clean.
- Full `ctest --preset debug` green after the M93 batch (see the
  completion report; pre-existing pins moved BY DESIGN: the generation
  version 16→17, the 1F event-kind census 8→9 + Dragonform present +
  Surveyor absent, and the M82 standalone-floor comparison now reproduces
  the Surveyor's expected mutation exactly as production applies it).
- New tests: placement determinism + plain-slot-only + never-on-1F
  (Surveyor); patrol composition/reload-honesty/XP-only; dragonform
  percentage round-trip (KO preserved, survivor floor) + the −100 pact
  arithmetic.

## Manual owner checklist

Matrix rows **178–180** (fog+Surveyor; counter pacing judgment; dragonform
fun judgment). Also worth a glance: the guild omen (M84) still upgrades
only plain slots with the two new kinds in rotation.

## Known limitations

- Patrol fights reuse the standard battle presentation (no bespoke
  ambush stinger) — candidate polish if manual play wants it.
- The fog has no "explored %" readout; the Surveyor is the only shortcut.

## Documentation updated

`docs/milestones.md` (M93 row) · `docs/game_design.md` (§6 amendment +
three new paragraphs) · `docs/technical_design.md` (§46) ·
`docs/manual_test_matrix.md` (rows 178–180) · `data/event_flavor.json`.

## Final status

`implemented, awaiting manual approval`
