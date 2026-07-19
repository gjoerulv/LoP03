# M21 — Final Music, Ambience, and Sound Effects

## A. Status and authority

- **Status:** implemented, awaiting manual approval (see §N).
- **Last reviewed repository commit:** M20 approval HEAD (2026-07-20).
  Re-audit: `AudioManager` (M14) resolves 9 SFX + 3 music roles two-tier
  (manifest file → synthesized tone → silence); 4 audio files ship, all from
  the deterministic in-project generator approved at M15
  (`tools/asset_gen/generate_audio.ps1` + the M14 confirm chime). No ambience
  concept exists; no title/guild/boss/victory/defeat/result music roles; no
  error/step/door/interact/hit-type/status SFX; transitions are hard cuts
  with no rate limiting. The manifest schema (v2) already carries
  sfx/music types with `loop`+`volume` — ambience ships as `ambience.*`
  role ids of the music type, so **no schema revision is required**.
  Volume settings (master/music/SFX) exist since M13. Battle SFX stage
  through `BattleState::pendingSfx_`; `bossBattle_` flag exists.
- **Owner decisions (2026-07-20):**
  1. **Sourcing: in-project generated.** Extend the deterministic generator
     for the full soundscape (~11 music, 4 ambience, 15 SFX); original,
     provenance-complete; any track later replaceable via manifest edit
     only.
  2. **Dungeon music: per-theme tracks** (keep/mine/forest), matching
     per-theme ambience; the shared `music.dungeon` role is retired
     (synth fallback preserved).
  3. **Battle end: victory fanfare + defeat dirge** as short non-looping
     music tracks, plus a calm result-screen track; stinger SFX roles stay
     defined.
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

## N. As-implemented record (2026-07-20)

**Sourcing (owner decision 1).** The full soundscape is produced in-project
by `tools/asset_gen/generate_audio.ps1` — 30 original 22050 Hz PCM16 mono
WAVs, byte-identical on rerun (hash-verified). Music keeps the approved
M15 identity (square lead + triangle bass); ambience is shaped noise
(xorshift → one-pole lowpass → whole-cycle amplitude LFO, seam-consistent)
with deterministic sine chirps/drips; SFX are segment-rendered
sine/square/tri/noise sweeps. `assets/credits.md` records all three
families; the M14 proof chime was regenerated into the SFX family.

**Roles & manager.** New raylib-free `src/audio/AudioRoles.hpp` is the
stable contract: `Sfx` (15), `MusicTrack` (11), `AmbienceTrack` (4), role
ids, the synth-fallback map (new roles fall to the nearest M8 loop),
per-role SFX rate-limit intervals, and the fade curve. `AudioManager`
gains an ambience stream channel (file-or-silence, follows the music
volume slider), a 0.25 s crossfade on music changes (synth tier hard-cuts;
switching back mid-fade restarts clean), one-shot jingle handling
(non-loop stream frees the channel when done), and SFX rate limiting
(first play always free). Manifest schema untouched (v2; `ambience` was
already a first-class type since M14) — **no schema revision**.

**Scene map (owner decisions 2–3).** Title menu gets its own track (was
Town); Guild + return-from-dungeon get the preparation track; the three
themes get distinct music + ambience pairs (`themeMusic`/`themeAmbience`
on `dungeon_.themeId`, keep-pair fallback for unknown ids); boss battles
use `music.boss` (existing `bossBattle_` flag); battle end switches the
music channel to the victory fanfare or defeat dirge (missing jingle file
→ the old stinger SFX, so battle end is never silent); the result screen
plays `music.result` with ambience off.

**New SFX wiring.** Error buzz on every refusal (shrine/merchant cannot
pay, shop/training cannot afford — previously the cancel blip; the equip
shop had no purchase sounds at all and gained confirm/error); footsteps in
town and dungeon cadenced purely by the role's 0.16 s rate limit (no
per-state timers); doors on dungeon room transitions (not the initial
spawn); interact for merchant purchase + omen acceptance; battle hits
typed at the presentation layer (`stageNumbers` codes: physical 2 / magic
4 / status 5 from the resolved `SkillDef`) — the deterministic sim is
untouched, all pre-existing battle tests pass unmodified.

**Evidence.** 235/235 tests (7 new in `tests/test_audio.cpp`): role-table
integrity, shipped-manifest coverage of every role (type, existing file,
loop flags — jingles non-loop, ambience loop, volume ranges), RIFF-header
validation of all 30 files (PCM16/mono/22050, size-consistent), credits
coverage per family, rate-limit policy, fade curve, synth-fallback map.
Smoke runs: normal launch + input + clean close, and the missing-file
drill (build copy of `title.wav` removed → runs, no crash) — both OK.

**Deviations / notes:**
1. Ambience has no synthesized fallback tier (silence + log by design);
   music keeps one via the nearest-loop map.
2. The retired `music.dungeon` role id was replaced by
   `music.dungeon.{keep,mine,forest}`; the old `dungeon.wav` file became
   `dungeon_keep.wav` (same approved melody).
3. Victory/defeat SFX stinger roles remain defined and shipped as files —
   they now serve as the jingle fallback rather than the battle-end
   default.
4. Loop-seam quality relies on per-note decay envelopes (music) and
   whole-cycle LFOs (ambience), not crossfaded seams; the owner listening
   pass (§J) is the acceptance gate for clicks.
