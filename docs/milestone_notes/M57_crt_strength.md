# M57 — Advanced CRT post-process (0–10 CRT Strength)

> **M70 addendum (2026-07-29):** geometry (barrel warp, pre-warp inset,
> rounded corners, the curved-edge mask) no longer follows strength — it
> moved to its own persistent **CRT Curvature** slider, and the render API
> became `VirtualScreen::setCrt(intensity, curvature)`. See
> `docs/milestone_notes/M70_crt_curvature.md`; this note remains the
> authority for everything strength still drives.

> Owner-directed implementation task, authorized 2026-07-24. Runs after the
> M53–M56 adjustment program and before the M23/M24 release track. **Pure
> presentation** — no gameplay, save, generation, battle-rules, or virtual-
> resolution change, and **no version bump**.
>
> Authority: `CLAUDE.md` > this note > `docs/milestones.md` >
> `docs/game_design.md` > `docs/technical_design.md`. §J is the as-implemented
> record.

## A. Base and baseline

- **Base checkout:** `3181aa8` ("M53-M56"), working tree clean at start; the
  required base commit for the task, verified before any change.
- **Baseline:** 526/526 Debug, 522/522 Release, `--capture` 75/75 clean.
- **Versions (unchanged):** rules 10 / generation 11 / save 1 / **settings 1** /
  achievements 1. `kSettingsVersion` is **not** bumped — the settings field
  change is defensive (old files load).

## B. Goal

Replace the M51 Boolean **CRT Effect** with a persistent **0–10 CRT Strength**
control backed by an advanced single-pass CRT shader that, as strength rises,
progressively resembles a stable ~1985 consumer CRT television — curved glass,
scan structure, a consumer slot mask, mild beam softness, restrained glow,
modest chroma bleed, curved dark corners/vignette, and a little high-strength
signal texture — while staying readable and playable. **Strength 0 must be the
exact existing unfiltered blit.** Presentation only.

## C. What was verified about the pre-M57 code (re-audit vs `3181aa8`)

| Claim | Verified |
|---|---|
| Game renders into a point-filtered 426×240 `RenderTexture`; blit scales into an aspect viewport | ✅ `VirtualScreen` + `computeViewport` |
| The shader wraps only the final `DrawTexturePro` in `blitToWindow` | ✅ `VirtualScreen.cpp` |
| The old shader did alternating-row dim + a monochrome vertical mask, no curvature | ✅ (replaced) |
| Capture exports the **pre-shader** virtual target (`exportImage`) | ✅ untouched — capture stays byte-unaffected |
| Debug overlay is drawn after the blit (unfiltered) | ✅ `Application::processFrame` |
| Shader compile is lazy with a safe fallback | ✅ pattern kept and extended |
| `Settings::crtEffect` optional bool; Settings shows On/Off; `Application` forwards each frame | ✅ all three changed |

## D. The setting (spec §1–2)

- `settings::Settings`: `bool crtEffect` → `float crtIntensity = 0.0f;` (clamped
  0..1). Player-facing control is a **0–10 slider** in Settings → Display, using
  the same Left/Right step (0.1) and clamp as the volume rows; label
  `CRT Strength:  < N >` where `N = crtStrengthStep(intensity)`.
- **Migration (no `kSettingsVersion` bump):** the parser reads an optional
  numeric `crtIntensity`; a valid value wins (clamped 0..1). If absent, the
  legacy `crtEffect` bool migrates — `true → 0.3` (approximately preserves the
  old subtle look, **not** the strongest new sim), `false → 0.0`; absent both →
  `0.0`. A malformed `crtIntensity` is reported and falls through to the
  legacy/default path. Serialization writes **only** `crtIntensity` — the two
  fields can never disagree; the parser alone still understands the old bool.
- Pure helpers `settings::crtStrengthStep(float)→0..10` and
  `crtIntensityFromStep(int)→0..1` (headless-testable) back the slider.

## E. The render boundary (spec §3–4)

- `VirtualScreen::setCrt(bool)` → `setCrtIntensity(float)`; clamps defensively,
  compiles the shader **lazily on the first strength > 0** (at most once), caches
  five uniform locations, and logs once + degrades to the plain blit on failure
  (detected via the missing `crtIntensity` uniform — raylib returns its default
  passthrough shader on a memory-compile failure). While strength is 0 the shader
  is never compiled or bound, so 0 costs only a float compare beyond the plain
  path. RAII ownership, black letterbox bars, the unfiltered debug overlay, and
  pre-shader native capture are all retained.
- The larger shader source lives in **`src/render/CrtShaderSource.hpp`**
  (`cd::kCrtFragmentSource`), included by `VirtualScreen.cpp` — no new asset
  type, no dependency.
- Uniforms supplied per blit: `crtIntensity` (float), `crtSourceRes` (426×240),
  `crtOutputRes` (viewport px), `crtSourceTexel` (1/source), `crtTime`
  (`GetTime()`, used only by the high-strength grain). The scan structure is
  anchored to the 240-line source; the slot mask is anchored to destination
  pixels (`screenUv × crtOutputRes`) and fades via
  `smoothstep(1.5, 3.5, outputW/sourceW)` so it can't shimmer/moiré or dominate
  at small scales. The render texture's wrap is set to `CLAMP` so edge taps can't
  wrap.

## F. The shader (spec §5–6)

One fragment pass, **11 texture samples**, no loops, no intermediate render
target. Components, each on its own non-linear activation of intensity `I`:

| Component | Activation (of `I`) | Notes |
|---|---|---|
| A. Barrel curvature | `smoothstep(0.20,0.75,I)` | slight pre-inset so nothing crops; identity at `I≈0`; corners → black |
| B. Rounded edge + vignette | same curve | soft `smoothstep` glass edge (black outside); gentle radial darkening, no fake bezel |
| C. Scanlines | `0.55·pow(I,0.8)` (immediate) | source-anchored `sin` beam, dark **gaps** between rows, never dead rows |
| D. Slot mask | `smoothstep(0.20,1,I)` × fade | destination-pixel RGB triad with a 3-row slot stagger; depth ≤ 0.16; fades at small scale |
| E. Beam spread | `smoothstep(0.25,1,I)` | 3 horizontal green taps; vertical detail untouched (font stays legible) |
| F. Glow/halation | `smoothstep(0.30,1,I)` | 4 neighbour taps, `smoothstep(0.55,1,luma)` threshold, added after darkening |
| G. Chroma convergence | `smoothstep(0.40,1,I)` | R left / B right sub-texel displacement, slightly more toward edges |
| H. Tonal response | `smoothstep(0.15,1,I)` | modest saturation lift + gentle `pow` contrast; deep blacks kept |
| I. Grain | `smoothstep(0.70,1,I)` | fine deterministic luminance grain (hash of coord+time), ≤ 0.03; no blocks/roll/flicker |

Continuum intent (met in review): `0` off · `~0.3` ≈ the old subtle CRT ·
`0.5–0.6` convincing clean CRT · `0.8` strong 1985 set · `1.0` maximum authored,
still readable. No individual sub-effect parameters are exposed in Settings.

## G. Tests (spec §8)

`tests/test_settings.cpp` — the single M51 `crtEffect` round-trip case is
replaced by five pure cases (net +4): float round-trip + default-0-when-absent;
clamp below 0 / above 1; legacy `false→0.0` and `true→0.3`; new numeric
precedence over a legacy bool; malformed numeric reported + safe default. Plus a
`crtStrengthStep`/`crtIntensityFromStep` conversion case (incl. clamping and the
reset default). All prior tests preserved. Headless tests do **not** assert CRT
visual quality.

## H. Manual validation (spec §9) — owner sign-off item

The capture command still exports the **pre-shader** native target and therefore
cannot show the filter (preserved). Window-level visual feel is an owner
sign-off. This session verified, via an **offline preview harness** (links the
built raylib, `#include`s the real shader, renders it over real captured scenes —
deterministic, no window automation, no live-settings tampering) that:

- the GLSL **compiles** on the owner's GPU (uniform resolves; no fallback);
- strength **0** is the exact plain image; **3** ≈ the old subtle CRT with crisp
  text; **5–6** reads as a convincing, symmetric, readable curved CRT with deep
  blacks preserved; **7** strong and readable; **10** heavy but still legible;
- the slot mask **fades to nothing at 426×240 (1×)** and **resolves cleanly at
  1920×1080** without harsh moiré;
- letterbox/pillarbox stays pure black outside the curved glass.

Observed trade-offs (honest): at **strength 10** chroma fringing on text is
pronounced (readable, but approaching a "3D-glasses" edge — the intended heavy
extreme); at the **minimum 426×240 window** high strengths are heavy because
curvature compresses the tiny image; the glow/tonal pass gently **warms existing
bright source content** (e.g. the shop's decorative lower-right orb) — that orb
is in the source art, not a shader artifact.

## I. Documents updated

`docs/game_design.md` (CRT Strength wording), `docs/technical_design.md` (the
0..1 intensity, the advanced shader, uniforms, migration, post-blit isolation),
`docs/manual_test_matrix.md` (row 130 reworked + M57 strength/resolution cases),
`README.md` (Settings mention), `docs/milestones.md`, this note.

## J. As-implemented record

Implemented 2026-07-24 on base `3181aa8`. **Pure presentation, no version bump**
(rules 10 / generation 11 / save 1 / settings 1 / achievements 1).

**Files changed**
- **Source:** `src/settings/Settings.hpp/.cpp` (field + migration + serialize +
  two pure step helpers); `src/render/VirtualScreen.hpp/.cpp` (intensity API,
  lazy compile, cached uniforms, wrap-clamp); **new**
  `src/render/CrtShaderSource.hpp` (the GLSL); `src/core/Application.cpp` (forward
  `crtIntensity`); `src/states/SettingsState.hpp/.cpp` (0–10 slider row).
- **Tests:** `tests/test_settings.cpp` (M51 CRT case → five M57 cases + a step
  case).
- **Docs:** this note + the five listed above.
- **No** CMake change (the new file is a header included by an existing TU); **no**
  capture-scene change (the effect is not capturable; the Display submenu text
  updated in the existing `60_settings_display` scene).

**Validation (this session, VS2022 dev shell)**
- Build debug + release — OK, **no project warnings**.
- `ctest --preset debug` → **530/530** (526 + 4 net). `ctest --preset release` →
  **526/526** (522 + 4). Debug > Release by 4 is the pre-existing god-mode gating,
  unrelated to M57.
- `CrystalDungeons.exe --capture` → **75/75 clean**; the Display submenu reads
  `CRT Strength:  < 0 >` with no `[ui-overflow]`.
- Offline harness: shader compiles (`crtIntensity` uniform resolves); previews at
  0/3/5/6/7/10 across 426×240 / 1278×720 / 1920×1080 reviewed (see §H).

**Deviations from the plan:** none material. Implemented exactly as specified;
the one implementation choice was moving the shader string into a dedicated
header (which §3 explicitly permits) and adding a `TEXTURE_WRAP_CLAMP` on the
render target so edge taps can't wrap (a safe, in-scope robustness step).

**Known limitations / owner-validation needs**
- Window-level visual feel, and whether strengths read as intended in live play
  (not just in the deterministic harness), are owner sign-off items.
- Strength 10 is deliberately heavy (usable, not subtle); the minimum-window +
  high-strength combination is heavy by nature.
