# M70 — Separate CRT Strength and CRT Curvature

Owner-specified refinement (detailed brief, 2026-07-29) of the M57 advanced
CRT filter: geometry distortion becomes its own persistent control, so
strength 7 keeps its strong CRT texture without the excessive screen curl.
Implemented 2026-07-29.

## A. Status

**☑ complete (approved by the owner 2026-08-05)** — implemented 2026-07-29. Evidence in §E;
the required window-level visual review (§F) is owner work.

## B. As implemented

- **Setting**: `settings::Settings::crtCurvature` (0.0..1.0, default
  **0.3**), shown as `CRT Curvature: < 0..10 >` directly under CRT Strength
  in Settings → Display. Steps of exactly 0.1 with the shared volume-row
  clamp; always visible and adjustable (dormant at strength 0 — hidden
  coupling would confuse). Reset and fresh installs give 3/10.
- **Persistence**: an optional `"crtCurvature"` number in `settings.json` —
  **no `kSettingsVersion` bump**. Absent → 0.3 (the deliberate migration: an
  old strength-7 file loads as Strength 7 / Curvature 3 — texture kept,
  curl relaxed); malformed → reported, 0.3; out-of-range → clamped. The
  M57 `crtIntensity` and legacy M51 `crtEffect` migrations are untouched;
  `crtEffect` is still never serialized.
- **API**: `VirtualScreen::setCrt(float intensity, float curvature)` — ONE
  call, so no frame observes a mismatched pair; both clamped inside.
  Everything else holds: strength 0 = the exact plain `DrawTexturePro`
  path regardless of curvature; lazy compile on first strength > 0 (at
  most once, curvature never triggers it); compile failure degrades to the
  plain blit (readiness now also requires the `crtCurvature` location);
  capture stays pre-shader; the debug overlay stays unfiltered; no
  per-frame allocation or resource churn while sliding either control.
- **Shader** (`CrtShaderSource.hpp`, still ONE pass, still **11 texture
  samples** — 7 beam/chroma + 4 glow; nothing added): new uniform
  `uniform float crtCurvature`. `scanAct/maskAct/beamAct/glowAct/
  chromaAct/grainAct/toneAct` derive from strength exactly as before;
  geometry derives ONLY from curvature via **`curveAct = pow(C, 1.35)`**
  (low-end precision: 2–3 mild glass, 5 clearly curved, 10 ≈ the old
  authored maximum). At curvature 0 the barrel warp and inset are exact
  identity AND the edge mask is forced fully open
  (`mix(1.0, edge, step(0.0001, curveAct))`), so not even the old
  sub-pixel AA sliver survives — a perfect rectangle.
- **Vignette split** (per the brief): a restrained STRENGTH-driven optical
  falloff `1 - dot(vc,vc) * 0.06 * smoothstep(0.35, 1.0, I)` (radial,
  never corner-cutting, no crop) multiplied by a CURVATURE-driven
  darkening `1 - dot(vc,vc) * 0.16 * curveAct`; their product at maximum
  ≈ the old single `0.20 * curveAct` term.
- Scanline phase still follows the (possibly warped) image uv — lines
  curve WITH the glass, magnitudes unchanged; the slot mask stays anchored
  to the flat destination pixels, exactly as M57 designed.

## C. Files changed

`settings/Settings.{hpp,cpp}` (field, `crtCurvatureStep`/
`crtCurvatureFromStep`, parse/serialize), `states/SettingsState.{hpp,cpp}`
(Row::CrtCurvature + label + adjust; the shared panel auto-sizes to 7
rows), `render/VirtualScreen.{hpp,cpp}` (`setCrt`, member, cached uniform
location, upload), `render/CrtShaderSource.hpp` (uniform + decoupled
curves + split vignette), `core/Application.cpp` (the one call site),
`tests/test_settings.cpp` (+5 cases), docs. No gameplay, content, save,
determinism, resolution, art, or dependency changes; no new render pass or
target.

## D. Automated validation (2026-07-29)

- `[settings]`-file battery: **21 cases / 128 assertions green**, including
  the new M70 cases — curvature default-on-absence (with `crtIntensity`
  0.7 present), round-trip, clamps both ways, malformed fallback to 0.3
  with a report, intensity/curvature independence, legacy-`crtEffect`
  isolation, step conversion, and `Settings{}` ⇒ strength 0 / curvature 3.
- `--capture` **83/83 scenes clean** — `60_settings_display` renders the
  new 7-row Display list inside the overflow lint (label unabbreviated).
- **Live shader smoke test** (sandboxed `%APPDATA%` redirect, the owner's
  real settings untouched): with `crtIntensity 0.7 / crtCurvature 0.3`
  the game compiled and linked the reworked shader on real hardware
  (`SHADER: [ID 4] Fragment shader compiled successfully`, `[ID 5]
  Program shader loaded successfully`) with no "CRT shader unavailable"
  fallback — the GLSL is valid and the uniform resolves.
- Closing verification: **606/606 Debug and 602/602 Release tests green**
  (the 4-case gap is the debug-only god-mode battery); `--capture`
  **83/83 scenes clean**; zero project-code warnings.

## E. What automation cannot show (owner visual review, per the brief)

Automated tests prove persistence, migration, layout, and that the shader
compiles and runs — they do NOT prove visual quality. The identity at
curvature 0 is proven at the math level (inset ×1.0, warp +0, mask forced
1.0, curved vignette ×1.0), not by pixel comparison. The brief's §11
review matrix is owner work:

- Combos: 0/0, 7/0, 7/2, 7/3 (**the key target**), 7/5, 7/10, 3/3, 10/3.
- Sizes: 1278×720, 1920×1080 or borderless, 426×240 minimum, one
  free-resized non-integer window.
- Screens: title, Settings → Display, a text-dense shop, Crystal Mine,
  dungeon pause modal, ordinary battle, a boss battle, High Contrast.
- Judge: 7/0 keeps scanlines/mask/glow/chroma/tone with zero bending;
  7/2–3 is the preferred strong-texture-mild-curve look; curvature stays
  centred/symmetric; nothing essential crops at 10; sliders apply
  immediately with no hitch.

## F. Known limitations

- The `pow(C, 1.35)` curve and the 0.06/0.16 vignette weights are the
  recommended starting values from the brief — tune after the manual
  comparison if the qualitative range feels off (each is one constant).
- The strength-driven flat vignette is new at high strengths on flat
  screens (previously flat screens had none, because vignette rode the
  strength-derived curvature); it is deliberately restrained (max 6 % in
  the far corners) per the brief's split.

## G. Final status

`complete (approved 2026-08-05)`
