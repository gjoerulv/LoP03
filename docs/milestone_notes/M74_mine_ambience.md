# M74 — Crystal Mine ambience: no birds underground (owner report, 2026-07-31)

> Status: **implemented, awaiting manual approval.**
> Scope: `assets/audio/ambience/mine.wav` and the mine section of
> `tools/asset_gen/generate_audio.ps1`. The other 38 WAVs are verified
> byte-identical.

---

## 1. Implementation summary

- **Milestone:** M74 — Crystal Mine ambience rework
- **Owner report:** the Crystal Mine ambience "still sounds like there are
  birds there". Requested: no birds; instead echoes of falling rocks and
  broken crystals; not intrusive.
- **Root cause — not a wiring bug.** The mine bed contains no bird code and
  never did; the right file plays for the right theme. The problem is that
  `AmbDrip`, the mine's only recurring event, **was synthesised as a bird
  whistle**:

  | | frequency | contour | figure |
  |---|---|---|---|
  | `AmbBird` (town) | 2200 → 2024 Hz | downward glide, harmonic 0.3 | 3 notes, ~110 ms apart |
  | `AmbDrip` (mine) | 2300 → 1500 Hz | downward glide, harmonic 0.4 | 2 notes, 110 ms apart |

  Same band, same downward pitch glide, same multi-note spacing, same added
  harmonic. A short pitched descending chirp around 2 kHz *is* a bird call —
  the label in the source said "water drip", but the waveform said bird. Eight
  of them fired per 12 s loop at amp 0.75 against a ~0.2 drone, so they were
  also the loudest thing in the bed.
- **Fix.** `AmbDrip` is deleted. The mine now uses two new event synths built
  so that they *cannot* read as a call:
  - **`AmbRockfall`** — a scatter of six dark filtered-noise transients
    (`AmbClack`) at irregular spacing, answered ~0.34 s later by a duller,
    quieter cavern repeat. Broadband and **unpitched by construction**.
  - **`AmbShard`** — a crystal facet giving way: an **inharmonic** strike with
    plate/bar partials at 1 : 2.76 : 5.40 : 8.93 (not a harmonic series),
    near-instant attack, **no glide at all**, plus one dimmer echo. Inharmonic
    and glide-free is precisely what separates "struck brittle mineral" from
    "voice".
  - A low **settling rumble** (58→44 Hz, 52→40 Hz) fills the space the eight
    drips used to occupy, so removing them does not leave the bed thin.
- **Non-intrusive by design:** four events across twelve seconds instead of
  eight, at lower level relative to the drone. Measured dynamic ratio dropped
  from 2.9× to 1.2× (see §5).
- **Player-facing change:** the Crystal Mine sounds like an enclosed, dead
  excavation instead of a cave with birds in it.
- **Engineering changes:** three new helpers in the audio generator
  (`AmbClack`, `AmbRockfall`, `AmbShard`); `AmbDrip` removed (it had exactly
  one caller). No shared RNG exists in `generate_audio.ps1` — `AmbNoise` and
  `AmbClack` each take an explicit seed and keep local state — so the mine
  section cannot shift any other file's bytes.

## 2. Files changed

- **Source:** none.
- **Tests:** none. `tests/test_audio.cpp` asserts manifest coverage and a file
  count of 39; both are unchanged (same path, same id, same count).
- **Content/data:** none. `assets/manifest.json` untouched — `ambience.mine`
  keeps its path, `loop: true` and `volume: 0.32`.
- **Assets:** `assets/audio/ambience/mine.wav` regenerated. The other 38 WAVs
  are byte-identical (verified against `git HEAD` by full path, §5).
- **Tools:** `tools/asset_gen/generate_audio.ps1` — mine section rewritten;
  `AmbDrip` removed; `AmbClack`/`AmbRockfall`/`AmbShard` added.
- **Documentation:** `docs/milestones.md`, this note, `docs/art_bible.md` §9,
  `assets/credits.md`.

## 3. Plan deviations

No approved milestone note preceded this one — it is a direct owner report,
implemented as described. One judgement call worth stating: **the water drips
are gone entirely rather than being re-synthesised at a lower pitch.** The
brief asked for rocks and crystals, and the drip was the exact thing that
sounded like a bird, so re-tuning it would have kept the risk for no benefit.
If you want drips back as a third element I can add a proper one — a
broadband transient with a fast upward bloop around 600–900 Hz, which is what
a real drip is, and nothing like the old glide.

## 4. Compatibility

- **Save files / settings / content schemas / deterministic seeds / score
  records:** no impact. Nothing outside the WAV changed.
- **Packaged assets:** no impact on layout or manifest. Same file path, same
  role id, same 12 s loop length, same 22050 Hz PCM16 mono format, same
  normalisation target (peak 0.5). It is a drop-in replacement.

## 5. Automated validation

- **Generator command:** `powershell -File tools/asset_gen/generate_audio.ps1`
- **Test command:**
  `cmd /c "call "...\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --preset debug"`
- **Results:** **607/607 tests passed, 0 failed.**
- **Asset verification (run in this session):**
  - **Only the mine changed:** every one of the 39 WAVs was extracted from
    `git HEAD` **by full relative path** and hash-compared. Exactly one
    differs: `ambience/mine.wav`. (An earlier check keyed by bare filename
    collapsed 39 files to 36 because `victory`/`defeat`/`result` names repeat
    across `music/` and `sfx/`; the full-path re-run is the one to trust.)
  - **Byte-stable:** a second full generator run reproduced all 39 WAVs
    byte-identically.
  - **Measured acoustics** (biquad bandpass, RMS over the whole loop):

    | bed | bird band 1.9 kHz RMS | rumble 150 Hz RMS | dynamic ratio |
    |---|---|---|---|
    | Town — *has* birds | 0.03965 | 0.00420 | 52.1× |
    | Mine — **before** | 0.01962 | 0.03856 | 2.9× |
    | **Mine — after** | **0.00762** | **0.06333** | **1.2×** |
    | Keep — no birds | 0.00677 | 0.13157 | 2.4× |

    Bird-band energy fell **2.6×** and now sits at the Ruined Keep's level —
    the bed nobody has ever described as birdy. Low-end weight rose 1.6×. The
    1.2× dynamic ratio means events now sit just above the bed rather than
    leaping out of it, which is the "not intrusive" requirement.
  - **Loop seam:** the wrap discontinuity is 4.0× the file's typical
    sample-to-sample step, versus 3.9× for the old mine — no regression, no
    click. (A cruder tail-RMS heuristic I tried first flagged the *Keep* as
    loud at the seam; measuring the actual wrap step showed 3.3× and no click,
    so that flag was my heuristic being wrong, not a defect in the Keep.)
- **Warnings:** none. (`vcvars64.bat` prints a benign
  `'vswhere.exe' is not recognized` line in this environment.)
- **Skipped validation and reason:** Release was not rebuilt — no compiled
  code changed. To confirm: `cmake --build --preset release` then
  `ctest --preset release`; expect 603/603 green.

## 6. Manual owner validation

**The only test that matters here is listening.**

1. Play `assets/audio/ambience/mine.wav` on its own, looped, for a minute or
   two. Confirm: no chirping; the rock falls and crystal breaks read as what
   they are; the loop point is inaudible.
2. In game, enter a **Crystal Mine** themed dungeon and stand still. Confirm
   the bed sits under the music without competing with it, and that you stop
   noticing it after a few seconds — that is the intent.
3. **A/B against the neighbours.** Visit a Ruined Keep and a Hollow Forest in
   the same session. The three should still be obviously different places.
   (The Hollow Forest has owl hoots on purpose — those *are* birds, and are
   correct there. Tell me if you want them gone too.)
4. Check the **Ambience volume slider** in Settings across its range; the bed
   should stay pleasant at maximum.
5. If it is still not right, tell me which of these it is: too busy / too
   sparse / too loud / too quiet / wrong character. Each maps to a different
   knob — event count, event amplitude, or the synth design itself.

## 7. Known limitations

- **The Hollow Forest still has owl hoots** (`AmbHoot`, 360/330 Hz). Left
  alone deliberately: they are correct for a forest and the report was
  specifically about the mine. Flagging it because it is the one remaining
  bird-like element in the soundscape.
- **The town's birds are unchanged and intentional.**
- Whether the new bed is *pleasant* over a long dungeon run is exactly the
  kind of thing measurement cannot answer. Owner validation required.
- The rock-fall tap pattern is fixed rather than varied per loop, so a very
  attentive listener will eventually learn its rhythm. This matches how every
  other bed in the game works (all are short fixed loops) and was not worth
  changing here.

## 8. Documentation updated

- `docs/milestones.md` — M74 row and section added.
- `docs/milestone_notes/M74_mine_ambience.md` — this note.
- `docs/art_bible.md` §9 — records the per-place ambience identities and the
  rule that came out of this bug: an event's *synthesis* has to match its
  label, and pitched glides in the 1.5–3 kHz band read as birdcall wherever
  they appear.
- `assets/credits.md` — M74 provenance row for the rebuilt mine bed.
- Checked and intentionally unchanged: `assets/manifest.json`,
  `docs/asset_pipeline.md` (no pipeline change — same file, id and format),
  `docs/game_design.md`, `docs/technical_design.md`.

## 9. Final status

`implemented, awaiting manual approval`
