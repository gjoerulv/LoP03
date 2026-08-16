# M92 — The long descent: 20 floors & the Guild's trove (generation v16)

**Status:** implemented, awaiting manual approval
**Program:** M88–M97 (authorized 2026-08-14). Generation **15 → 16**
(new 20-floor seed output; 1F/4F byte-identical to v15). No battle-rules
or save-schema change.

## Scope (owner item 6 + Q&A decision 1)

A third Floors option (20) alongside 1 and 4; the "normal" skill scrolls
(Fireball, Bulwark, …) finally obtainable: a **scoring, stakes-raising
20-floor clear** offers a pick-one scroll choice.

## What was built

- **Floors {1,4,20}** (Guild): three-way cycle, "20 (boss below)" label.
  `generateFloors` was already count-generic (wardens on every non-final
  floor, the boss on the last) — the 20-shape needed only the new
  invariants test, no generator change. HUD chip reads F n/20 generically.
- **Third scoreboard board**: `onFloorsBoard` splits 1 / 4 / 20 (legacy
  entries stay classic); the M79 cycle pair walks all three; per-board
  chips and empty-board lines.
- **The trove** (`game/ScrollTrove.hpp`, pure + tested): pool = every
  Scroll item except the treasure-dig exclusives; **7 new scroll items**
  authored (bulwark, frost_lance, spark, group_mend, guard_aura,
  battle_cry, piercing_arrow — wrapping existing normal skills, no new
  skills) joining the 3 shipped ones; offers = 3 distinct learnable draws
  by pure hash of the RUN seed (reload-proof); trigger =
  `scrollTroveEarned(floors, raisedStakes, total)` — mirrors the
  stakes-advance rule (score-0 completions earn nothing).
- **`ScrollChoiceState`**: a Reward-framed modal pushed UNDER the result
  screen (the M67 unwind — it surfaces on the way back to town): three
  offers + a real "(Leave it)" row (Cancel deliberately does nothing),
  the highlighted scroll's description below. **The chosen scroll goes to
  the bag** and teaches via the Party panel's M64 flow — a deviation from
  the plan's "immediate learn" sketch, chosen because the owner's item 6
  began from that panel flow; recorded here for the manual pass.
- **Map economy**: the M83 gate already read `floorCount >= 4`, so 20F
  rolls map pieces like the 4F descent (same town ladder — the trove is
  the 20F prize).
- **Debug**: "Grant 1x each skill scroll". **Capture**: `110_scroll_trove`.

## Owner-visible risk (deliberate)

A 20-floor run is one continuous sitting — entry-only autosave, the M82
rule kept by design. If manual play demands it, a suspend save is a
separate future decision (recorded in the program plan).

## Compatibility

- Saves/settings: untouched. Scoreboard: new entries tag generation v16
  and `floors: 20`; old entries keep their boards and versions.
- Deterministic seeds: 1F/4F output byte-identical to v15 (floorSeed
  untouched); the trove offers are run-seed-pure.

## Automated validation

- Debug + Release builds clean (VS2022 shell); capture 110/110 clean.
- Full `ctest --preset debug`: green — see the completion report status
  (run after the M91+M92 batch landed; the only pre-existing pin moved was
  the generation-version test, 15 → 16 by design).
- New tests (`test_floors.cpp`): 20F generation invariants (wardens 1–19,
  boss on 20, determinism, sub-seeds), the three-way board split, trove
  pool hygiene (teachable, never the Lost Scrolls), seeded
  distinct/learnable offers + reload-proofness, and the trigger truth
  table.

## Manual owner checklist

Matrix rows **176–177**: the three-way Floors row and boards; the trove
modal end-to-end (raise stakes → clear 20F → choose → teach via the Party
panel; reload replays the same offers). Judge the single-sitting length.

## Known limitations

- No warden-cadence variety across 19 gates (same all-elite rule every
  floor — the M82 pattern, unchanged); if the descent drags in manual
  play, cadence tuning is a follow-up.
- Offers can repeat across DIFFERENT runs (per-run seeded draws, no
  pity/no-repeat memory) — deliberate simplicity.

## Documentation updated

`docs/milestones.md` (M92 row) · `docs/game_design.md` (§6 three shapes +
the long-descent paragraph) · `docs/technical_design.md` (§45) ·
`docs/manual_test_matrix.md` (rows 176–177).

## Final status

`implemented, awaiting manual approval`
