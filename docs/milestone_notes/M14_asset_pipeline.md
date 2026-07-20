# M14 — Asset Manifest and Replaceable Resources

## A. Status and authority

- **Status:** complete (approved by the owner on 2026-07-19; implemented
  2026-07-19; see section N). **Owner-approved decisions (2026-07-19):** manifest schema
  v1 = versioned wrapper + flat `assets` array with dotted logical IDs and
  per-type metadata; audio fallback tier = synthesized tones, then silence;
  reload model = re-fetch by ID, callers cache nothing, ResourceManager
  rebuilds its cache on reload.
- **Last reviewed repository commit:** `67689b4` (2026-07-19).
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M14 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Planning fidelity:** architectural. The manifest schema and API shape below
  are proposals; they must be refined against M11's `docs/asset_pipeline.md`
  findings and approved by the owner before freezing. This note must be
  re-audited and refreshed against the then-current repository before
  implementation begins.

## B. Problem statement

At the reviewed commit, presentation identity lives in C++:

- `ResourceManager` requires callers to supply both a cache key **and** a file
  path (`texture(key, assetPath)`), so asset selection is scattered through
  states;
- `assets/` contains only `.gitkeep`; every visual is code-generated;
- `AudioManager` synthesizes all SFX/music at runtime; there is no file-backed
  audio path;
- replacing any texture, font, or sound therefore requires a C++ edit —
  violating the CLAUDE.md completion target that assets be selected through
  stable logical IDs and external presentation data.

M15 (art slice), M17 (exploration art), and M21 (final audio) all depend on
this milestone's contract being stable first.

## C. Goals

- A versioned external asset manifest maps logical IDs (e.g.
  `ui.frame.default`, `actor.knight.overworld`, `music.town.day`,
  `sfx.ui.confirm`) to files and metadata.
- States request logical IDs only; no presentation file path remains in any
  state.
- Replacing a texture, font, music track, ambience loop, or SFX for an
  existing role requires **no C++ edit**.
- Missing/invalid assets produce clear development diagnostics and safe
  fallbacks (generated placeholder texture, default font, synthesized audio or
  silence) — never a crash, never an arbitrary path read.
- Manifest validation is raylib-free and headless-testable.

## D. Non-goals

- No final asset production (M15/M17/M21) — placeholders remain the shipped
  visuals/audio after this milestone.
- No art direction decisions.
- No hot-reload file watcher — a manual development "reload assets" command is
  the target.
- No theme/pack override system beyond what the schema reserves room for.

## E. Dependencies

- M11 `docs/asset_pipeline.md` — current-state record and open questions.
- **Owner approval of the version-1 manifest schema before it freezes**
  (public-schema rule in `CLAUDE.md`).
- M12/M13 are sequenced earlier but are not hard technical prerequisites;
  confirm ordering at refresh time.

## F. Proposed slices (provisional — refine before starting)

1. **Manifest schema + loader/validator** — versioned
   `assets/manifest.json`; raylib-free parser reusing the
   `ObjectReader`/`LoadReport` validation pattern; checks for duplicate IDs,
   missing files, unsafe paths (`paths::sanitizeRelative`), bad metadata;
   tests over good/malformed manifests.
2. **ResourceManager evolution** — logical-ID lookup replacing key+path
   callers; cache and RAII ownership retained; generated
   placeholder/default-font fallback preserved; migrate existing call sites.
3. **File-backed AudioManager** — load music (streamed) and SFX from manifest
   entries with loop/volume metadata; retain the synthesized generator as the
   documented fallback tier (file → synthesized → silence); no stacked tracks
   across transitions.
4. **Development reload + packaging** — manual reload command (debug builds);
   CMake copies `assets/` next to the executable like `data/`; attribution/
   license record file (e.g. `assets/credits.md`) required for any external
   asset.

Proposed directory shape (from the roadmap; confirm at refresh):

```text
assets/
  manifest.json
  credits.md
  fonts/
  textures/{actors,enemies,environments,effects,ui}/
  audio/{music,ambience,sfx}/
```

## G. Expected systems affected

- `src/resource/ResourceManager.*` — API change to logical IDs.
- `src/audio/AudioManager.*` — file-backed loading + fallback tiers.
- New manifest module (likely under `src/content/` or a sibling
  `src/assets/`; decide at implementation).
- `src/states/` — call-site migration off literal paths/keys.
- `CMakeLists.txt` — `assets/` copy step.
- `tests/` — manifest validation tests.
- `docs/asset_pipeline.md` — becomes the authoritative pipeline doc.

## H. Data, schema, and save compatibility

- **New public schema:** `assets/manifest.json` version 1 — owner approval
  required before freeze.
- Gameplay content under `data/`: **no impact** (presentation data stays
  separate per the roadmap guardrail).
- Saves, settings, deterministic seeds, scoring: **no impact**.

## I. Automated validation

- Manifest validator tests: duplicates, missing required IDs, unsafe paths,
  wrong types, unknown asset kinds, missing files.
- Fallback tests where headless-testable (policy logic; GPU-dependent loading
  stays runtime-validated).
- Full suite green; Debug and Release builds.

## J. Manual validation for the owner

- Swap one placeholder texture and one SFX purely by editing
  `assets/manifest.json` + dropping files; confirm the change appears with no
  C++ edit or rebuild beyond asset copy.
- Delete an optional asset file; confirm visible/silent fallback and a clear
  log line, no crash.
- Corrupt the manifest; confirm the game starts with placeholders and a
  readable error report.
- Confirm music transitions (title→town→dungeon→battle) never stack tracks.

## K. Acceptance criteria

- Changing town music, a player/enemy/environment texture, or a UI font
  requires no C++ edit.
- Duplicate IDs, missing required IDs, unsafe paths, and bad metadata are
  clear validation errors.
- Missing optional assets do not crash; fallbacks render/sound as documented.
- No state contains a presentation file path.
- Manifest version-1 schema documented and owner-approved.

## L. Risks and failure modes

- **Schema lock-in** — a rushed schema forces a v2 migration mid-art-
  production; the owner-approval gate and M15's real-world trial are the
  controls.
- **Handle stability** — callers caching raw handles across reloads
  dangle; the reload command must define handle semantics (stable handles or
  re-fetch-by-ID rule).
- **Fallback masking** — silent fallbacks can hide missing assets; the
  development diagnostic must be loud even when the player experience degrades
  gracefully.
- **API-migration churn** — touching every resource call site invites
  unrelated edits; keep the migration mechanical.
- **Audio regression** — replacing the synthesized path can break the
  no-crash-without-device guarantee; keep every call guarded as today.

## M. Required documentation updates on completion

- `docs/milestones.md`; this note (as-implemented + deviations).
- `docs/asset_pipeline.md` — authoritative schema/workflow reference,
  including how to add/replace an asset.
- `docs/technical_design.md` — resource/audio architecture.
- `README.md` — deliverable now includes `assets/` beside `data/`.
- `.claude/skills/crystal-dungeons/SKILL.md` — manifest gotchas.
- Completion report per `docs/milestone_completion_template.md`.

## N. As-implemented record (2026-07-19)

Base commit `67689b4`. All four provisional slices delivered; 181/181 tests
(8 new manifest tests, including validation of the shipped manifest).

- **Manifest** (`src/assets/AssetManifest.*`): schema v1 exactly as
  owner-approved; raylib-free parse/validate on the ObjectReader/LoadReport
  pattern; duplicate/unknown-type/unsafe-path/bad-metadata/missing-file
  entries reported and skipped; missing manifest.json = valid empty catalog.
- **ResourceManager**: logical-ID-only API (`texture(id)`, `font(id)`)
  resolving through the catalog; per-entry filter/size metadata honored;
  placeholder/default-font fallback preserved; `reload()` per the approved
  re-fetch model. The old key+path API is gone (it had no callers).
- **AudioManager**: two tiers per role — manifest file, then synthesized,
  then silence; file music via raylib music streams (new `MusicHandle` RAII
  wrapper) with manifest loop/volume metadata; master volume now uses the
  full range with the synth tier attenuated to preserve loudness;
  `applyManifest` is reload-safe (stops, unloads, re-resolves, restarts the
  current track). Ambience type validates but plays from M21.
- **Wiring**: `assets/` resolves beside the exe like `data/` (CMake copies
  both); manifest loads at startup with a logged summary; debug-only **F5**
  (`ReloadAssets`, fixed binding) re-resolves live.
- **Proof asset**: `assets/audio/sfx/confirm.wav` (original two-note chime,
  generated) ships mapped to `sfx.ui.confirm` with a `credits.md` entry —
  runtime log confirms the file loads and plays in place of the synthesized
  tone with zero C++ involvement.
- **Runtime verification**: startup log shows "Asset manifest: 1 entry" and
  the WAV loading; clean init and exit.

**Deviations:** none of substance from the approved slices. Notes: the
texture/font side has no shipped assets yet (roles arrive with M15/M17 art) —
the mechanism is tested via placeholder fallback and manifest tests; the
attribution policy lives in `assets/credits.md` with M24 as the enforcement
gate; packaging beyond the build-directory copy remains M24.
