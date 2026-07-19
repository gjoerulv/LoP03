# M21 — Final Music, Ambience, and Sound Effects

## A. Status and authority

- **Status:** planned
- **Last reviewed repository commit:**
  `a316f244e870718aa27d9995dc871e11572ad429` (2026-07-19).
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M21 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Planning fidelity:** bounded specification. Track lists and sourcing
  decisions depend on the M15-approved direction and the M14 manifest as they
  actually shipped; refresh this note against the then-current repository
  before implementation begins.

## B. Problem statement

All audio is runtime-synthesized placeholder tones (`AudioManager`), by
design "intentionally primitive" since M8. After M14 the file-backed path and
manifest exist, and M15 set the direction with representative tracks — but
the full audio identity (per-scene music, per-theme ambience, complete SFX
families) does not exist. CLAUDE.md's audio requirements (logical IDs,
replaceable without C++, streaming, separate volume controls, silence
degradation, provenance records, no sound-only information) must be satisfied
by real shipped content.

## C. Goals

- Music for at least: title, town, preparation/guild, dungeon (per-theme or an
  owner-approved shared/variant model), normal battle, boss battle, victory,
  defeat, result.
- Ambience for: town, Ruined Keep, Crystal Mine, Hollow Forest.
- Complete SFX families: UI (navigate/confirm/cancel/error), exploration
  (steps/doors/chests/interact), battle (hits by type, heal, status, KO,
  victory/defeat stingers), with variation for repetitive actions where
  useful.
- Delivery entirely through the asset manifest: streamed music (raylib music
  streams), click-free loops and transitions, no stacked tracks across state
  changes, master/music/SFX volumes persisted via M13 settings, no clipping
  at maximum volume, rate-limited rapid menu SFX, safe silence fallback.
- Provenance, license, and attribution records for every shipped asset.

## D. Non-goals

- No gameplay or mechanic changes; no sound-only gameplay information (the
  muted game must stay fully playable).
- No audio middleware dependency (raylib streams suffice; new dependencies
  are owner-gated per CLAUDE.md).
- No re-architecture of `AudioManager` beyond what M14 established.

## E. Dependencies

- M14 manifest + file-backed `AudioManager`.
- M13 persisted volume settings.
- M15 approved audio direction and sample tracks.
- M18 battle presentation (SFX hook points exist).
- **Owner approval required for:** sourcing strategy (commissioned vs
  licensed vs produced), the dungeon-music model (per-theme vs shared/
  variant), and final acceptance of the soundscape.

## F. Proposed slices (provisional — refine before starting)

1. **Music states + transition policy** — scene→track mapping, transition
   behavior (cut/fade), stacking prevention verified.
2. **Music + ambience content** — produced/licensed tracks landed through the
   manifest, loop points verified.
3. **SFX families** — UI, exploration, battle sets with variation and
   rate-limiting.
4. **Mix pass** — relative levels, clipping check, volume-control behavior,
   silence/fallback drill; provenance records completed.

## G. Expected systems affected

- `assets/audio/` + manifest entries + `assets/credits.md` — the bulk of the
  work is content.
- `src/audio/AudioManager` — transition/rate-limit polish only.
- `src/states/` — scene music/SFX hookups where missing.
- `tests/` — manifest audio validation, fallback policy tests.

## H. Data, schema, and save compatibility

- Manifest: audio metadata within the established schema (revisions
  owner-gated).
- Settings: uses existing M13 volume fields.
- Gameplay data, saves, seeds, scoring: **no impact.**

## I. Automated validation

- Manifest validation: every referenced audio ID resolves; loop/volume
  metadata valid; license record present for every shipped file.
- Missing-audio fallback tests (headless policy level).
- Full suite green.

## J. Manual validation for the owner

- Listen through every scene and every transition (title↔town↔guild↔dungeon↔
  battle↔boss↔victory/defeat↔result): no stacking, no clicks at loop seams,
  transitions feel intentional.
- Max-volume clipping check; volume sliders behave and persist.
- Rapid menu scrolling: SFX rate-limiting sounds acceptable.
- Delete an audio file: silence + log, no crash.
- Full run muted: nothing essential is lost.
- Final soundscape acceptance (subjective owner judgment).

## K. Acceptance criteria

- All major scenes have intentional audio; the minimum music/ambience/SFX
  lists above are covered.
- No stacked tracks, clicks, clipping, or crashes in the owner pass.
- Swapping any track or effect requires no C++ edit.
- Volume settings persist and apply consistently.
- Every shipped asset has provenance and license/attribution records.
- The game remains fully understandable muted.

## L. Risks and failure modes

- **Licensing contamination** — one unlicensed file blocks release (M24
  checks again, but the record must be built here, at acquisition).
- **Loop/transition artifacts** — clicks and stacked tracks are the classic
  failures; the owner listening pass is mandatory, and transition logic needs
  the no-stack guarantee kept from M8.
- **Mix imbalance across hardware** — check on more than one output device
  if possible; keep per-asset volume metadata in data, not code.
- **Sourcing-cost overrun** — the M15 estimate is the budget reference;
  escalate if the track list exceeds it.
- **Second-order:** audio pacing interacts with battle speed (M18) — SFX must
  compress acceptably at Fast/near-instant.

## M. Required documentation updates on completion

- `docs/milestones.md`; this note (as-implemented + deviations).
- `docs/asset_pipeline.md` — audio authoring/marking-up workflow.
- `assets/credits.md` — complete attribution.
- `README.md` — known-limitations line about placeholder audio removed.
- Completion report per `docs/milestone_completion_template.md`.
