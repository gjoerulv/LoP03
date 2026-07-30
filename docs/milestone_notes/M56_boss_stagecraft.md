# M56 — Boss stagecraft (battle backdrops + Crystal Shatter)

> Program: **Adjustment program (M53–M56)**, authorized 2026-07-24. Final
> milestone; **pure presentation** — no battle-rule, scoring, generation, or save
> change, and no version bump.
>
> Authority: `CLAUDE.md` > this note > `docs/milestones.md`. §J is the
> as-implemented record.

## Goal

1. **B1 — per-theme battle backdrops.** A subdued theme dressing (Keep / Mine /
   Forest / Castle, plus a Plain fallback) behind the M46 battle grounding, so a
   fight reads its place at a glance without competing with the combatants.
2. **B2 — the Crystal Shatter boss intro.** A dramatic, always-skippable
   transition played for **every** battle against a boss team (dungeon bosses,
   Boss Rush waves, and the King).

Owner decision (2026-07-24): Crystal Shatter for every boss-team battle,
skippable with Confirm.

## Hazards respected

- **Hazard #1 (`StateStack::replaceState`).** The boss intro is a state **pushed
  on top of** the caller, never a `replaceState` (whose queued Pop fires a
  spurious `onResume` on the state below). Flow: caller → `BossIntroState` →
  (on end/skip) pushes `BattleState` on top of itself → on `onResume` (battle
  popped) pops itself → caller's `onResume` runs once with the final result.
  Belt-and-braces: `DungeonState::onResume` is guarded against an `Ongoing`
  result.
- **Determinism.** The intro and backdrops never touch `Battle.rngSeed` /
  `rollCursor`; shard/glint layout is a pure hash of a presentation seed (the
  dungeon seed), read-only.
- **Photosensitivity (M51 AoE-tint contract).** The single Peak pulse is a
  **dim** overlay with alpha capped at 0.12 and gated by `effectFlash`
  (Off→0 / Reduced→½ / Full), a single decay (never a strobe, nothing
  alternates). The darkening (cracks → black) is a fade, uncapped. Shake honors
  the `effectShake` setting.

## B1 — Battle backdrops

- New `src/render/BattleBackdrop.hpp/.cpp`: `enum class BattleStage { Plain,
  Keep, Mine, Forest, Castle }`; pure `stageForTheme(themeId)` (unknown → Plain);
  pure `buildBackdrop(stage, band, phase, accents) → vector<BackdropRect>` with a
  role enum (Ink / BorderDark / AccentCrystal / AccentMagic / AccentGold); a thin
  raylib `drawBattleBackdrop` mapping roles to `palette()` and `DrawRectangle`.
- Plumbing: `BattleState` ctor gains a trailing defaulted
  `render::BattleStage stage = Plain`; `DungeonState::startBattle` passes
  `stageForTheme(dungeon_.themeId)`; `CastleChallengeState` passes `Castle`. Drawn
  in `render()` **between the flat band fill and the ink keylines/brackets/pips**
  ([BattleState.cpp:1266-1293](../../src/states/BattleState.cpp)), so the M46
  grounding stays crisp on top; built once per frame from a pure list, no heap
  churn.
- **Subdued rules (mechanically tested):** ≤ ~25 % of the band covered;
  silhouettes only in the bottom ~40 % + an 8px top skyline strip (the mid-band
  corridor for floats/status stays flat); bodies in BorderDark with Ink accents;
  ≤ 1 accent role per stage; static except one 2-frame glint. **High contrast:
  `accents=false` drops the accent layer, silhouettes stay.**
- Sketches (original): *Keep* — broken parapet merlons along the top strip +
  tumbled-block piles at the floor; *Mine* — angular crystal clusters from the
  floor + one cyan facet glint; *Forest* — rough trunk columns with ink root
  flares + a sparse stepped canopy strip; *Castle* — twin notched banners flanking
  a stepped throne-arch outline + one gold pip per banner + a single gold keyline.

## B2 — Crystal Shatter boss intro

- New `src/states/BossIntroState.hpp/.cpp`, pushed on top of the caller carrying
  the full BattleState launch payload (built `Battle`, result/stats slots, music,
  castle flag, stage, presentation seed). `rendersBelow()=true` (the frozen scene
  shows), `updatesBelow()=false` (the dungeon freezes for free). Routed for every
  boss-team battle: `DungeonState::startBattle` (team `bossId` non-empty) and
  `CastleChallengeState::startNextFight` (Boss Rush + King; Endless waves have no
  `bossId` and stay plain).
- New pure header `src/states/BossIntroTimeline.hpp` (dt-injected,
  FadeController-shaped): **Hold 0.10 → Build 0.45 → Peak 0.15 → Handoff 0.20**;
  `skip()` jumps to Handoff. Build grows ink+crystal cracks from centre; Peak is
  the single dim pulse + shake and completes the darkening; Handoff flings 12–16
  seeded crystal shards outward over black. On `done()` or skip, `BossIntroState`
  pushes `BattleState` and calls `fade.start()` so the battle reveals from black.

## ⚠ Deviation from the plan (documented, routine)

**No live-scene snapshot service.** The plan proposed a RAII `SnapshotHolder` on
AppContext + an Application hook that reads the virtual render target
(`LoadImageFromTexture` → `LoadTextureFromImage`) mid-frame so the Handoff could
fling pieces of the *actual* frozen scene. Reading the active framebuffer during
`BeginTextureMode`, the y-flip, and the texture-lifetime management are fragile
and platform-dependent for what is a pure cosmetic flourish, and
`BattleState::onEnter` does **not** in fact call `fade.start()` (the plan assumed
it did). Instead the shatter is **abstract**: seeded crystal shards flung over a
darkening-to-black field, and `BossIntroState` starts the fade itself on handoff.
The owner-decided outcome — a Crystal Shatter transition for every boss-team
battle, skippable — is delivered; the effect is simpler, safe, and adds no
framebuffer read. Reversible if the owner wants the literal scene-shard version
later. No owner sign-off needed (a presentation implementation choice; no rule,
schema, or player-facing-contract change).

## Tests & capture

- `tests/test_battle_backdrop.cpp`: stage mapping incl. unknown; per
  stage×phase×accents — rects in-band, allowed roles only, `accents=false` ⇒ zero
  accent roles, coverage ≤ 25 %, silhouette placement, determinism.
- `tests/test_boss_intro.cpp`: phase durations/order, monotonic progress, skip
  semantics, single-peak pulse scaling Off/Reduced/Full, shard-layout bounds +
  determinism.
- Capture: four backdrop scenes (one per stage; Mine also in the high-contrast
  pass to prove accent suppression), boss-intro scenes via a capture time-freeze
  hook (mid-Build cracks + Peak with the longest telegraph).

## Documents

`docs/game_design.md` (backdrops + Crystal Shatter player description),
`docs/technical_design.md` (BattleStage, BossIntroState/Timeline, the snapshot
deviation), `docs/milestones.md`.

## Acceptance criteria

All four backdrops subdued per the builder rules and readable in high contrast;
Crystal Shatter plays for every boss-team battle, skippable,
photosensitivity-compliant, leaves `rollCursor` untouched; full suite green from
515 (Debug) / 511 (Release), capture clean with the new scenes.

## J. As-implemented record

Implemented 2026-07-24 on base checkout `07a13bb` + M53/M54/M55. **Pure
presentation, no version bump** (rules 10 / generation 11 / save 1 / settings 1 /
achievements 1, all carried from M55).

**As built.**
- B1: `render/BattleBackdrop.hpp/.cpp` — pure `stageForTheme` + `buildBackdrop`
  (role enum, corridor-clear, ≤ 25 % coverage, accents-off drops accents) and a
  thin raylib `drawBattleBackdrop`. `BattleState` gains the trailing
  `render::BackdropStage stage = Plain`; drawn between the band fill and the ink
  keylines. `DungeonState::startBattle` passes `stageForTheme`,
  `CastleChallengeState` passes `Castle`.
- B2: `states/BossIntroState.*` pushed on top of the caller (never replaceState),
  `rendersBelow` / no `updatesBelow`, carrying the BattleState launch payload;
  pure `states/BossIntroTimeline.hpp` (Hold/Build/Peak/Handoff, `skip()` →
  Handoff, `buildIntroShards`, `bossIntroPulseAlpha`). Routed for every boss-team
  battle (dungeon boss, Boss Rush wave, King); Endless stays plain.
  `Dungeon`/`Castle` `onResume` guarded against `Ongoing`. The intro calls
  `fade.start()` on handoff.

**Deviation (documented, routine — see the ⚠ section above).** No live-scene
snapshot service; the Crystal Shatter is an **abstract** crystal-shard shatter
over a darkening field. Reading the active virtual target mid-draw is fragile for
a cosmetic flourish, and `BattleState::onEnter` does not in fact fade on its own
(the plan assumed it did, so the intro starts the fade). The owner-decided outcome
— a skippable Crystal Shatter for every boss-team battle — is delivered; the
approach is reversible.

**Fix during implementation (routine, local).** My initial `render::BattleStage`
enum collided with the BattleSequencer's existing `render::BattleStage`
animation-phase enum; renamed mine to `render::BackdropStage` throughout. No
behaviour change.

**Validation (this session).**
- Configure/build debug + release — OK, no warnings.
- Debug tests: `ctest --preset debug` → **526/526 passed** (515 + 11 new:
  5 `test_battle_backdrop` + 6 `test_boss_intro`). `[backdrop],[boss-intro]`
  alone: 11 cases, 2205 assertions.
- Release tests: `ctest --preset release` → **522/522 passed**.
- Capture: `CrystalDungeons.exe --capture` → **75/75 scenes clean** (68 + seven:
  four backdrop stages, the Mine high-contrast pass, and two frozen boss-intro
  beats — mid-Build and Peak — with the King's longest telegraph).
- No compiler warnings (a C4456 shadow warning in the shard loop was fixed by
  renaming the local; behaviour-identical, so the test results above stand).
