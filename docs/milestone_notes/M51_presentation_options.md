# M51 — Presentation & options

## A. Status and authority

- **Status:** ☑ complete (approved) — approved by the owner 2026-07-23 after
  manual testing ("M47–M51 is done"), committed as `64d220e`; the audit that
  accompanied the approval re-verified 485/485 tests (Debug **and** Release)
  and `--capture` 61/61 at that commit. Authored
  just-in-time on 2026-07-23 from the
  owner-approved M47–M51 "Court & Comfort" plan. **Final milestone** of that
  program; after it, M23 → M24 remain (deferred). See §J for the as-implemented
  record.
- **Authority:** `CLAUDE.md` → this note → `docs/milestones.md` →
  `docs/game_design.md` → `docs/technical_design.md`.
- **Base checkout:** `4640c50` ("M50"), working tree clean at authorization.
- **Baseline:** 479/479 tests, `--capture` 59/59 scenes overflow-clean.
- **Versions at baseline:** `battle::kBattleRulesVersion` **9**,
  `dungeon::kGenerationVersion` **10**, `kSaveVersion` **1**,
  `kSettingsVersion` **1**. **No bump anywhere** — the two new settings fields are
  optional bools with defensive parse (old `settings.json` loads unchanged).
- **Terminal state for this session:** `implemented, awaiting manual approval`.

## B. Re-audit against `4640c50`

| Plan claim | Verified? |
|---|---|
| Title tagline is a fixed string in `MainMenuState::render` | ✅ [MainMenuState.cpp:112](../../src/states/MainMenuState.cpp:112): `"an 8-bit-plus dungeon-score roguelite"` — and it **describes the genre**, which the plan forbids, so it must be replaced |
| Capture pins `SetRandomSeed(123456789)` per run | ✅ [CaptureRunner.cpp:205](../../src/capture/CaptureRunner.cpp:205) — a `GetRandomValue` pick at title `onEnter` is capture-stable across reruns |
| `motionPhase()` (2-frame, 2.5 Hz) is the sanctioned motion clock | ✅ [UiDraw.hpp:104](../../src/ui/UiDraw.hpp:104). ⚠️ It is **2-step**; the title pulse wants a **3-step** ramp (textHint → textDim → text), so a small 3-phase clock is added beside it. |
| `SettingsState` is one flat 15-row list | ✅ [SettingsState.cpp:26–40](../../src/states/SettingsState.cpp:26) — 15 `constexpr` row indices, rebuilt into one `Menu`. The submenu is a `SettingsState`-only restructure. |
| `Settings` is a flat struct, version 1, defensive optional parse | ✅ [Settings.hpp:41](../../src/settings/Settings.hpp:41); `highContrast` is the precedent optional-bool field. `crtEffect`/`backgroundAudio` follow it. |
| `VirtualScreen::blitToWindow` is a single `DrawTexturePro` — the CRT hook | ✅ [VirtualScreen.cpp:29](../../src/render/VirtualScreen.cpp:29) |
| Capture exports the **virtual target**, not the window | ✅ `exportImage` reads `target_.texture` ([VirtualScreen.cpp:32](../../src/render/VirtualScreen.cpp:32)) — the CRT shader wraps only the window blit, so capture is unaffected **by construction** |
| No focus/audio handling exists; `AudioManager::setEnabled(bool)` exists | ✅ [AudioManager.cpp:372](../../src/audio/AudioManager.cpp:372) starts/stops music+ambience; `Application::processFrame` has no focus logic yet |
| Capture already forces audio off | ✅ [CaptureRunner.cpp:236](../../src/capture/CaptureRunner.cpp:236) `audio.setEnabled(false)`. **And capture runs its OWN loop, not `Application::processFrame`** — so the focus-audio change cannot touch capture at all. |
| `BattleSequencer::flashStrength()`/`shakeOffset()` gated by `effectFlash`/`effectShake`; impact beat | ✅ [BattleSequencer.hpp:47](../../src/render/BattleSequencer.hpp:47); `BattleState` reads them at [BattleState.cpp:1091,1229](../../src/states/BattleState.cpp:1091) |
| `BattleState` knows the acting skill's `SkillTarget` at presentation time | ✅ it holds the `SkillDef*` through the execute paths ([BattleState.cpp:610,666](../../src/states/BattleState.cpp:610)) |
| **Plan says AoE targets are `all_enemies/all_allies/everyone`** | ⚠️ **There is no `everyone`.** `SkillTarget` = `{SingleEnemy, AllEnemies, SingleAlly, AllAllies, Self}` ([Enums.hpp:22](../../src/content/Enums.hpp:22)). AoE = `AllEnemies` ∨ `AllAllies` ∨ the Dragon's `attackHitsAll` basic. |

## C. Goals

The five presentation-and-options features that close the program, none of which
touches gameplay, generation, saves' meaning, or determinism:

1. A **randomized comedic title phrase** (must include "Geese and Dragons; Spoons
   and Snacks!"; never a genre description; light 3-step pulse).
2. **Settings submenus** (Audio / Display / Gameplay / Controls / Reset / Back),
   with two new optional bools: `crtEffect` (Off) and `backgroundAudio` (Off).
3. A **CRT shader** (embedded GLSL, applied only at the window blit, graceful
   fallback, no curvature).
4. **Focus-aware audio** (mute when the window is unfocused unless Background
   Audio is on).
5. A faint **AoE screen tint** on all-target actions.

## D. Constraints

- No version bumps. `crtEffect`/`backgroundAudio` are optional bools; an old
  `settings.json` loads with both false (test).
- The CRT shader wraps only `blitToWindow`; capture exports the pre-shader
  virtual target, so `--capture` is unaffected. A compile failure logs once and
  falls back to the plain blit.
- The title phrase is chosen with `GetRandomValue` at `onEnter`, so the pinned
  capture seed keeps `01_title` reproducible. The pulse is presentation-only.
- The AoE tint is drawn during the impact beat, gated by `effectFlash`
  (Off = none, Reduced = half), alpha ≤ ~0.12 × `flashStrength()` — a single
  decay pulse, never a strobe. It reads nothing back into the sim.

## E. Slices

### E1 — Title phrases (feature 6)

A `kTitlePhrases` table of ~12 original dry-comedy one-liners (the plan's list),
**including "Geese and Dragons; Spoons and Snacks!"**, none describing the genre.
`MainMenuState` gains `phrase_`, chosen in `onEnter` via
`GetRandomValue(0, N-1)`. `render` draws `phrase_` where the tagline was, with
the crystal pips flanking (already width-derived, so variable-length phrases
work). Pulse: a **3-step** brightness ramp (`textHint`→`textDim`→`text`) on a
~1.5 Hz clock — a new `motionPhase3()` beside `motionPhase()`. No layout shift.

Lint (`test_ui_kit.cpp` or a small phrase test): every phrase fits the title
width at `kFontBody`, and the mandated line is present.

### E2 — Settings submenus (feature 10)

`Settings` gains `bool crtEffect = false;` and `bool backgroundAudio = false;`,
serialized/parsed as optional (absent = false), beside `highContrast`.

`SettingsState` gains a mode enum:
- **Top level:** Audio / Display / Gameplay / Controls / Reset settings &
  bindings / Back.
- **Audio:** Master / Music / SFX volumes / **Background Audio** / Back.
- **Display:** Window / **CRT Effect** / Battle Flash / Battle Shake /
  High Contrast / Back.
- **Gameplay:** Battle Speed / Message Speed / Tutorial Prompts /
  Reset Tutorial Prompts / Back.
- **Controls:** Remap Keyboard / Remap Gamepad / Back.

Cancel from a submenu returns to the top level; Cancel from the top level saves
and pops (today's behavior). Save-on-exit unchanged. `rebuild()` becomes
mode-aware; the row-index constants are replaced by per-mode row lists. Toggling
CRT applies immediately (Application reads it each frame); Background Audio takes
effect via the focus check (E4).

### E3 — CRT shader (feature 6/3)

A GLSL-330 fragment shader as an embedded C++ string, compiled with
`LoadShaderFromMemory` — **no new asset type, no dependency**. Subtle scanlines +
a faint aperture mask, **no curvature** (pixels stay crisp). Owned by
`VirtualScreen` (it owns the target it samples); `blitToWindow` uses it when
enabled, via `BeginShaderMode`/`EndShaderMode` around the `DrawTexturePro`.
`Application` calls `screen_.setCrt(settings.crtEffect)` each frame (cheap; only
toggles a bool). Compile failure → logged once, `crtEnabled_` forced false, plain
blit. Capture unaffected (exports the target directly). Scanline geometry uses
the render size, passed as a uniform.

### E4 — Focus audio (feature 10/4)

In `Application::processFrame`, once per frame:
`audio_.setEnabled(IsWindowFocused() || settings_.values.backgroundAudio)`.
Default (`backgroundAudio` false) mutes when unfocused — the owner-approved
behavior change. `setEnabled` early-returns when the state is unchanged, so this
is free on steady frames. Capture never runs `processFrame`, and forces audio
off itself, so it is doubly unaffected.

### E5 — AoE screen tint (feature 11)

At action-commit, `BattleState` records whether the committed action is
all-target and its effect flavor:
`enum class AoeTint { None, Heal, Damage, Debuff }` on the state. Set in the
execute paths from the `SkillDef` (`AllEnemies`/`AllAllies` + category/status) or
the Dragon's `attackHitsAll` basic attack (Damage). During the impact beat,
`render` draws one full-screen rect: `alpha = 0.12 × flashStrength() ×
(effectFlash==Reduced ? 0.5 : 1)`, color by flavor (Heal = `success`, Damage =
`danger`, Debuff = `magic`), under the bottom panel. `effectFlash==Off` → no
tint (flashStrength is already 0 there, so this is automatic, but the flavor is
also cleared so nothing lingers). Single decay pulse (flashStrength already
decays through the impact beat). Cleared when the sequence finishes.

## F. Tests

- **Settings round-trip:** `crtEffect`/`backgroundAudio` serialize and parse;
  an old file lacking both loads with both false; a malformed value falls back.
- **Submenu model:** a pure description of the per-mode row lists (each mode's
  rows, the safe/back row) — enough to pin navigation without a raylib state.
- **Phrase lint:** every phrase fits the title width; the mandated line is in the
  table; none is empty.
- **AoE tint gating (pure helper):** `aoeTintAlpha(flavor, flashStrength,
  effectLevel)` returns 0 for `None`/`Off`, half for `Reduced`, the capped value
  otherwise — headless, no raylib.
- **`motionPhase3()`** cycles 0→1→2 deterministically.

## G. Capture

- `01_title` refreshes automatically (pinned seed → a fixed phrase). If the
  seeded pick is dull, adjust nothing — it is deterministic and that is the
  point; the note records which phrase it lands on.
- New scenes: **top-level settings**, the **Display submenu**, and an **AoE tint
  frame** (an all-enemies skill mid-impact) at Full flash. 59 → **~62**.
- CRT is **not** capturable (it lives at the window blit; capture exports the
  virtual target) — stated here so its absence from the capture set is expected.

## H. Documents to update

`docs/game_design.md` (title phrase; the options set; the mute-on-unfocus
default), `docs/technical_design.md` (the shader isolation at the blit, the
optional settings fields, the AoE tint gating, `motionPhase3`),
`docs/manual_test_matrix.md` (new rows: submenu navigation, CRT on/off,
background-audio focus behavior, the AoE tint at each flash level, the title
phrase), `README.md` (Settings/controls mention the submenus + new toggles),
`docs/milestones.md`, this note's §J.

## I. Acceptance criteria

1. The title shows a random original phrase (never the genre), the mandated line
   is in the pool, and it pulses lightly without moving layout; capture `01_title`
   is reproducible under the pinned seed.
2. Settings is organized into Audio / Display / Gameplay / Controls / Reset /
   Back; Cancel steps back one level then saves-and-exits; the two new toggles
   default Off.
3. The CRT effect toggles subtle scanlines with no curvature; a compile failure
   falls back to a plain blit; capture is byte-unaffected.
4. Losing window focus mutes audio unless Background Audio is On.
5. An all-target action tints the screen faintly during impact, gated by the
   flash setting (Off = none, Reduced = half); it never strobes and never affects
   the sim.
6. Settings round-trip with and without the new fields; no version bumps.
7. Full suite green in Debug **and** Release; capture clean.

## J. As-implemented record

All five slices landed. Verified 2026-07-23 on `4640c50`: **485/485 tests**
(479 baseline + 6 new option cases) green in **Debug and Release**, `--capture`
**61/61** scenes overflow-clean (3 added: `60_settings_display`, `61_aoe_tint`,
plus the auto-refreshed `01_title`/`03_settings`), no warnings introduced.

### As built

- **Title phrases:** `states/TitlePhrases.hpp` (12 lines incl. the mandated one);
  `MainMenuState` picks in `onEnter` via `GetRandomValue`, pulses on the new
  `ui::motionPhase3()`. The pinned-seed capture landed on "Do not feed the
  dungeon." — deterministic across reruns.
- **Settings submenus:** `SettingsState` restructured into `Mode` + per-mode
  `Row` lists; Cancel steps out one level. `crtEffect`/`backgroundAudio` added to
  `Settings` with optional parse/serialize.
- **CRT shader:** embedded GLSL-330 in `VirtualScreen`, wrapping only the window
  blit; graceful default-shader fallback; capture reads the pre-shader target.
- **Focus audio:** one line in `Application::processFrame`.
- **AoE tint:** pure `states/AoeTint.hpp` (classify + alpha); `BattleState` sets
  `aoeTint_` in the four execute paths and draws the impact-beat overlay.

### Deviations from this note

1. **The plan's AoE target list named `everyone`, which does not exist** in
   `SkillTarget`. Implemented over `AllEnemies`/`AllAllies` + the Dragon's
   `attackHitsAll` basic, as the re-audit flagged. No functional gap — those are
   every all-target action the game has.
2. **CRT compile-failure detection uses the missing `crtResolution` uniform**
   rather than an `IsShaderValid`-style check: raylib returns its *default*
   (passthrough) shader on a memory-compile failure, so `id > 0` cannot
   distinguish success from failure. A real compile yields the uniform (loc ≥ 0);
   the fallback lacks it. Either way the result is a graceful plain blit, and
   `UnloadShader` no-ops on the default shader id, so ownership is safe.
3. **A capture-only sequencer freeze** (`captureFreezeSeq_` + `captureAoeImpact`)
   was needed for `61_aoe_tint`, since the tint lives only on the impact beat and
   the 30-tick capture settle would otherwise run past it. The flag is declared
   unconditionally (a bare bool) but only set under `CRYSTAL_CAPTURE`.
4. **`motionPhase3()`** is new (the plan assumed a 3-step clock existed; only the
   2-step `motionPhase()` did).

### Files changed

- **Source:** `src/settings/Settings.hpp/.cpp` (2 optional bools),
  `src/core/Application.cpp` (focus audio + `setCrt`),
  `src/render/VirtualScreen.hpp/.cpp` + `src/render/RaylibRAII.hpp` (`ShaderHandle`
  + the CRT shader), `src/states/MainMenuState.hpp/.cpp` + **new**
  `src/states/TitlePhrases.hpp` (phrase), `src/ui/UiDraw.hpp/.cpp`
  (`motionPhase3`), `src/states/SettingsState.hpp/.cpp` (submenus),
  **new** `src/states/AoeTint.hpp` + `src/states/BattleState.hpp/.cpp` (tint +
  capture hook), `src/capture/CaptureRunner.cpp` (3 scenes).
- **Tests:** **new** `tests/test_presentation_options.cpp` (6 cases),
  `tests/test_settings.cpp` (the M51 round-trip), `tests/CMakeLists.txt`.
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md`,
  `docs/technical_design.md`, `docs/manual_test_matrix.md` (rows 128–132),
  `README.md`.

### Compatibility

No version bumps (`kSaveVersion` 1, `kSettingsVersion` 1, generation 10, rules
9). An old `settings.json` lacking `crtEffect`/`backgroundAudio` loads with both
false (test). Nothing about battle, generation, scoring, or determinism changed.

### Known limitations

- **The CRT effect is not in the capture set** — it lives at the window blit and
  capture exports the virtual target. Its look is an owner feel-check (matrix
  row 130). It has been verified only to compile and toggle without crashing in
  this session's builds, not visually.
- **Focus-audio and the shader's on-screen result are runtime behaviours** the
  headless suite can't assert; rows 130–131 cover them.
- The AoE tint colour for an all-allies **buff** (not a heal) is still the green
  "success" tint — a party-wide brace reads as a friendly pulse, which is the
  intent, but it does not distinguish buff from heal. Deliberate simplification.
